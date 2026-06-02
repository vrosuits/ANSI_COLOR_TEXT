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
