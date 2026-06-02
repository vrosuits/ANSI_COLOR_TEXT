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
- `src/AnsiParser.*` — `parse()`: raw bytes with `ESC[...m` SGR → escape-free text + contiguous styled spans. `ESC[nC` (cursor-forward) becomes n spaces (approximates ANSI-art positioning); other non-SGR CSI is dropped. There is no virtual-screen model yet, so absolute cursor positioning (`H`) and erase sequences aren't honored.
- `src/AnsiStyler.*` — `applyToEditor()`: dedups attrs into Scintilla style slots, resolves inverse via fg/bg swap, marks strike via an indicator. Talks to an abstract `IEditor` (no SDK dependency).
- `src/PluginDefinition.*` — menu commands + `ScintillaEditor` (the `IEditor` impl that sends `SCI_*` messages).
- `src/DllMain.cpp` — the required Notepad++ plugin exports.
- `test/test_core.cpp` — host self-test for parser/palette/styler.

## Build & test

```
cmake -B build
cmake --build build
ctest --test-dir build          # runs the core self-test
```

- The portable core + tests build with any C++17 compiler. To compile just the test quickly: `clang++ -std=c++17 -Isrc src/Ansi*.cpp test/test_core.cpp -o t.exe` (the local **g++/mingw is broken — missing cc1plus; use clang++ or MSVC**).
- The **DLL target builds on Windows only** and needs the Notepad++ plugin SDK headers (`PluginInterface.h`, `Scintilla.h`, `Notepad_plus_msgs.h`). CMake fetches them from the plugin template repo automatically, or pass `-DNPP_SDK_INCLUDE=<path>`.

## Gotchas

- Scintilla styles have **no strikethrough / blink** attribute. Strike is drawn with an indicator (`INDIC_STRIKE`). Blink is simulated: the styler allocates a paired hidden style (fg = bg) per blink run and reports `blinkRanges`; the plugin runs a `SetTimer` loop (`blinkTimerProc`) that toggles ranges between the visible and hidden styles. "Toggle Blink" pauses/resumes it.
- `STYLE_MAX` is 255 — true-color-heavy art can exhaust style slots; the styler reports `budgetExceeded` and falls back rather than failing.
- CP437 ANSI art needs a matching font (IBM VGA / "Terminal") to show box-drawing glyphs; the buffer is treated as UTF-8 bytes.
- The render command **rewrites the Scintilla buffer** with escape-stripped text (the on-disk file is untouched unless saved).
