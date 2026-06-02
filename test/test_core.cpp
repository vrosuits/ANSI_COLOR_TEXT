// test_core.cpp - host self-test for the ANSI parsing/styling core.
// No Notepad++ or Scintilla needed; build via the ANSI_BUILD_TESTS CMake option.
#include "AnsiParser.h"
#include "AnsiPalette.h"
#include "AnsiStyler.h"

#include <cstdio>
#include <vector>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

using namespace ansi;

// A mock editor that records what the styler did.
struct MockEditor : IEditor {
    std::string text;
    int defineCount = 0;
    std::vector<int> styleNumbers; // per styleRange call
    int strikeCount = 0;

    void setText(const std::string& t) override { text = t; }
    void defineStyle(int, const Color&, const Color&, uint32_t) override { ++defineCount; }
    void styleRange(size_t, size_t, int style) override { styleNumbers.push_back(style); }
    void strikeRange(size_t, size_t) override { ++strikeCount; }
};

static void testPalette() {
    // 16-color basics.
    CHECK(palette16(0) == Color(0, 0, 0));
    CHECK(palette16(1).r == 205 && palette16(1).g == 0 && palette16(1).b == 0);
    CHECK(!palette16(99).set);

    // 256-color cube: index 16 is the cube origin (black), 231 is the last (white-ish).
    CHECK(palette256(16) == Color(0, 0, 0));
    CHECK(palette256(231) == Color(255, 255, 255));
    // Grayscale ramp start.
    CHECK(palette256(232) == Color(8, 8, 8));
    CHECK(palette256(255) == Color(238, 238, 238));
}

static void testParseBasic() {
    // Red "Hi" then reset, plus trailing plain text.
    ParsedDocument d = parse("\x1b[31mHi\x1b[0m!");
    CHECK(d.text == "Hi!");
    // Spans: "Hi" (red fore) and "!" (default).
    CHECK(d.spans.size() == 2);
    CHECK(d.spans[0].attr.fore == Color(205, 0, 0));
    CHECK(d.spans[1].attr.fore.set == false);
}

static void testParseExtendedColor() {
    // 256-color fg 202 and true-color bg.
    ParsedDocument d = parse("\x1b[38;5;202mX\x1b[48;2;10;20;30mY");
    CHECK(d.text == "XY");
    CHECK(d.spans.size() == 2);
    CHECK(d.spans[0].attr.fore == palette256(202));
    CHECK(d.spans[1].attr.back == Color(10, 20, 30));
    // fg carries across the second escape (only bg changed).
    CHECK(d.spans[1].attr.fore == palette256(202));
}

static void testParseAttributes() {
    ParsedDocument d = parse("\x1b[1;4;9mZ");
    CHECK(d.spans.size() == 1);
    uint32_t f = d.spans[0].attr.flags;
    CHECK(f & AF_Bold);
    CHECK(f & AF_Underline);
    CHECK(f & AF_Strike);
}

static void testNonSgrDropped() {
    // A cursor-position CSI (ends in 'H') must be stripped, not shown.
    ParsedDocument d = parse("a\x1b[2;3Hb");
    CHECK(d.text == "ab");
}

static void testStylerDedup() {
    // Two red runs separated by a default run -> red defined once (2 styles total).
    ParsedDocument d = parse("\x1b[31mA\x1b[0mB\x1b[31mC");
    MockEditor ed;
    StylerConfig cfg;
    StyleResult r = applyToEditor(d, ed, cfg);
    CHECK(ed.text == "ABC");
    CHECK(r.stylesUsed == 2);        // red + default
    CHECK(ed.defineCount == 2);      // each unique style defined once
    CHECK(ed.styleNumbers.size() == 3); // three runs all styled
    CHECK(!r.budgetExceeded);
}

static void testStylerStrike() {
    ParsedDocument d = parse("\x1b[9mX");
    MockEditor ed;
    StylerConfig cfg;
    applyToEditor(d, ed, cfg);
    CHECK(ed.strikeCount == 1);
}

int main() {
    testPalette();
    testParseBasic();
    testParseExtendedColor();
    testParseAttributes();
    testNonSgrDropped();
    testStylerDedup();
    testStylerStrike();

    if (g_failures == 0) {
        std::printf("OK: all core tests passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
