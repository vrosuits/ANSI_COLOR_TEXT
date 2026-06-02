// AnsiSgr.h - shared parsing of SGR (Select Graphic Rendition) parameters.
// Used by both the linear parser (AnsiParser) and the virtual-screen renderer
// (AnsiScreen) so the color/attribute logic lives in exactly one place.
#pragma once

#include "AnsiColor.h"
#include <string>
#include <vector>

namespace ansi {

// Split the numeric parameters of a CSI sequence body (the chars between ESC[
// and the final byte), e.g. "1;38;5;202" -> {1,38,5,202}. Always returns at
// least one element; an empty parameter becomes 0 (so "" -> {0}).
std::vector<int> sgrParams(const std::string& body);

// Apply a parsed SGR parameter list to the running attribute state.
void applySgr(const std::vector<int>& params, Attr& attr);

} // namespace ansi
