# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project goal

Make Notepad++ display ANSI-colored text. Requirements (from `README.md`):

- ANSI 16-color, 256-color, and ideally 24-bit true color.
- SGR text attributes: bold, italic, underline, strikethrough, blink ("flashing"), inverse.
- Selectable foreground **and** background color; explicit black-or-white background choice.
- Load real ANSI text/art files (including ANSI graphics and Unicode text/graphics) and render them in color.

## Architecture

Decided approach: a **full Notepad++ C++ plugin** (Win32 DLL on Scintilla), not a UDL/Lexer.

Code is split so the interesting logic is host-testable without Notepad++:

- `src/AnsiColor.h` — shared types (`Color`, `Attr` flags, `Span`, `ParsedDocument`). No Win32/Scintilla.
- `src/AnsiPalette.*` — 16 / 256 / true-color index → RGB.
- `src/AnsiSgr.*` — `sgrParams()` + `applySgr()`: the single shared implementation of SGR colour/attribute parsing, used by both renderers below.
- `src/AnsiScreen.*` — `renderScreen(input, ScreenConfig)`: **the renderer the plugin uses.** Replays the stream through a virtual 2D character grid with a cursor: absolute positioning (`H`/`f`), all cursor moves (`A/B/C/D/E/F/G/d/a/e`), tab stops (`HT`, `HTS`, `TBC`, `CHT`, `CBT`), scroll regions (`DECSTBM`, `SU`/`SD`, `IND`/`RI`/`NEL`), and erases (`ED`/`EL`/`ECH`), then flattens the grid into a `ParsedDocument`. `ScreenConfig` exposes wrap width, screen height (0 = unbounded, no scrolling), tab width, and erase-uses-background — defaults reproduce standard behavior.
- `src/AnsiParser.*` — `parse()`: the simpler *linear* renderer (streams text, approximates `ESC[nC` as spaces, drops other CSI). Kept and tested for purely linear streams; the plugin uses `renderScreen` instead.
- `src/AnsiStyler.*` — `applyToEditor()`: dedups attrs into Scintilla style slots, resolves inverse via fg/bg swap, marks strike via an indicator, reports `blinkRanges`. Talks to an abstract `IEditor` (no SDK dependency).
- `src/Settings.*` — `PluginSettings` + `.ini` load/save (in the Notepad++ plugin config dir); `toScreenConfig()` maps it to `ScreenConfig`.
- `src/PluginDefinition.*` — menu commands, `ScintillaEditor` (`IEditor` impl), blink + animation timers.
- `src/SettingsDialog.*` + `src/resource.h` + `src/SettingsDialog.rc` — the Win32 settings dialog.
- `src/DllMain.cpp` — the required Notepad++ plugin exports.
- `test/test_core.cpp` — host self-test (palette, both renderers, styler).

Plugin menu commands: Render ANSI Colors, Play as Animation, Stop Animation, Toggle Black/White Background, Pause/Resume Blink, Settings…, About. Animation = re-render successive byte-prefixes on a timer (configurable delay + bytes/frame), so cursor-positioned art animates. Frame cuts use `ansi::nextFrameBoundary()`, which snaps each prefix to the end of a printable char so a frame never ends mid-escape (flicker-free playback).

## Build & test

Core + host self-test (any C++17 compiler):
```
cmake -B build && cmake --build build && ctest --test-dir build
```
Quick test compile: `clang++ -std=c++17 -Isrc src/AnsiPalette.cpp src/AnsiSgr.cpp src/AnsiParser.cpp src/AnsiScreen.cpp src/AnsiStyler.cpp test/test_core.cpp -o t.exe`. (Local **mingw g++ is broken — missing cc1plus; use clang++ or MSVC**.)

The **DLL builds on Windows with MSVC** (verified) and needs the Npp SDK headers. CMake fetches the plugin template automatically, or pass `-DNPP_SDK_INCLUDE=<path>`. The VS *generator* can't see the installed BuildTools/Insiders here, so build via vcvars + Ninja:
```
call "...\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DNPP_SDK_INCLUDE="%CD%\_deps\plugintemplate\src"
cmake --build build      # -> build\ANSI_COLOR_TEXT.dll (x64)
```
SDK-version quirks already handled: the menu-callback type is `PFUNCPLUGINCMD` (not `PFUNCPLUGIN`), and modern Scintilla has no `SCI_SETLEXER` — use `SCI_SETILEXER, 0, 0` to disable lexing.

## Gotchas

- Scintilla styles have **no strikethrough / blink** attribute. Strike is drawn with an indicator (`INDIC_STRIKE`). Blink is simulated: the styler allocates a paired hidden style (fg = bg) per blink run and reports `blinkRanges`; the plugin runs a `SetTimer` loop (`blinkTimerProc`) that toggles ranges between the visible and hidden styles. "Toggle Blink" pauses/resumes it.
- `STYLE_MAX` is 255 — true-color-heavy art can exhaust style slots; the styler reports `budgetExceeded` and falls back rather than failing.
- CP437 ANSI art needs a matching font (IBM VGA / "Terminal") to show box-drawing glyphs; the buffer is treated as UTF-8 bytes.
- The render command **rewrites the Scintilla buffer** with escape-stripped text (the on-disk file is untouched unless saved).
