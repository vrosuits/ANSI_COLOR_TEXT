#include "AnsiParser.h"
#include "AnsiSgr.h"

#include <vector>

namespace ansi {
namespace {

constexpr char ESC = '\x1b';

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
                    applySgr(sgrParams(body), cur);
                } else if (final == 'C') {
                    // CUF (cursor forward): real ANSI art uses this to position
                    // graphics rightward. Render it as that many spaces under the
                    // current attribute (default count 1). Approximates positioning
                    // without a full virtual screen.
                    int cuf = sgrParams(body)[0];
                    if (cuf <= 0) cuf = 1;
                    doc.text.append(static_cast<size_t>(cuf), ' ');
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
