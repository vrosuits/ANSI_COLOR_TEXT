// test_core.cpp - host self-test for the ANSI parsing/styling core.
// No Notepad++ or Scintilla needed; build via the ANSI_BUILD_TESTS CMake option.
#include "AnsiParser.h"
#include "AnsiScreen.h"
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

static void testCursorForward() {
    // CUF (ESC[4C) becomes 4 spaces; bare ESC[C defaults to 1.
    CHECK(parse("a\x1b[4Cb").text == "a    b");
    CHECK(parse("a\x1b[Cb").text == "a b");
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

static void testStylerBlink() {
    // Blink red text -> one blink range with distinct on/off styles.
    ParsedDocument d = parse("\x1b[5;31mBlink\x1b[0m plain");
    MockEditor ed;
    StylerConfig cfg;
    StyleResult r = applyToEditor(d, ed, cfg);
    CHECK(r.blinkRanges.size() == 1);
    CHECK(r.blinkRanges[0].onStyle != r.blinkRanges[0].offStyle);
    CHECK(r.blinkRanges[0].length == 5); // "Blink"
}

static void testScreenBasics() {
    // Plain text with a newline lays out on two rows.
    CHECK(renderScreen("ab\ncd").text == "ab\ncd");
    // CR overwrites from column 0.
    CHECK(renderScreen("abc\rX").text == "Xbc");
}

static void testScreenAbsolutePos() {
    // CUP to row 2, col 3 (1-based) then write: row 1 is blank, row 2 has
    // two leading spaces before 'Z'.
    ParsedDocument d = renderScreen("\x1b[2;3HZ");
    CHECK(d.text == "\n  Z");
}

static void testScreenCursorMoves() {
    // Write "AB", move cursor back 2 and up nothing, overwrite 'C' -> "CB".
    CHECK(renderScreen("AB\x1b[2DC").text == "CB");
    // Column-absolute (CHA) to column 1 then overwrite.
    CHECK(renderScreen("xyz\x1b[1GQ").text == "Qyz");
}

static void testScreenEraseLine() {
    // Erase-to-EOL (EL 0) after moving back clears the tail.
    // "ABCDE", move to col 3 (CHA 3), erase to EOL -> "AB".
    CHECK(renderScreen("ABCDE\x1b[3G\x1b[0K").text == "AB");
}

static void testScreenEraseDisplay() {
    // ED 2 clears the screen; pairing with CUP home (as real art does) puts the
    // following text at the top-left.
    CHECK(renderScreen("junk\x1b[2J\x1b[Hok").text == "ok");
}

static void testScreenKeepsAttributes() {
    // Color survives positioning: red 'A', jump, still-red 'B'.
    ParsedDocument d = renderScreen("\x1b[31mA\x1b[5GB");
    // text is "A" + 3 spaces + "B"
    CHECK(d.text == "A   B");
    // The 'B' cell keeps the red foreground.
    CHECK(d.spans.back().attr.fore == Color(205, 0, 0));
}

int main() {
    testPalette();
    testScreenBasics();
    testScreenAbsolutePos();
    testScreenCursorMoves();
    testScreenEraseLine();
    testScreenEraseDisplay();
    testScreenKeepsAttributes();
    testParseBasic();
    testParseExtendedColor();
    testParseAttributes();
    testNonSgrDropped();
    testCursorForward();
    testStylerDedup();
    testStylerStrike();
    testStylerBlink();

    if (g_failures == 0) {
        std::printf("OK: all core tests passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
