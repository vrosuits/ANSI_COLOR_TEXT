#include "AnsiParser.h"
#include "AnsiPalette.h"

#include <vector>

namespace ansi {
namespace {

constexpr char ESC = '\x1b';

// Parse the integer parameters of an SGR sequence (the chars between ESC[ and
// the trailing 'm'), e.g. "1;38;5;202". An empty parameter is treated as 0,
// matching terminal convention (ESC[m == ESC[0m).
std::vector<int> splitParams(const std::string& body) {
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

// Apply one fully-parsed SGR parameter list to the running attribute state.
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

} // namespace

ParsedDocument parse(const std::string& input) {
    ParsedDocument doc;
    doc.text.reserve(input.size());

    Attr   cur;          // running attribute state
    size_t spanStart = 0;

    // Flush the text accumulated since the last attribute change into a span.
    auto flush = [&](const Attr& a) {
        size_t end = doc.text.size();
        if (end > spanStart)
            doc.spans.push_back(Span{spanStart, end - spanStart, a});
        spanStart = end;
    };

    const size_t n = input.size();
    for (size_t i = 0; i < n;) {
        char c = input[i];
        if (c == ESC && i + 1 < n && input[i + 1] == '[') {
            // CSI sequence: ESC [ <params/intermediates> <final byte 0x40-0x7E>
            size_t j = i + 2;
            while (j < n) {
                unsigned char fb = static_cast<unsigned char>(input[j]);
                if (fb >= 0x40 && fb <= 0x7E) break; // final byte
                ++j;
            }
            if (j < n) {
                char final = input[j];
                std::string body = input.substr(i + 2, j - (i + 2));
                if (final == 'm') {
                    // SGR: attribute change -> close the current span first.
                    flush(cur);
                    applySgr(splitParams(body), cur);
                } else if (final == 'C') {
                    // CUF (cursor forward): real ANSI art uses this to position
                    // graphics rightward. Render it as that many spaces under the
                    // current attribute (default count 1). Approximates positioning
                    // without a full virtual screen.
                    int n = splitParams(body)[0];
                    if (n <= 0) n = 1;
                    doc.text.append(static_cast<size_t>(n), ' ');
                }
                // Other CSI (cursor up/down/position, erase, etc.) are dropped.
                i = j + 1;
                continue;
            }
            // Unterminated escape at end of buffer: emit the rest as literal text.
        }
        doc.text.push_back(c);
        ++i;
    }

    flush(cur); // final trailing run
    return doc;
}

} // namespace ansi
