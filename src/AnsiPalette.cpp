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

#include "AnsiPalette.h"

namespace ansi {

// The classic 16-color ANSI palette (xterm-ish values). Indices 0-7 are the
// normal colors; 8-15 are their bright variants.
static const Color kBase16[16] = {
    {  0,   0,   0}, // 0  black
    {205,   0,   0}, // 1  red
    {  0, 205,   0}, // 2  green
    {205, 205,   0}, // 3  yellow
    {  0,   0, 238}, // 4  blue
    {205,   0, 205}, // 5  magenta
    {  0, 205, 205}, // 6  cyan
    {229, 229, 229}, // 7  white (light gray)
    {127, 127, 127}, // 8  bright black (dark gray)
    {255,   0,   0}, // 9  bright red
    {  0, 255,   0}, // 10 bright green
    {255, 255,   0}, // 11 bright yellow
    { 92,  92, 255}, // 12 bright blue
    {255,   0, 255}, // 13 bright magenta
    {  0, 255, 255}, // 14 bright cyan
    {255, 255, 255}, // 15 bright white
};

Color palette16(int index) {
    if (index < 0 || index > 15)
        return Color{}; // unset -> caller uses editor default
    return kBase16[index];
}

Color palette256(int index) {
    if (index < 0 || index > 255)
        return Color{};

    if (index < 16)
        return kBase16[index];

    if (index < 232) {
        // 6x6x6 color cube. Each axis uses the xterm step table {0,95,135,175,215,255}.
        static const uint8_t kStep[6] = {0, 95, 135, 175, 215, 255};
        int i = index - 16;
        int r = (i / 36) % 6;
        int g = (i / 6)  % 6;
        int b =  i        % 6;
        return Color{kStep[r], kStep[g], kStep[b]};
    }

    // Grayscale ramp: 24 steps from 8 to 238 in increments of 10.
    uint8_t v = static_cast<uint8_t>(8 + (index - 232) * 10);
    return Color{v, v, v};
}

} // namespace ansi
