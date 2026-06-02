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

#include "AnsiSgr.h"
#include "AnsiPalette.h"

namespace ansi {

std::vector<int> sgrParams(const std::string& body) {
    std::vector<int> out;
    int cur = 0;
    bool any = false;
    for (char c : body) {
        if (c >= '0' && c <= '9') {
            cur = cur * 10 + (c - '0');
            any = true;
        } else if (c == ';') {
            out.push_back(any ? cur : 0);
            cur = 0;
            any = false;
        }
        // Any other byte (e.g. ':') is ignored for parameter splitting.
    }
    out.push_back(any ? cur : 0);
    return out;
}

void applySgr(const std::vector<int>& p, Attr& attr) {
    for (size_t i = 0; i < p.size(); ++i) {
        int code = p[i];
        switch (code) {
            case 0:  attr = Attr{}; break;                 // reset all
            case 1:  attr.flags |= AF_Bold;      break;
            case 2:  attr.flags |= AF_Faint;     break;
            case 3:  attr.flags |= AF_Italic;    break;
            case 4:  attr.flags |= AF_Underline; break;
            case 5:
            case 6:  attr.flags |= AF_Blink;     break;    // 6 = rapid blink
            case 7:  attr.flags |= AF_Inverse;   break;
            case 8:  attr.flags |= AF_Conceal;   break;
            case 9:  attr.flags |= AF_Strike;    break;
            case 21:
            case 22: attr.flags &= ~(AF_Bold | AF_Faint); break;
            case 23: attr.flags &= ~AF_Italic;    break;
            case 24: attr.flags &= ~AF_Underline; break;
            case 25: attr.flags &= ~AF_Blink;     break;
            case 27: attr.flags &= ~AF_Inverse;   break;
            case 28: attr.flags &= ~AF_Conceal;   break;
            case 29: attr.flags &= ~AF_Strike;    break;
            case 39: attr.fore = Color{}; break;           // default fg
            case 49: attr.back = Color{}; break;           // default bg
            case 38: // extended fg
            case 48: { // extended bg
                bool fg = (code == 38);
                if (i + 1 < p.size() && p[i + 1] == 5) {
                    // 38;5;n  -> 256-color
                    if (i + 2 < p.size()) {
                        Color c = palette256(p[i + 2]);
                        (fg ? attr.fore : attr.back) = c;
                    }
                    i += 2;
                } else if (i + 1 < p.size() && p[i + 1] == 2) {
                    // 38;2;r;g;b -> true color
                    if (i + 4 < p.size()) {
                        Color c{static_cast<uint8_t>(p[i + 2]),
                                static_cast<uint8_t>(p[i + 3]),
                                static_cast<uint8_t>(p[i + 4])};
                        (fg ? attr.fore : attr.back) = c;
                    }
                    i += 4;
                }
                break;
            }
            default:
                if (code >= 30 && code <= 37)        attr.fore = palette16(code - 30);
                else if (code >= 40 && code <= 47)   attr.back = palette16(code - 40);
                else if (code >= 90 && code <= 97)   attr.fore = palette16(code - 90 + 8);
                else if (code >= 100 && code <= 107) attr.back = palette16(code - 100 + 8);
                // unknown codes are ignored
                break;
        }
    }
}

} // namespace ansi
