// Copyright 2026 Antony J Ingram, UNIVERSAL I.T SYSTEMS
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
