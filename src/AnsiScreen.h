// AnsiScreen.h - terminal-accurate ANSI rendering via a virtual screen.
//
// Unlike the linear AnsiParser (which streams text and only approximates
// positioning), this models a 2D character grid with a cursor and replays the
// stream the way a terminal would: absolute/relative cursor moves and erase
// sequences mutate cells in place. The grid is then flattened into the same
// ParsedDocument (clean text + styled spans) the styler consumes.
//
// Use this for real ANSI art; use AnsiParser for purely linear streams where a
// grid model is unnecessary.
#pragma once

#include "AnsiColor.h"
#include <string>

namespace ansi {

// Render `input` through a virtual screen into clean text + styled spans.
//
// Control handling:
//   - LF (\n) starts a new line (down + column 0); CR (\r) returns to column 0.
//   - SGR (ESC[...m) updates the current attribute.
//   - Cursor: CUU/CUD/CUF/CUB (A/B/C/D), CNL/CPL (E/F), CHA (G), VPA (d),
//     CUP/HVP (H/f, 1-based row;col), and save/restore (s/u).
//   - Erase: ED (J: 0=to end, 1=to start, 2=all) and EL (K: 0=to EOL,
//     1=to BOL, 2=whole line).
//   - Other CSI sequences are ignored.
//
// The grid grows on demand. `maxWidth` caps the column at which writing wraps to
// the next line (0 = no wrap; rely on explicit newlines/positioning).
ParsedDocument renderScreen(const std::string& input, size_t maxWidth = 0);

} // namespace ansi
