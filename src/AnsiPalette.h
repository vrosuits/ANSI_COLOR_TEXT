// AnsiPalette.h - maps ANSI color indices to 24-bit RGB.
#pragma once

#include "AnsiColor.h"
#include <cstdint>

namespace ansi {

// Returns the RGB for a standard 16-color index (0-15: 0-7 normal, 8-15 bright).
Color palette16(int index);

// Returns the RGB for an xterm 256-color index (0-255):
//   0-15   the standard 16 colors
//   16-231 a 6x6x6 color cube
//   232-255 a 24-step grayscale ramp
Color palette256(int index);

} // namespace ansi
