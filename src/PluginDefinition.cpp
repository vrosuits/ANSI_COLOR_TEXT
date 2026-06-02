#include "PluginDefinition.h"
#include "AnsiParser.h"
#include "AnsiStyler.h"

#include "Scintilla.h"
#include "Notepad_plus_msgs.h"

#include <string>
#include <vector>

NppData  nppData;
FuncItem funcItem[nbFunc];

// Background mode persists across renders. Default to a black background, which
// is what most ANSI art is authored for.
bool g_blackBackground = true;

// Indicator number used to draw strikethrough (Scintilla styles lack a strike
// attribute). Indicators 0-7 are reserved by Notepad++; 8+ are free for plugins.
static const int kStrikeIndicator = 9;

// Blink animation state. Scintilla cannot blink, so a Windows timer toggles
// blink-flagged ranges between their visible and hidden styles.
static const UINT_PTR kBlinkTimerId   = 0xA751;
static const UINT     kBlinkIntervalMs = 500;
static std::vector<ansi::BlinkRange> g_blinkRanges;
static HWND g_blinkSci      = nullptr; // the view currently being animated
static bool g_blinkRunning  = false;
static bool g_blinkVisible  = true;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void pluginInit(HANDLE /*hModule*/) {}

void pluginCleanUp() {
    if (g_blinkRunning) {
        ::KillTimer(nppData._nppHandle, kBlinkTimerId);
        g_blinkRunning = false;
    }
}

bool setCommand(size_t index, const TCHAR* cmdName, PFUNCPLUGIN pFunc, ShortcutKey* sk, bool checkOnInit) {
    if (index >= nbFunc) return false;
    if (!pFunc) return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc      = pFunc;
    funcItem[index]._init2Check = checkOnInit;
    funcItem[index]._pShKey     = sk;
    return true;
}

void commandMenuInit() {
    setCommand(0, TEXT("Render ANSI Colors"),        renderAnsi,       nullptr, false);
    setCommand(1, TEXT("Toggle Black/White Background"), toggleBackground, nullptr, false);
    setCommand(2, TEXT("Toggle Blink (experimental)"), editFlash,        nullptr, false);
    setCommand(3, TEXT("About"),                     showAbout,        nullptr, false);
}

void commandMenuCleanUp() {}

// ---------------------------------------------------------------------------
// Scintilla helpers
// ---------------------------------------------------------------------------

namespace {

// Handle of the Scintilla view that currently has focus.
HWND currentScintilla() {
    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&which));
    if (which == -1) return nullptr;
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

LRESULT sci(HWND h, UINT msg, WPARAM w = 0, LPARAM l = 0) {
    return ::SendMessage(h, msg, w, l);
}

// Pack an ansi::Color into Scintilla's 0x00BBGGRR COLORREF.
COLORREF toColorRef(const ansi::Color& c) {
    return RGB(c.r, c.g, c.b);
}

// Read the entire current document as bytes.
std::string readDocument(HWND h) {
    LRESULT len = sci(h, SCI_GETLENGTH);
    std::string buf(static_cast<size_t>(len), '\0');
    if (len > 0)
        sci(h, SCI_GETTEXT, static_cast<WPARAM>(len + 1), reinterpret_cast<LPARAM>(&buf[0]));
    return buf;
}

// Concrete IEditor backed by a Scintilla view.
class ScintillaEditor : public ansi::IEditor {
public:
    ScintillaEditor(HWND h, const ansi::Color& defFore, const ansi::Color& defBack)
        : h_(h), defFore_(defFore), defBack_(defBack) {}

    void setText(const std::string& text) override {
        // Treat the buffer as raw bytes (UTF-8). CP437 ANSI art needs a matching
        // font (e.g. a "Terminal"/IBM VGA font) to show box-drawing glyphs.
        sci(h_, SCI_SETCODEPAGE, SC_CP_UTF8);
        // Disable the syntax lexer so it does not overwrite our styling.
        sci(h_, SCI_SETLEXER, SCLEX_NULL);

        // Establish the default style (background + default foreground) and
        // propagate it to all style slots before we redefine the ones we use.
        sci(h_, SCI_STYLESETFORE, STYLE_DEFAULT, toColorRef(defFore_));
        sci(h_, SCI_STYLESETBACK, STYLE_DEFAULT, toColorRef(defBack_));
        sci(h_, SCI_STYLECLEARALL);

        // Configure the strikethrough indicator.
        sci(h_, SCI_INDICSETSTYLE, kStrikeIndicator, INDIC_STRIKE);

        sci(h_, SCI_SETREADONLY, 0);
        sci(h_, SCI_CLEARALL);
        sci(h_, SCI_APPENDTEXT, static_cast<WPARAM>(text.size()),
            reinterpret_cast<LPARAM>(text.data()));
    }

    void defineStyle(int style, const ansi::Color& fore, const ansi::Color& back, uint32_t flags) override {
        if (fore.set) sci(h_, SCI_STYLESETFORE, style, toColorRef(fore));
        else          sci(h_, SCI_STYLESETFORE, style, toColorRef(defFore_));
        if (back.set) sci(h_, SCI_STYLESETBACK, style, toColorRef(back));
        else          sci(h_, SCI_STYLESETBACK, style, toColorRef(defBack_));

        sci(h_, SCI_STYLESETBOLD,      style, (flags & ansi::AF_Bold)      ? 1 : 0);
        sci(h_, SCI_STYLESETITALIC,    style, (flags & ansi::AF_Italic)    ? 1 : 0);
        sci(h_, SCI_STYLESETUNDERLINE, style, (flags & ansi::AF_Underline) ? 1 : 0);
    }

    void styleRange(size_t start, size_t length, int style) override {
        sci(h_, SCI_STARTSTYLING, static_cast<WPARAM>(start));
        sci(h_, SCI_SETSTYLING,   static_cast<WPARAM>(length), style);
    }

    void strikeRange(size_t start, size_t length) override {
        sci(h_, SCI_SETINDICATORCURRENT, kStrikeIndicator);
        sci(h_, SCI_INDICATORFILLRANGE, static_cast<WPARAM>(start), static_cast<LPARAM>(length));
    }

private:
    HWND        h_;
    ansi::Color defFore_;
    ansi::Color defBack_;
};

void defaultsForBackground(bool black, ansi::Color& fore, ansi::Color& back) {
    if (black) { back = ansi::Color{0, 0, 0};       fore = ansi::Color{229, 229, 229}; }
    else       { back = ansi::Color{255, 255, 255}; fore = ansi::Color{0, 0, 0}; }
}

// Re-style all blink ranges for the current phase (visible vs hidden).
void applyBlinkPhase(bool visible) {
    if (!g_blinkSci) return;
    for (const ansi::BlinkRange& br : g_blinkRanges) {
        int style = visible ? br.onStyle : br.offStyle;
        sci(g_blinkSci, SCI_STARTSTYLING, static_cast<WPARAM>(br.start));
        sci(g_blinkSci, SCI_SETSTYLING,   static_cast<WPARAM>(br.length), style);
    }
}

VOID CALLBACK blinkTimerProc(HWND, UINT, UINT_PTR, DWORD) {
    g_blinkVisible = !g_blinkVisible;
    applyBlinkPhase(g_blinkVisible);
}

void stopBlink() {
    if (g_blinkRunning) {
        ::KillTimer(nppData._nppHandle, kBlinkTimerId);
        g_blinkRunning = false;
    }
    // Leave blink text in its visible state.
    g_blinkVisible = true;
    applyBlinkPhase(true);
}

void startBlink() {
    if (g_blinkRunning || g_blinkRanges.empty() || !g_blinkSci) return;
    g_blinkVisible = true;
    g_blinkRunning = ::SetTimer(nppData._nppHandle, kBlinkTimerId, kBlinkIntervalMs, blinkTimerProc) != 0;
}

} // namespace

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void renderAnsi() {
    HWND h = currentScintilla();
    if (!h) return;

    // Drop any animation from a previous render before rewriting the buffer.
    stopBlink();
    g_blinkRanges.clear();
    g_blinkSci = nullptr;

    std::string raw = readDocument(h);
    ansi::ParsedDocument doc = ansi::parse(raw);

    ansi::Color defFore, defBack;
    defaultsForBackground(g_blackBackground, defFore, defBack);

    ansi::StylerConfig cfg;
    cfg.defaultFore = defFore;
    cfg.defaultBack = defBack;

    ScintillaEditor editor(h, defFore, defBack);
    ansi::StyleResult res = ansi::applyToEditor(doc, editor, cfg);

    // Animate any blink ranges this document produced.
    g_blinkRanges = std::move(res.blinkRanges);
    g_blinkSci    = h;
    startBlink();

    if (res.budgetExceeded) {
        ::MessageBox(nppData._nppHandle,
            TEXT("This file uses more distinct color/attribute combinations than ")
            TEXT("the available style slots. Some runs were rendered with a fallback style."),
            NPP_PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
    }
}

void toggleBackground() {
    g_blackBackground = !g_blackBackground;
    renderAnsi(); // re-render with the new default background
}

void editFlash() {
    if (g_blinkRanges.empty()) {
        ::MessageBox(nppData._nppHandle,
            TEXT("No blinking text in the current render. Run 'Render ANSI Colors' ")
            TEXT("on a file that uses the blink attribute (SGR 5) first."),
            NPP_PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }
    // Pause/resume the blink animation.
    if (g_blinkRunning) stopBlink();
    else                startBlink();
}

void showAbout() {
    ::MessageBox(nppData._nppHandle,
        TEXT("ANSI Color Text\r\n\r\n")
        TEXT("Renders ANSI/SGR-colored text in Notepad++ (16 / 256 / true color, ")
        TEXT("bold, italic, underline, strikethrough, inverse).\r\n\r\n")
        TEXT("Use 'Render ANSI Colors' on a file containing ANSI escape sequences."),
        NPP_PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
}
