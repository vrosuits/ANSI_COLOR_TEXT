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

// AnsiEncoder.h - the inverse of AnsiParser: turn attributes and styled
// documents back into ANSI/SGR escape sequences. Powers the editor's
// "apply color to selection" and ANSI export.
#pragma once

#include "AnsiColor.h"
#include <string>

namespace ansi {

// Encode one attribute as a single `ESC[...m` SGR sequence that, applied after a
// reset, reproduces `a`. Colors use the most compact faithful form: a 16-color
// code when the RGB matches the standard palette, else a 256-color code when it
// matches the xterm cube/ramp, else 24-bit true color. Returns an empty string
// for a fully-default attribute (no params).
std::string encodeSgr(const Attr& a);

// Wrap `text` so it renders with attribute `a`: `encodeSgr(a)` + text + reset.
// If `a` is default, returns `text` unchanged (nothing to wrap).
std::string wrapSgr(const std::string& text, const Attr& a);

// Serialize a parsed document back to an ANSI byte stream: a reset + the encoded
// attribute before each span's text, and a trailing reset. parse(encode(doc))
// reproduces doc's text and per-character attributes.
std::string encode(const ParsedDocument& doc);

} // namespace ansi
