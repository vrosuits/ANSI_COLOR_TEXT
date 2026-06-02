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

#include "PluginDefinition.h"
#include "AnsiScreen.h"
#include "AnsiStyler.h"

#include "Scintilla.h"
#include "Notepad_plus_msgs.h"

#include <string>
#include <vector>

NppData       nppData;
FuncItem      funcItem[nbFunc];
PluginSettings g_settings;
HINSTANCE     g_hModule = nullptr;

static bool g_settingsLoaded = false;

// Indicator number used to draw strikethrough (Scintilla styles lack a strike
// attribute). Indicators 0-7 are reserved by Notepad++; 8+ are free for plugins.
static const int kStrikeIndicator = 9;

// Timers: blink toggling and animation playback.
static const UINT_PTR kBlinkTimerId = 0xA751;
static const UINT_PTR kAnimTimerId  = 0xA752;

static std::vector<ansi::BlinkRange> g_blinkRanges;
static HWND g_blinkSci     = nullptr;
static bool g_blinkRunning = false;
static bool g_blinkVisible = true;

static std::string g_animBytes;       // full source being played back
static size_t      g_animPos    = 0;  // bytes revealed so far
static HWND        g_animSci    = nullptr;
static bool        g_animRunning = false;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void pluginInit(HANDLE hModule) {
    g_hModule = static_cast<HINSTANCE>(hModule);
}

void pluginCleanUp() {
    if (g_blinkRunning) { ::KillTimer(nppData._nppHandle, kBlinkTimerId); g_blinkRunning = false; }
    if (g_animRunning)  { ::KillTimer(nppData._nppHandle, kAnimTimerId);  g_animRunning  = false; }
}

bool setCommand(size_t index, const TCHAR* cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey* sk, bool checkOnInit) {
    if (index >= nbFunc) return false;
    if (!pFunc) return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc      = pFunc;
    funcItem[index]._init2Check = checkOnInit;
    funcItem[index]._pShKey     = sk;
    return true;
}

void commandMenuInit() {
    setCommand(0, TEXT("Render ANSI Colors"),            renderAnsi,       nullptr, false);
    setCommand(1, TEXT("Play as Animation"),             playAnimation,    nullptr, false);
    setCommand(2, TEXT("Stop Animation"),                stopAnimationCmd, nullptr, false);
    setCommand(3, TEXT("Toggle Black/White Background"), toggleBackground, nullptr, false);
    setCommand(4, TEXT("Pause/Resume Blink"),            editFlash,        nullptr, false);
    setCommand(5, TEXT("Settings..."),                   showSettings,     nullptr, false);
    setCommand(6, TEXT("About"),                         showAbout,        nullptr, false);
}

void commandMenuCleanUp() {}

// ---------------------------------------------------------------------------
// Scintilla helpers
// ---------------------------------------------------------------------------

namespace {

HWND currentScintilla() {
    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&which));
    if (which == -1) return nullptr;
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

LRESULT sci(HWND h, UINT msg, WPARAM w = 0, LPARAM l = 0) {
    return ::SendMessage(h, msg, w, l);
}

COLORREF toColorRef(const ansi::Color& c) { return RGB(c.r, c.g, c.b); }

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
        sci(h_, SCI_SETCODEPAGE, SC_CP_UTF8);
        // Modern Scintilla: SCI_SETLEXER is gone. Setting a null ILexer leaves
        // the document in container/no-lexer mode so our styling is not undone.
        sci(h_, SCI_SETILEXER, 0, 0);
        sci(h_, SCI_STYLESETFORE, STYLE_DEFAULT, toColorRef(defFore_));
        sci(h_, SCI_STYLESETBACK, STYLE_DEFAULT, toColorRef(defBack_));
        sci(h_, SCI_STYLECLEARALL);
        sci(h_, SCI_INDICSETSTYLE, kStrikeIndicator, INDIC_STRIKE);
        sci(h_, SCI_SETREADONLY, 0);
        sci(h_, SCI_CLEARALL);
        sci(h_, SCI_APPENDTEXT, static_cast<WPARAM>(text.size()),
            reinterpret_cast<LPARAM>(text.data()));
    }

    void defineStyle(int style, const ansi::Color& fore, const ansi::Color& back, uint32_t flags) override {
        sci(h_, SCI_STYLESETFORE, style, toColorRef(fore.set ? fore : defFore_));
        sci(h_, SCI_STYLESETBACK, style, toColorRef(back.set ? back : defBack_));
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

std::wstring pluginConfigDir() {
    TCHAR dir[MAX_PATH];
    dir[0] = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, reinterpret_cast<LPARAM>(dir));
    return dir;
}

// Render `raw` to view `h` using current settings; returns blink ranges etc.
ansi::StyleResult renderBytesToView(HWND h, const std::string& raw) {
    ansi::Color defFore, defBack;
    defaultsForBackground(g_settings.blackBackground, defFore, defBack);

    ansi::StylerConfig cfg;
    cfg.defaultFore = defFore;
    cfg.defaultBack = defBack;

    ansi::ParsedDocument doc = ansi::renderScreen(raw, g_settings.toScreenConfig());
    ScintillaEditor editor(h, defFore, defBack);
    return ansi::applyToEditor(doc, editor, cfg);
}

// --- blink ---------------------------------------------------------------
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
    if (g_blinkRunning) { ::KillTimer(nppData._nppHandle, kBlinkTimerId); g_blinkRunning = false; }
    g_blinkVisible = true;
    applyBlinkPhase(true);
}

void startBlink() {
    if (g_blinkRunning || g_blinkRanges.empty() || !g_blinkSci) return;
    g_blinkVisible = true;
    UINT interval = static_cast<UINT>(g_settings.blinkIntervalMs);
    g_blinkRunning = ::SetTimer(nppData._nppHandle, kBlinkTimerId, interval, blinkTimerProc) != 0;
}

// --- animation -----------------------------------------------------------
void stopAnimation() {
    if (g_animRunning) { ::KillTimer(nppData._nppHandle, kAnimTimerId); g_animRunning = false; }
}

VOID CALLBACK animTimerProc(HWND, UINT, UINT_PTR, DWORD) {
    if (!g_animSci) { stopAnimation(); return; }

    size_t chunk = static_cast<size_t>(g_settings.animChunkBytes);
    // Snap to a glyph boundary so a frame never ends mid-escape (flicker-free).
    g_animPos = ansi::nextFrameBoundary(g_animBytes, g_animPos, chunk);

    ansi::StyleResult res = renderBytesToView(g_animSci, g_animBytes.substr(0, g_animPos));

    if (g_animPos >= g_animBytes.size()) {
        // Final frame: stop and hand any blink ranges to the blink animator.
        stopAnimation();
        g_blinkRanges = std::move(res.blinkRanges);
        g_blinkSci    = g_animSci;
        startBlink();
    }
}

} // namespace

void ensureSettings() {
    if (g_settingsLoaded) return;
    loadSettings(pluginConfigDir().c_str(), g_settings);
    g_settingsLoaded = true;
}

void persistSettings() {
    saveSettings(pluginConfigDir().c_str(), g_settings);
}

void reRenderCurrent() { renderAnsi(); }

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void renderAnsi() {
    ensureSettings();
    HWND h = currentScintilla();
    if (!h) return;

    stopAnimation();
    stopBlink();
    g_blinkRanges.clear();
    g_blinkSci = nullptr;

    std::string raw = readDocument(h);
    ansi::StyleResult res = renderBytesToView(h, raw);

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

void playAnimation() {
    ensureSettings();
    HWND h = currentScintilla();
    if (!h) return;

    stopAnimation();
    stopBlink();
    g_blinkRanges.clear();
    g_blinkSci = nullptr;

    g_animBytes = readDocument(h);
    g_animPos   = 0;
    g_animSci   = h;
    if (g_animBytes.empty()) return;

    UINT delay = static_cast<UINT>(g_settings.animDelayMs);
    g_animRunning = ::SetTimer(nppData._nppHandle, kAnimTimerId, delay, animTimerProc) != 0;
}

void stopAnimationCmd() {
    if (!g_animRunning) return;
    stopAnimation();
    // Reveal the whole document immediately on the final frame.
    if (g_animSci && !g_animBytes.empty()) {
        ansi::StyleResult res = renderBytesToView(g_animSci, g_animBytes);
        g_blinkRanges = std::move(res.blinkRanges);
        g_blinkSci    = g_animSci;
        startBlink();
    }
}

void toggleBackground() {
    ensureSettings();
    g_settings.blackBackground = !g_settings.blackBackground;
    persistSettings();
    renderAnsi();
}

void editFlash() {
    if (g_blinkRanges.empty()) {
        ::MessageBox(nppData._nppHandle,
            TEXT("No blinking text in the current render. Run 'Render ANSI Colors' ")
            TEXT("on a file that uses the blink attribute (SGR 5) first."),
            NPP_PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (g_blinkRunning) stopBlink();
    else                startBlink();
}

void showAbout() {
    ::MessageBox(nppData._nppHandle,
        TEXT("ANSI Color Text\r\n\r\n")
        TEXT("Renders ANSI/SGR-colored text in Notepad++ (16 / 256 / true color, ")
        TEXT("bold, italic, underline, strikethrough, inverse, blink).\r\n\r\n")
        TEXT("Virtual-screen renderer with cursor positioning, scroll regions, ")
        TEXT("tab stops, and timed animation playback. Configure under 'Settings...'."),
        NPP_PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
}
