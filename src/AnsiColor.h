// AnsiColor.h - shared data types for the ANSI color rendering core.
//
// This header is platform-independent (no Windows / Scintilla headers) so the
// parser and palette can be compiled and unit-tested on any host. See
// AnsiParser.h for the entry point and AnsiStyler.h for the Scintilla binding.
#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace ansi {

// A resolved 24-bit color. `set == false` means "use the editor default"
// (i.e. no SGR color was specified for this channel).
struct Color {
    bool    set = false;
    uint8_t r = 0, g = 0, b = 0;

    constexpr Color() = default;
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_) : set(true), r(r_), g(g_), b(b_) {}

    bool operator==(const Color& o) const {
        return set == o.set && r == o.r && g == o.g && b == o.b;
    }
    bool operator!=(const Color& o) const { return !(*this == o); }
};

// SGR text-attribute flags (a bitmask). Blink and conceal are recorded even
// though Scintilla styles cannot render them natively (see AnsiStyler).
enum AttrFlag : uint32_t {
    AF_None      = 0,
    AF_Bold      = 1u << 0,
    AF_Faint     = 1u << 1,
    AF_Italic    = 1u << 2,
    AF_Underline = 1u << 3,
    AF_Blink     = 1u << 4,
    AF_Inverse   = 1u << 5,
    AF_Conceal   = 1u << 6,
    AF_Strike    = 1u << 7,
};

// The fully-resolved appearance of a run of text.
struct Attr {
    Color    fore;            // foreground color (unset -> editor default)
    Color    back;            // background color (unset -> editor default)
    uint32_t flags = AF_None; // OR of AttrFlag

    bool operator==(const Attr& o) const {
        return fore == o.fore && back == o.back && flags == o.flags;
    }
    bool operator!=(const Attr& o) const { return !(*this == o); }
};

// A contiguous run of *clean* (escape-stripped) text that shares one Attr.
// `start`/`length` index into ParsedDocument::text (byte offsets).
struct Span {
    size_t start  = 0;
    size_t length = 0;
    Attr   attr;
};

// Result of parsing an ANSI byte stream: the displayable text with all escape
// sequences removed, plus the styled spans covering it. The spans are
// contiguous and cover [0, text.size()) with no gaps or overlaps.
struct ParsedDocument {
    std::string       text;
    std::vector<Span> spans;
};

} // namespace ansi
