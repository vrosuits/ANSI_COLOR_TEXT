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
#include "AnsiEncoder.h"
#include "Version.h"   // generated; ANSI_VER_WIDE

#include "Scintilla.h"
#include "Notepad_plus_msgs.h"
#include "menuCmdID.h"

#include <commdlg.h>
#include <map>
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
static uintptr_t   g_animBufId  = 0;  // buffer being animated

// ---------------------------------------------------------------------------
// Non-destructive view model.
//
// The canonical content of an ANSI document is its raw byte stream (with real
// ESC 0x1b bytes). Rendering to color USED to overwrite the Scintilla buffer
// with escape-stripped text, which lost the source (so animation couldn't
// restart, and editing the colored text destroyed the codes). Instead we keep
// the raw source per Notepad++ buffer and treat the on-screen content as one of
// three interchangeable VIEWS of it:
//   - Color       : the rendered, colorized preview (read-only).
//   - RawCodes    : the raw source with real ESC bytes (editable, == on disk).
//   - RawSymbols  : the raw source with ESC shown as \e (editable, AI-friendly).
// Saves always write the RawCodes form, whatever view is showing.
// ---------------------------------------------------------------------------
enum class ViewMode { Color, RawCodes, RawSymbols };

struct BufState {
    std::string rawEsc;                 // canonical source, real ESC bytes
    ViewMode    mode   = ViewMode::RawCodes;
    bool        hasRaw = false;         // rawEsc has been captured at least once
};

// Keyed by Notepad++ buffer id (NPPM_GETCURRENTBUFFERID / notification idFrom).
static std::map<uintptr_t, BufState> g_bufStates;

// While a save is in flight we temporarily swap the canonical raw into the view
// so the file on disk is always real ANSI; this remembers what to restore after.
struct SaveRestore {
    bool      active = false;
    HWND      h      = nullptr;
    uintptr_t bufId  = 0;
    ViewMode  mode   = ViewMode::RawCodes;
};
static SaveRestore g_saveRestore;

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
    setCommand(0,  TEXT("Render ANSI Colors"),            renderAnsi,       nullptr, false);
    setCommand(1,  TEXT("Show Raw - Escape Codes"),       renderRawCodes,   nullptr, false);
    setCommand(2,  TEXT("Show Raw - \\e Symbols"),        renderRawSymbols, nullptr, false);
    setCommand(3,  TEXT("Play as Animation"),             playAnimation,    nullptr, false);
    setCommand(4,  TEXT("Stop Animation"),                stopAnimationCmd, nullptr, false);
    setCommand(5,  TEXT("Toggle Black/White Background"), toggleBackground, nullptr, false);
    setCommand(6,  TEXT("Pause/Resume Blink"),            editFlash,        nullptr, false);
    setCommand(7,  TEXT("Apply Color to Selection..."),   applyColor,       nullptr, false);
    setCommand(8,  TEXT("Insert Reset Code"),             insertReset,      nullptr, false);
    setCommand(9,  TEXT("Import ANSI File..."),           importAnsi,       nullptr, false);
    setCommand(10, TEXT("Export ANSI File..."),           exportAnsi,       nullptr, false);
    setCommand(11, TEXT("Generate ANSI with AI..."),      generateAnsiAi,   nullptr, false);
    setCommand(12, TEXT("AI Settings..."),                aiSettings,       nullptr, false);
    setCommand(13, TEXT("Settings..."),                   showSettings,     nullptr, false);
    setCommand(14, TEXT("About"),                         showAbout,        nullptr, false);
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

// Apply the optional render font and zero Scintilla's extra line spacing so
// block glyphs (U+2580/2584/2588 ...) tile without fine lines between rows. Call
// before SCI_STYLECLEARALL so the font/size propagate to every derived style.
void applyRenderFontAndSpacing(HWND h) {
    sci(h, SCI_SETEXTRAASCENT,  0, 0);
    sci(h, SCI_SETEXTRADESCENT, 0, 0);
    if (!g_settings.renderFont.empty())
        sci(h, SCI_STYLESETFONT, STYLE_DEFAULT,
            reinterpret_cast<LPARAM>(g_settings.renderFont.c_str()));
    if (g_settings.renderFontSize > 0)
        sci(h, SCI_STYLESETSIZE, STYLE_DEFAULT, g_settings.renderFontSize);
}

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
        applyRenderFontAndSpacing(h_);
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
        // Final frame: the view is now the full color render, so mark the buffer
        // as a (read-only) Color view of the stored raw — that is what lets the
        // animation be replayed later instead of reading back stripped text.
        stopAnimation();
        sci(g_animSci, SCI_SETREADONLY, 1);
        BufState& st = g_bufStates[g_animBufId];
        st.rawEsc = g_animBytes;
        st.hasRaw = true;
        st.mode   = ViewMode::Color;
        g_blinkRanges = std::move(res.blinkRanges);
        g_blinkSci    = g_animSci;
        startBlink();
    }
}

// --- view model: render the raw source as one of three interchangeable views

uintptr_t currentBufferId() {
    return static_cast<uintptr_t>(
        ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0));
}

// Write `text` as plain, editable content with default styling (no ANSI styles
// and no strike indicators left over from a prior color render).
void setPlainText(HWND h, const std::string& text) {
    ansi::Color defFore, defBack;
    defaultsForBackground(g_settings.blackBackground, defFore, defBack);
    sci(h, SCI_SETREADONLY, 0);
    sci(h, SCI_SETCODEPAGE, SC_CP_UTF8);
    sci(h, SCI_SETILEXER, 0, 0);
    applyRenderFontAndSpacing(h);
    sci(h, SCI_STYLESETFORE, STYLE_DEFAULT, toColorRef(defFore));
    sci(h, SCI_STYLESETBACK, STYLE_DEFAULT, toColorRef(defBack));
    sci(h, SCI_STYLECLEARALL);
    sci(h, SCI_CLEARALL);
    sci(h, SCI_APPENDTEXT, static_cast<WPARAM>(text.size()),
        reinterpret_cast<LPARAM>(text.data()));
    sci(h, SCI_SETINDICATORCURRENT, kStrikeIndicator);
    sci(h, SCI_INDICATORCLEARRANGE, 0, static_cast<LPARAM>(sci(h, SCI_GETLENGTH)));
}

// Capture the canonical raw (real ESC) for buffer `id` from the current view,
// honoring its mode, store it, and return it. A Color view is a stripped preview
// so we trust the stored raw; a symbolic view is converted back to real ESC.
const std::string& captureRaw(HWND h, uintptr_t id) {
    BufState& st = g_bufStates[id];
    switch (st.mode) {
        case ViewMode::Color:
            if (!st.hasRaw) { st.rawEsc = readDocument(h); st.hasRaw = true; }
            break;
        case ViewMode::RawSymbols:
            st.rawEsc = ansi::fromSymbolicEscapes(readDocument(h));
            st.hasRaw = true;
            break;
        case ViewMode::RawCodes:
        default:
            st.rawEsc = readDocument(h);
            st.hasRaw = true;
            break;
    }
    return st.rawEsc;
}

// Show the colorized, read-only preview.
void viewColor(HWND h, uintptr_t id) {
    stopAnimation();
    stopBlink();
    g_blinkRanges.clear();
    g_blinkSci = nullptr;

    std::string raw = captureRaw(h, id);
    ansi::StyleResult res = renderBytesToView(h, raw);
    sci(h, SCI_SETREADONLY, 1);  // color is a preview; edit in a Raw view

    g_bufStates[id].mode = ViewMode::Color;
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

// Show the raw source as editable text (real ESC bytes, or \e symbols).
void viewRaw(HWND h, uintptr_t id, bool symbolic) {
    stopAnimation();
    stopBlink();
    g_blinkRanges.clear();
    g_blinkSci = nullptr;

    std::string raw = captureRaw(h, id);
    setPlainText(h, symbolic ? ansi::toSymbolicEscapes(raw) : raw);
    g_bufStates[id].mode = symbolic ? ViewMode::RawSymbols : ViewMode::RawCodes;
}

// Show buffer `id` in the configured default view (after import / AI generate).
void applyDefaultView(HWND h, uintptr_t id) {
    switch (g_settings.defaultView) {
        case 1:  viewRaw(h, id, /*symbolic=*/false); break;  // raw escape codes
        case 2:  viewRaw(h, id, /*symbolic=*/true);  break;  // raw \e symbols
        case 0:
        default: viewColor(h, id);                   break;  // rendered color
    }
}

// Before a save, swap the canonical raw (real ESC) into the active view so the
// file on disk is always real ANSI regardless of the current view; restore after.
void onFileBeforeSave(uintptr_t bufId) {
    auto it = g_bufStates.find(bufId);
    if (it == g_bufStates.end() || !it->second.hasRaw) return;
    if (bufId != currentBufferId()) return;          // only the active view
    if (it->second.mode == ViewMode::RawCodes) return;  // already canonical
    HWND h = currentScintilla();
    if (!h) return;
    std::string raw = captureRaw(h, bufId);          // fold in any symbolic edits
    g_saveRestore = SaveRestore{true, h, bufId, it->second.mode};
    setPlainText(h, raw);
}

void onFileSaved(uintptr_t bufId) {
    if (!g_saveRestore.active || g_saveRestore.bufId != bufId) return;
    HWND     h = g_saveRestore.h;
    ViewMode m = g_saveRestore.mode;
    g_saveRestore.active = false;
    if (m == ViewMode::Color)           viewColor(h, bufId);
    else if (m == ViewMode::RawSymbols) viewRaw(h, bufId, true);
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

void reRenderCurrent() {
    // Re-apply settings to whatever view is active; leave untracked buffers alone
    // (don't transform a document the user never asked us to render).
    HWND h = currentScintilla();
    if (!h) return;
    uintptr_t id = currentBufferId();
    auto it = g_bufStates.find(id);
    if (it == g_bufStates.end()) return;
    if (it->second.mode == ViewMode::Color) viewColor(h, id);
    else                                    viewRaw(h, id, it->second.mode == ViewMode::RawSymbols);
}

void handleNotification(SCNotification* n) {
    if (!n) return;
    switch (n->nmhdr.code) {
        case NPPN_FILEBEFORESAVE:
            onFileBeforeSave(static_cast<uintptr_t>(n->nmhdr.idFrom));
            break;
        case NPPN_FILESAVED:
            onFileSaved(static_cast<uintptr_t>(n->nmhdr.idFrom));
            break;
        case NPPN_FILECLOSED:
            g_bufStates.erase(static_cast<uintptr_t>(n->nmhdr.idFrom));
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void renderAnsi() {
    ensureSettings();
    HWND h = currentScintilla();
    if (!h) return;
    viewColor(h, currentBufferId());
}

void renderRawCodes() {
    ensureSettings();
    HWND h = currentScintilla();
    if (!h) return;
    viewRaw(h, currentBufferId(), /*symbolic=*/false);
}

void renderRawSymbols() {
    ensureSettings();
    HWND h = currentScintilla();
    if (!h) return;
    viewRaw(h, currentBufferId(), /*symbolic=*/true);
}

void playAnimation() {
    ensureSettings();
    HWND h = currentScintilla();
    if (!h) return;

    uintptr_t id = currentBufferId();
    stopAnimation();
    stopBlink();
    g_blinkRanges.clear();
    g_blinkSci = nullptr;

    // Replay from the canonical raw source (works even if the current view is the
    // finished color render — that is what fixes "animation won't restart").
    g_animBytes = captureRaw(h, id);
    g_animPos   = 0;
    g_animSci   = h;
    g_animBufId = id;
    if (g_animBytes.empty()) return;
    sci(h, SCI_SETREADONLY, 0);   // frames rewrite the buffer as they reveal

    UINT delay = static_cast<UINT>(g_settings.animDelayMs);
    g_animRunning = ::SetTimer(nppData._nppHandle, kAnimTimerId, delay, animTimerProc) != 0;
}

void stopAnimationCmd() {
    if (!g_animRunning) return;
    stopAnimation();
    // Reveal the whole document immediately as the final (color) frame.
    if (g_animSci && !g_animBytes.empty()) {
        ansi::StyleResult res = renderBytesToView(g_animSci, g_animBytes);
        sci(g_animSci, SCI_SETREADONLY, 1);
        BufState& st = g_bufStates[g_animBufId];
        st.rawEsc = g_animBytes;
        st.hasRaw = true;
        st.mode   = ViewMode::Color;
        g_blinkRanges = std::move(res.blinkRanges);
        g_blinkSci    = g_animSci;
        startBlink();
    }
}

void toggleBackground() {
    ensureSettings();
    g_settings.blackBackground = !g_settings.blackBackground;
    persistSettings();
    HWND h = currentScintilla();
    if (!h) return;
    // Re-show whatever view is active under the new background.
    uintptr_t id = currentBufferId();
    ViewMode m = g_bufStates[id].mode;
    if (m == ViewMode::Color) viewColor(h, id);
    else                      viewRaw(h, id, m == ViewMode::RawSymbols);
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
        TEXT("ANSI Color Text  v") ANSI_VER_WIDE TEXT("\r\n\r\n")
        TEXT("Renders and edits ANSI/SGR-colored text in Notepad++ (16 / 256 / true ")
        TEXT("color, bold, italic, underline, strikethrough, inverse, blink).\r\n\r\n")
        TEXT("Virtual-screen renderer with cursor positioning, scroll regions, tab ")
        TEXT("stops, and timed animation playback. Editor commands colorize the ")
        TEXT("selection and import/export ANSI files. Configure under 'Settings...'."),
        NPP_PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
}

// ---------------------------------------------------------------------------
// Editor commands
// ---------------------------------------------------------------------------

void applyAttrToSelection(const ansi::Attr& a) {
    HWND h = currentScintilla();
    if (!h) return;

    Sci_Position selS = static_cast<Sci_Position>(sci(h, SCI_GETSELECTIONSTART));
    Sci_Position selE = static_cast<Sci_Position>(sci(h, SCI_GETSELECTIONEND));
    if (selS == selE) {
        ::MessageBox(nppData._nppHandle,
            TEXT("Select some text first, then apply a color."),
            NPP_PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }

    size_t len = static_cast<size_t>(selE - selS);
    std::string sel(len + 1, '\0');     // +1 for the NUL the call appends
    sci(h, SCI_GETSELTEXT, 0, reinterpret_cast<LPARAM>(&sel[0]));
    sel.resize(len);

    std::string wrapped = ansi::wrapSgr(sel, a);
    sci(h, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(wrapped.c_str()));
}

void insertReset() {
    HWND h = currentScintilla();
    if (!h) return;
    sci(h, SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("\x1b[0m"));
}

void importAnsi() {
    TCHAR path[MAX_PATH] = {0};
    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nppData._nppHandle;
    ofn.lpstrFilter = TEXT("ANSI files (*.ans;*.txt)\0*.ans;*.txt\0All files (*.*)\0*.*\0");
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (::GetOpenFileName(&ofn)) {
        ensureSettings();
        // Open the file as a normal document, then show it in the configured
        // default view (color by default) so imports display immediately.
        ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(path));
        HWND h = currentScintilla();
        if (h) applyDefaultView(h, currentBufferId());
    }
}

void exportAnsi() {
    HWND h = currentScintilla();
    if (!h) return;
    // Export the canonical raw ANSI (real ESC), never the stripped color preview
    // or a symbolic view.
    std::string data = captureRaw(h, currentBufferId());

    TCHAR path[MAX_PATH] = {0};
    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nppData._nppHandle;
    ofn.lpstrFilter = TEXT("ANSI files (*.ans)\0*.ans\0All files (*.*)\0*.*\0");
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrDefExt = TEXT("ans");
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    if (!::GetSaveFileName(&ofn)) return;

    HANDLE hf = ::CreateFile(path, GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        ::MessageBox(nppData._nppHandle, TEXT("Could not open the file for writing."),
                     NPP_PLUGIN_NAME, MB_OK | MB_ICONERROR);
        return;
    }
    DWORD written = 0;
    ::WriteFile(hf, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
    ::CloseHandle(hf);
}

void openInNewTabAndRender(const std::string& ansiText, bool animate) {
    ensureSettings();
    // Open a fresh document so generated output never clobbers the user's file.
    ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
    HWND h = currentScintilla();
    if (!h) return;
    sci(h, SCI_SETCODEPAGE, SC_CP_UTF8);
    sci(h, SCI_SETREADONLY, 0);
    sci(h, SCI_CLEARALL);
    sci(h, SCI_APPENDTEXT, static_cast<WPARAM>(ansiText.size()),
        reinterpret_cast<LPARAM>(ansiText.data()));
    if (animate) playAnimation();
    else         applyDefaultView(h, currentBufferId());
}
