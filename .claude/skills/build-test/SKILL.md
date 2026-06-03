---
name: build-test
description: Build this Notepad++ ANSI Color Text plugin and run its tests. Use whenever asked to build, compile, rebuild, run tests, run the self-test, check that the code still compiles, or verify a change before committing — covers both the fast host-only core build and the full Windows DLL build. Reach for this instead of guessing cmake invocations, because a plain `cmake --build` fails here without the vcvars environment.
---

# Build & test the ANSI Color Text plugin

This project has two build surfaces, and picking the right one is the whole game
for a fast feedback loop:

- **Core logic** (`src/Ansi*.cpp`, `src/AiJson.cpp`, `src/AiClient.cpp`) is
  platform-independent and host-testable — it builds with any C++17 compiler and
  needs no Notepad++ SDK or MSVC environment.
- **The plugin layer** (`PluginDefinition`, the dialogs, `AiHttp`, `Settings`,
  `DllMain`) is Win32/Scintilla and only builds into the DLL via MSVC.

So: if you only touched core logic, take the **quick path** — it's seconds, not a
minute, and skips the whole vcvars dance. Only do the full DLL build when you
changed the Win32 layer or actually need the `.dll`.

## Quick path — core build + host self-test (no vcvars)

The self-test covers the palette, both renderers, the styler, the encoder, and
the AI JSON + client. Build and run it directly with clang++:

```powershell
clang++ -std=c++17 -Isrc `
  src/AnsiPalette.cpp src/AnsiSgr.cpp src/AnsiParser.cpp src/AnsiScreen.cpp `
  src/AnsiStyler.cpp src/AnsiEncoder.cpp src/AiJson.cpp src/AiClient.cpp `
  test/test_core.cpp -o t.exe
.\t.exe
```

Success prints `OK: all core tests passed`. Keep the source list in sync with the
`ansicore` target in `CMakeLists.txt` — if a new non-Win32 `.cpp` is added there,
add it here too, or the link step fails on missing symbols.

(The same thing via CMake also works on Linux/CI: `cmake -B build && cmake --build
build && ctest --test-dir build` — but on this Windows box CMake drives MSVC,
which brings in the vcvars requirement below, so clang++ is the faster local loop.)

## Full path — DLL build + tests (MSVC via vcvars + Ninja)

The DLL must be built with MSVC, and **MSVC needs the vcvars environment** — a
plain `cmake --build build` fails with `Cannot open include file: 'string'`
because the compiler can't find the standard library. `_build.bat` calls
`vcvars64.bat` first, so always go through it rather than invoking cmake directly:

```powershell
# Builds ansicore, the test exe, and ANSI_COLOR_TEXT.dll (x64) into .\build
.\_build.bat
ctest --test-dir build --output-on-failure
```

`_build.bat` prints `BUILD_DONE` on success and produces
`build\ANSI_COLOR_TEXT.dll`. The DLL links `winhttp` + `crypt32` (AI HTTP + DPAPI
key crypto); if those fail to resolve, the link libraries in `CMakeLists.txt` are
the place to look.

## Lint (optional, fast feedback on edits)

Formatting is governed by `.clang-format`, which is deliberately tuned to the
**existing hand-aligned style** (`ColumnLimit: 0`, so it never rewraps the
author's lines). Format files you edit; do **not** bulk-reformat the tree.

```powershell
clang-format --dry-run --Werror src\<File>.cpp   # check for drift
clang-format -i src\<File>.cpp                    # apply to a file you edited
```

Semantic checks live in `.clang-tidy` (bugprone / performance / safe
modernizations). Core files lint standalone; the Win32 files need a compile
database:

```powershell
clang-tidy src\AnsiScreen.cpp -- -std=c++17 -Isrc       # core file, no SDK
# For the Win32 files, generate compile_commands.json inside a vcvars shell:
#   cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DNPP_SDK_INCLUDE=...
clang-tidy -p build src\AiHttp.cpp
```

## Before you call it done

- Changed core logic → quick path must print `OK: all core tests passed`.
- Changed the Win32 layer or the build → full `_build.bat` + `ctest` must pass.
- CI mirrors this split: `core-linux` builds/tests the core on Linux;
  `plugin-windows` builds the DLL with the VS 2022 generator. Keep both green.
