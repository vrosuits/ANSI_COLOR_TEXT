---
name: build-test
description: Build this Notepad++ ANSI plugin (core + DLL) and run the host self-test. Use when asked to build, compile, run tests, or verify a change compiles on Windows.
---

# Build & test the ANSI Color Text plugin

This repo builds with **MSVC via vcvars + Ninja**. A plain `cmake --build build`
fails with missing standard headers (`string`, `cstdint`) because MSVC needs the
vcvars environment — always go through `_build.bat`, which calls `vcvars64.bat`
first.

## Full build (core static lib + DLL)

```powershell
# Builds ansicore, the test exe, and ANSI_COLOR_TEXT.dll into .\build
.\_build.bat
```

Success prints `BUILD_DONE` and produces `build\ANSI_COLOR_TEXT.dll` (x64).

## Run the host self-test

The core (palette, both renderers, styler, encoder, AI JSON + client) is
host-testable without Notepad++:

```powershell
ctest --test-dir build --output-on-failure
```

## Lint (optional, fast feedback)

Formatting (`.clang-format` is tuned to the existing hand style; never bulk-reformat):

```powershell
# Check a file for formatting drift without changing it:
clang-format --dry-run --Werror src\<File>.cpp
# Apply formatting to a file you just edited:
clang-format -i src\<File>.cpp
```

Semantic checks (`.clang-tidy`). Core files lint standalone; the Win32 files
need a compile database:

```powershell
# Core file (no SDK needed):
clang-tidy src\AnsiScreen.cpp -- -std=c++17 -Isrc
# Whole tree: generate compile_commands.json via vcvars, then -p build:
#   (inside a vcvars shell) cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DNPP_SDK_INCLUDE=...
clang-tidy -p build src\AiHttp.cpp
```

## Notes

- The DLL links `winhttp` + `crypt32` (AI HTTP + DPAPI key crypto).
- CI mirrors this: `core-linux` builds/tests the core; `plugin-windows` builds
  the DLL with the VS 2022 generator. Keep both green.
