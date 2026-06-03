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

// test_core.cpp - host self-test for the ANSI parsing/styling core.
// No Notepad++ or Scintilla needed; build via the ANSI_BUILD_TESTS CMake option.
#include "AnsiParser.h"
#include "AnsiScreen.h"
#include "AnsiPalette.h"
#include "AnsiStyler.h"
#include "AnsiEncoder.h"
#include "AiJson.h"
#include "AiClient.h"

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

static void testTabs() {
    // Default tab stops every 8 columns: 'A' then tab -> column 8.
    ParsedDocument d = renderScreen("A\tB");
    CHECK(d.text == "A       B"); // A + 7 spaces + B (B at col 8)
    // Custom tab width.
    ScreenConfig cfg; cfg.tabWidth = 4;
    CHECK(renderScreen("A\tB", cfg).text == "A   B"); // B at col 4
}

static void testEraseUsesBackground() {
    // With a background set, ECH paints the cells (they become visible spaces
    // carrying that background) rather than vanishing.
    ScreenConfig cfg; // eraseUsesBackground defaults true
    ParsedDocument d = renderScreen("\x1b[41mAB\x1b[3D\x1b[2X", cfg);
    // "AB" written red-bg, cursor back 3 (to col 0... clamped), erase 2 chars.
    bool anyRedBack = false;
    for (const Span& s : d.spans)
        if (s.attr.back == Color(205, 0, 0)) anyRedBack = true;
    CHECK(anyRedBack);

    // With the option off, erased cells are transparent (default span).
    ScreenConfig off; off.eraseUsesBackground = false;
    ParsedDocument d2 = renderScreen("\x1b[41mXY\x1b[2D\x1b[2X", off);
    CHECK(d2.text.find_first_not_of(" \n") == std::string::npos); // all blank
}

static void testScrollRegion() {
    // Bounded 3-row screen; write 4 lines so the top scrolls off.
    ScreenConfig cfg; cfg.height = 3;
    ParsedDocument d = renderScreen("L1\nL2\nL3\nL4", cfg);
    // After the 4th line feed at the bottom, L1 has scrolled away.
    CHECK(d.text == "L2\nL3\nL4");
}

static void testReverseIndex() {
    // RI (ESC M) moves up a line without changing column; writing then lands
    // on the previous row.
    ParsedDocument d = renderScreen("A\nB\x1bM C");
    // Row0: "A", then "B" on row1 col0->col1, RI up to row0 col1, space+C.
    CHECK(d.text == "A C\nB");
}

static void testFrameBoundary() {
    // Boundaries always make progress, land at or past the chunk, and never
    // split an escape (the prefix must not end with a partial ESC sequence).
    std::string s = "\x1b[31mAB\x1b[0mC\x1b[1;5HD";
    size_t pos = 0, guard = 0;
    while (pos < s.size() && guard++ < 1000) {
        size_t next = nextFrameBoundary(s, pos, 1);
        CHECK(next > pos);
        CHECK(next <= s.size());
        // The byte just before the cut is a printable, not part of an escape.
        CHECK(s[next - 1] != '\x1b');
        pos = next;
    }
    CHECK(pos == s.size());
}

static void testAnimationNeverBlank() {
    // Mimic draw-then-erase animation steps. Walking the playback with the
    // boundary snapper, no frame after the first glyph is ever blank.
    std::string s = "\x1b[1;1HX\x1b[1;3HY\x1b[1;1H \x1b[1;5HZ\x1b[1;3H ";
    size_t pos = 0;
    bool drawn = false;
    while (pos < s.size()) {
        pos = nextFrameBoundary(s, pos, 2);
        ParsedDocument d = renderScreen(s.substr(0, pos));
        bool hasGlyph = d.text.find_first_of("XYZ") != std::string::npos;
        if (drawn) CHECK(hasGlyph); // once something is drawn, never blank again
        if (hasGlyph) drawn = true;
    }
    CHECK(drawn);
}

static void testEncodeSgr() {
    // Default attr -> no sequence.
    CHECK(encodeSgr(Attr{}) == "");

    // 16-color foreground red (index 1) -> code 31.
    Attr red; red.fore = palette16(1);
    CHECK(encodeSgr(red) == "\x1b[31m");

    // Bright bg (index 12) + bold.
    Attr a; a.flags = AF_Bold; a.back = palette16(12);
    CHECK(encodeSgr(a) == "\x1b[1;104m");

    // 256-color picks the 5;n form.
    Attr c256; c256.fore = palette256(202);
    CHECK(encodeSgr(c256) == "\x1b[38;5;202m");

    // True color falls back to 2;r;g;b.
    Attr tc; tc.back = Color(10, 20, 30);
    CHECK(encodeSgr(tc) == "\x1b[48;2;10;20;30m");

    // wrapSgr brackets the text with the sequence and a reset.
    CHECK(wrapSgr("hi", red) == "\x1b[31mhi\x1b[0m");
    CHECK(wrapSgr("plain", Attr{}) == "plain");
}

static void testEncodeRoundTrip() {
    // Encoding a parsed document and re-parsing reproduces text and attributes.
    std::string original = "\x1b[1;31mError\x1b[0m: \x1b[38;5;46mok\x1b[0m plain";
    ParsedDocument a = parse(original);
    std::string encoded = encode(a);
    ParsedDocument b = parse(encoded);

    CHECK(a.text == b.text);
    // Compare effective per-character attributes via flattened spans.
    auto flatten = [](const ParsedDocument& d) {
        std::vector<Attr> per(d.text.size());
        for (const Span& s : d.spans)
            for (size_t i = 0; i < s.length; ++i) per[s.start + i] = s.attr;
        return per;
    };
    CHECK(flatten(a) == flatten(b));
}

static void testJson() {
    JsonValue v;
    CHECK(parseJson("{\"a\":1,\"b\":[true,\"hi\\n\"],\"c\":{\"d\":\"x\"}}", v));
    CHECK(v.isObject());
    CHECK(v.find("a") && v.find("a")->type == JsonValue::Type::Number);
    const JsonValue* b = v.find("b");
    CHECK(b && b->isArray() && b->at(1) && b->at(1)->asString() == "hi\n");
    const JsonValue* c = v.find("c");
    CHECK(c && c->find("d") && c->find("d")->asString() == "x");
    // Unicode escape decodes to UTF-8.
    JsonValue u;
    CHECK(parseJson("\"\\u00e9\"", u));   // e-acute
    CHECK(u.asString() == "\xc3\xa9");
    // Malformed input is rejected.
    JsonValue bad;
    CHECK(!parseJson("{\"a\":}", bad));
}

static void testJsonEscapeAndBase64() {
    CHECK(jsonEscape("a\"b\\c\n") == "a\\\"b\\\\c\\n");
    // base64 round-trips arbitrary bytes.
    std::string raw = std::string("\x00\x01\x02hello\xff", 9);
    CHECK(base64Decode(base64Encode(raw)) == raw);
    CHECK(base64Encode("Man") == "TWFu");
}

static void testAiRequest() {
    std::vector<AiProvider> ps = defaultProviders();
    CHECK(ps.size() == 5);

    // OpenAI-compatible shape.
    AiProvider openai{"OpenAI", AiKind::OpenAICompatible, "https://api.openai.com/", "KEY", "gpt-4o"};
    AiRequest r = buildRequest(openai, "SYS", "USER");
    CHECK(r.url == "https://api.openai.com/v1/chat/completions"); // trailing slash trimmed
    bool hasAuth = false;
    for (const std::string& h : r.headers) if (h == "Authorization: Bearer KEY") hasAuth = true;
    CHECK(hasAuth);
    CHECK(r.body.find("\"model\":\"gpt-4o\"") != std::string::npos);
    CHECK(r.body.find("\"role\":\"system\"") != std::string::npos);

    // Anthropic shape.
    AiProvider ant{"Anthropic", AiKind::Anthropic, "https://api.anthropic.com", "K2", "claude"};
    AiRequest ra = buildRequest(ant, "SYS", "USER");
    CHECK(ra.url == "https://api.anthropic.com/v1/messages");
    bool hasKey = false, hasVer = false;
    for (const std::string& h : ra.headers) {
        if (h == "x-api-key: K2") hasKey = true;
        if (h == "anthropic-version: 2023-06-01") hasVer = true;
    }
    CHECK(hasKey && hasVer);
    CHECK(ra.body.find("\"max_tokens\"") != std::string::npos);
}

static void testAiResponse() {
    // OpenAI-compatible success.
    AiResult a = parseResponse(AiKind::OpenAICompatible,
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"HELLO\"}}]}", 200);
    CHECK(a.ok && a.text == "HELLO");

    // Anthropic success.
    AiResult b = parseResponse(AiKind::Anthropic,
        "{\"content\":[{\"type\":\"text\",\"text\":\"HI\"}]}", 200);
    CHECK(b.ok && b.text == "HI");

    // Error object surfaces the message.
    AiResult e = parseResponse(AiKind::OpenAICompatible,
        "{\"error\":{\"message\":\"bad key\"}}", 401);
    CHECK(!e.ok && e.error == "bad key");
}

static void testFenceAndEscapes() {
    CHECK(stripCodeFence("```ansi\nBODY\n```") == "BODY");
    CHECK(stripCodeFence("no fence here") == "no fence here");
    // Textual escapes -> real ESC byte.
    CHECK(normalizeEscapes("\\e[31mX") == "\x1b[31mX");
    CHECK(normalizeEscapes("\\033[0m") == "\x1b[0m");
    CHECK(normalizeEscapes("\\x1b[1m") == "\x1b[1m");
    CHECK(normalizeEscapes("\\u001b[7m") == "\x1b[7m");
    // A real ESC is preserved.
    CHECK(normalizeEscapes("\x1b[5m") == "\x1b[5m");
}

static void testSymbolicEscapes() {
    using ansi::toSymbolicEscapes;
    using ansi::fromSymbolicEscapes;

    // Round-trip: real ESC <-> "\e", literal backslash preserved.
    std::string raw = "\x1b[31mred\x1b[0m and a\\b";
    std::string sym = toSymbolicEscapes(raw);
    CHECK(sym == "\\e[31mred\\e[0m and a\\\\b");
    CHECK(fromSymbolicEscapes(sym) == raw);

    // Lenient parse of the forms a human/AI may type.
    CHECK(fromSymbolicEscapes("ESC[1mX") == "\x1b[1mX");       // bare ESC + CSI
    CHECK(fromSymbolicEscapes("esc]0;t") == "\x1b]0;t");       // bare esc + OSC
    CHECK(fromSymbolicEscapes("\\033[2J") == "\x1b[2J");       // octal
    CHECK(fromSymbolicEscapes("\\x1b[m") == "\x1b[m");          // hex
    CHECK(fromSymbolicEscapes("\\u001b[m") == "\x1b[m");        // unicode
    CHECK(fromSymbolicEscapes("^[[m") == "\x1b[m");             // caret notation
    // A bare "ESC" not introducing a sequence is left alone (avoid false hits).
    CHECK(fromSymbolicEscapes("ESCAPE") == "ESCAPE");
}

int main() {
    testPalette();
    testFrameBoundary();
    testAnimationNeverBlank();
    testEncodeSgr();
    testEncodeRoundTrip();
    testSymbolicEscapes();
    testJson();
    testJsonEscapeAndBase64();
    testAiRequest();
    testAiResponse();
    testFenceAndEscapes();
    testScreenBasics();
    testScreenAbsolutePos();
    testScreenCursorMoves();
    testScreenEraseLine();
    testScreenEraseDisplay();
    testScreenKeepsAttributes();
    testTabs();
    testEraseUsesBackground();
    testScrollRegion();
    testReverseIndex();
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
