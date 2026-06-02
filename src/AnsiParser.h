// AnsiParser.h - turns a raw ANSI byte stream into clean text + styled spans.
#pragma once

#include "AnsiColor.h"
#include <string>

namespace ansi {

// Parse `input` (raw bytes, possibly containing CSI/SGR escape sequences) into
// a ParsedDocument: escape-free displayable text plus contiguous styled spans.
//
// Supported sequences:
//   - SGR (ESC[...m): attributes 0-9, 21-29, 30-37/90-97 (fg), 40-47/100-107 (bg),
//     38/48;5;n (256-color) and 38/48;2;r;g;b (true color), 39/49 (default fg/bg).
//   - CUF (ESC[nC, cursor-forward) is rendered as n spaces, which approximates
//     the rightward positioning common in ANSI art without a virtual screen.
//   - Other CSI sequences (ESC[ ... final-byte) are consumed and dropped so they
//     do not appear as literal text.
//
// Carriage returns / line feeds and all other bytes are passed through verbatim
// into the clean text. The current attribute state carries across lines until a
// reset (SGR 0) or an explicit change, matching terminal behavior.
ParsedDocument parse(const std::string& input);

} // namespace ansi
