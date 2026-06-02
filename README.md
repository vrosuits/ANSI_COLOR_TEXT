# ANSI Color Text — a Notepad++ plugin

Render ANSI / SGR-colored text and ANSI art inside Notepad++ — in 16-color,
256-color, and 24-bit true color, with bold, italic, underline, strikethrough,
inverse, and (simulated) blink. It includes a virtual-screen renderer that
honors cursor positioning, scroll regions, tab stops, and erase sequences, plus
timed **animation playback** for cursor-addressed ANSI animations.

## Features

- **Color:** ANSI 16-color, xterm 256-color, and 24-bit true color, for both
  foreground and background.
- **Attributes:** bold, italic, underline (native Scintilla); strikethrough (via
  an indicator); inverse (foreground/background swap); blink (timer-simulated).
- **Selectable background:** one-key toggle between a black and a white default
  background — ANSI art is usually authored for black.
- **Virtual-screen renderer:** replays the byte stream like a terminal —
  absolute positioning (`ESC[r;cH`), all relative cursor moves, tab stops
  (`HT`/`HTS`/`TBC`/`CHT`/`CBT`), scroll regions (`DECSTBM`, `SU`/`SD`,
  `IND`/`RI`/`NEL`), and erases (`ED`/`EL`/`ECH`).
- **Animation playback:** *Play as Animation* reveals the stream progressively on
  a timer, so cursor-positioned ANSI animations play back. Frame cuts snap to
  glyph boundaries, so playback is flicker-free.
- **Configurable:** a settings dialog (persisted to an `.ini`) exposes wrap
  width, screen height, tab width, erase behavior, scroll-region toggle, default
  background, blink interval, and animation speed.

## Menu commands

Under **Plugins → ANSI Color Text**:

| Command | Action |
| --- | --- |
| Render ANSI Colors | Parse and colorize the current document. |
| Play as Animation | Play the document as a timed ANSI animation. |
| Stop Animation | Stop playback and show the final frame. |
| Toggle Black/White Background | Switch the default background and re-render. |
| Pause/Resume Blink | Pause or resume blink animation. |
| Settings… | Open the configuration dialog. |
| About | Plugin info. |

## Installation

1. Build the DLL (see below) or obtain `ANSI_COLOR_TEXT.dll` (x64).
2. Copy it into your Notepad++ install at
   `plugins\ANSI_COLOR_TEXT\ANSI_COLOR_TEXT.dll`.
3. Restart Notepad++. The **ANSI Color Text** submenu appears under **Plugins**.

> ANSI art authored in code page 437 needs a CP437-capable font (e.g. an IBM VGA
> / "Terminal" font) to display box-drawing and block glyphs correctly. The
> document buffer is treated as UTF-8 bytes.

## Building

The codebase is split so the rendering/parsing core is platform-independent and
unit-tested on any host; only the thin plugin layer needs Windows and the
Notepad++ SDK.

### Core + tests (any platform, any C++17 compiler)

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

### The plugin DLL (Windows, MSVC)

The DLL needs the Notepad++ plugin SDK headers; CMake fetches them automatically,
or pass `-DNPP_SDK_INCLUDE=<path>`. Build through a Developer environment:

```bat
call "<VS>\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DNPP_SDK_INCLUDE="%CD%\_deps\plugintemplate\src"
cmake --build build
```

The output is `build\ANSI_COLOR_TEXT.dll`.

## Usage

Open a file containing ANSI escape sequences (try `samples/16color.ans`,
`samples/truecolor.ans`, or the animated `samples/animation.ans`) and run
**Render ANSI Colors** — or **Play as Animation** for the animated sample.

Rendering rewrites the in-editor buffer with escape-stripped, styled text; the
file on disk is untouched unless you save.

## Architecture

| Module | Responsibility |
| --- | --- |
| `src/AnsiColor.h` | Shared types (`Color`, `Attr`, `Span`, `ParsedDocument`). |
| `src/AnsiPalette.*` | 16 / 256 / true-color index → RGB. |
| `src/AnsiSgr.*` | Shared SGR (color/attribute) parsing. |
| `src/AnsiScreen.*` | Virtual-screen renderer (the one the plugin uses). |
| `src/AnsiParser.*` | Simpler linear renderer (for purely linear streams). |
| `src/AnsiStyler.*` | Maps parsed spans onto an abstract editor (Scintilla styles, indicators). |
| `src/Settings.*` | Settings struct + `.ini` persistence. |
| `src/PluginDefinition.*`, `src/DllMain.cpp` | Notepad++ plugin glue, exports, timers. |
| `src/SettingsDialog.*`, `src/resource.h`, `src/*.rc` | Win32 settings dialog. |
| `test/test_core.cpp` | Host self-test for the core. |

## License

Licensed under the Apache License, Version 2.0 — see [LICENSE](LICENSE) and
[NOTICE](NOTICE).

Copyright © 2026 Antony J Ingram, UNIVERSAL I.T SYSTEMS.
