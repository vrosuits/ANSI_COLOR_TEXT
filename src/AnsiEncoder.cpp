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

#include "AnsiEncoder.h"
#include "AnsiPalette.h"

#include <string>
#include <vector>

namespace ansi {
namespace {

// Index of `c` in the 16-color palette, or -1.
int match16(const Color& c) {
    for (int i = 0; i < 16; ++i)
        if (palette16(i) == c) return i;
    return -1;
}

// Index of `c` in the xterm 256-color table, or -1. (Skips 0-15, which match16
// already covers, so the 16-color form is preferred.)
int match256(const Color& c) {
    for (int i = 16; i < 256; ++i)
        if (palette256(i) == c) return i;
    return -1;
}

// Append the SGR parameters for one color (foreground or background) to `p`.
void encodeColor(const Color& c, bool fg, std::vector<int>& p) {
    int i16 = match16(c);
    if (i16 >= 0) {
        int base = (i16 < 8) ? (fg ? 30 : 40) + i16
                             : (fg ? 90 : 100) + (i16 - 8);
        p.push_back(base);
        return;
    }
    int i256 = match256(c);
    if (i256 >= 0) {
        p.push_back(fg ? 38 : 48);
        p.push_back(5);
        p.push_back(i256);
        return;
    }
    p.push_back(fg ? 38 : 48);
    p.push_back(2);
    p.push_back(c.r);
    p.push_back(c.g);
    p.push_back(c.b);
}

std::string paramsToSgr(const std::vector<int>& p) {
    if (p.empty()) return std::string();
    std::string s = "\x1b[";
    for (size_t i = 0; i < p.size(); ++i) {
        if (i) s += ';';
        s += std::to_string(p[i]);
    }
    s += 'm';
    return s;
}

} // namespace

std::string encodeSgr(const Attr& a) {
    std::vector<int> p;
    if (a.flags & AF_Bold)      p.push_back(1);
    if (a.flags & AF_Faint)     p.push_back(2);
    if (a.flags & AF_Italic)    p.push_back(3);
    if (a.flags & AF_Underline) p.push_back(4);
    if (a.flags & AF_Blink)     p.push_back(5);
    if (a.flags & AF_Inverse)   p.push_back(7);
    if (a.flags & AF_Conceal)   p.push_back(8);
    if (a.flags & AF_Strike)    p.push_back(9);
    if (a.fore.set) encodeColor(a.fore, true,  p);
    if (a.back.set) encodeColor(a.back, false, p);
    return paramsToSgr(p);
}

std::string wrapSgr(const std::string& text, const Attr& a) {
    std::string sgr = encodeSgr(a);
    if (sgr.empty()) return text;
    return sgr + text + "\x1b[0m";
}

std::string encode(const ParsedDocument& doc) {
    std::string out;
    bool wroteAny = false;
    for (const Span& span : doc.spans) {
        std::string sgr = encodeSgr(span.attr);
        // Reset before a styled span (so prior attributes never leak in), and
        // before a default span only if we previously set something.
        if (!sgr.empty()) {
            out += "\x1b[0m";
            out += sgr;
            wroteAny = true;
        } else if (wroteAny) {
            out += "\x1b[0m";
            wroteAny = false;
        }
        out.append(doc.text, span.start, span.length);
    }
    if (wroteAny) out += "\x1b[0m";
    return out;
}

std::string toSymbolicEscapes(const std::string& withEsc) {
    std::string out;
    out.reserve(withEsc.size() + 8);
    for (char c : withEsc) {
        if (c == '\x1b')    out += "\\e";
        else if (c == '\\') out += "\\\\";
        else                out += c;
    }
    return out;
}

std::string fromSymbolicEscapes(const std::string& s) {
    auto eqi = [](char a, char b) {  // ASCII case-insensitive compare
        return (a | 0x20) == (b | 0x20);
    };
    std::string out;
    out.reserve(s.size());
    const size_t n = s.size();
    for (size_t i = 0; i < n;) {
        char c = s[i];
        if (c == '\\' && i + 1 < n) {
            char d = s[i + 1];
            if (d == '\\')             { out += '\\';   i += 2; continue; }
            if (d == 'e' || d == 'E')  { out += '\x1b'; i += 2; continue; }
            if ((d == 'x' || d == 'X') && i + 3 < n &&
                s[i + 2] == '1' && (s[i + 3] == 'b' || s[i + 3] == 'B')) {
                out += '\x1b'; i += 4; continue;
            }
            if ((d == 'u' || d == 'U') && i + 5 < n &&
                s[i + 2] == '0' && s[i + 3] == '0' && s[i + 4] == '1' &&
                (s[i + 5] == 'b' || s[i + 5] == 'B')) {
                out += '\x1b'; i += 6; continue;
            }
            if (d == '0' && i + 3 < n && s[i + 2] == '3' && s[i + 3] == '3') {
                bool more = (i + 4 < n) && s[i + 4] >= '0' && s[i + 4] <= '7';
                if (!more) { out += '\x1b'; i += 4; continue; }
            }
            out += c; ++i; continue;  // unrecognized backslash sequence: keep literal
        }
        if (c == '^' && i + 1 < n && s[i + 1] == '[') { out += '\x1b'; i += 2; continue; }
        // Bare ESC/Esc/esc that introduces a CSI ('[') or OSC (']') sequence —
        // the form some AI models emit instead of a real control byte.
        if ((c == 'E' || c == 'e') && i + 3 < n &&
            eqi(s[i + 1], 's') && eqi(s[i + 2], 'c') &&
            (s[i + 3] == '[' || s[i + 3] == ']')) {
            out += '\x1b'; i += 3; continue;
        }
        out += c; ++i;
    }
    return out;
}

} // namespace ansi
