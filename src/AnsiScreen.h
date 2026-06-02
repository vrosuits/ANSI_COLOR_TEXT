// AnsiScreen.h - terminal-accurate ANSI rendering via a virtual screen.
//
// Unlike the linear AnsiParser (which streams text and only approximates
// positioning), this models a 2D character grid with a cursor and replays the
// stream the way a terminal would: cursor moves, scroll regions, tab stops and
// erase sequences mutate cells in place. The grid is then flattened into the
// same ParsedDocument (clean text + styled spans) the styler consumes.
//
// Use this for real ANSI art and animations; use AnsiParser for purely linear
// streams where a grid model is unnecessary.
#pragma once

#include "AnsiColor.h"
#include <cstddef>
#include <string>

namespace ansi {

// Tunable terminal behaviors. The defaults reproduce the renderer's standard
// behavior; every field can be overridden (the plugin exposes them in its
// settings dialog).
struct ScreenConfig {
    // Column at which writing wraps to the next line. 0 = never wrap (rely on
    // explicit newlines / positioning). 80 is the de-facto ANSI-art width.
    size_t maxWidth = 80;

    // Screen height for scroll-region semantics. 0 = unbounded: the grid grows
    // downward forever and line feeds never scroll (best for long static art).
    // When > 0, the screen is bounded and a line feed at the bottom margin
    // scrolls, which is what animations using DECSTBM expect.
    size_t height = 0;

    // Columns between default tab stops (used by HT / CHT / CBT).
    int tabWidth = 8;

    // When true, erase sequences paint cells with the current SGR background
    // (faithful terminal behavior). When false, erased cells become fully
    // blank/transparent (render as default-styled spaces).
    bool eraseUsesBackground = true;

    // Honor DECSTBM (set scroll region). Only meaningful when height > 0.
    bool scrollRegionEnabled = true;
};

// Render `input` through a virtual screen into clean text + styled spans.
//
// Control handling:
//   - LF (\n) line feed (scrolls within the region when bounded); CR (\r) to
//     column 0; HT (\t) advances to the next tab stop; BS (\b) moves left.
//   - SGR (ESC[...m) updates the current attribute.
//   - Cursor: CUU/CUD/CUF/CUB (A/B/C/D), CNL/CPL (E/F), CHA (G), VPA (d),
//     HPR/VPR (a/e), CUP/HVP (H/f), save/restore (s/u and ESC 7/8).
//   - Tabs: HTS (ESC H), TBC (g), CHT (I), CBT (Z).
//   - Scroll: DECSTBM (r), SU/SD (S/T), IND/RI/NEL (ESC D / ESC M / ESC E).
//   - Erase: ED (J), EL (K), ECH (X).
//   - Other CSI sequences are ignored.
ParsedDocument renderScreen(const std::string& input, const ScreenConfig& cfg);

// Convenience overload using default config.
inline ParsedDocument renderScreen(const std::string& input) {
    return renderScreen(input, ScreenConfig{});
}

// Advance an animation playback position by at least `minChunk` bytes, then
// snap forward to the end of the next printable character so a frame never ends
// inside (or immediately after) an escape sequence. Ending mid-control reveals a
// half-drawn frame (e.g. the previous glyph erased before the next is drawn);
// snapping to a glyph boundary keeps playback flicker-free. Returns a position
// in (pos, input.size()]; returns input.size() if no further glyph exists.
size_t nextFrameBoundary(const std::string& input, size_t pos, size_t minChunk);

} // namespace ansi
