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

// AiDialogs.cpp - the "Generate ANSI with AI" and "AI Settings" dialogs, plus
// the generation flow that calls a provider and opens the result in a new tab.
#include "PluginDefinition.h"
#include "resource.h"
#include "AiClient.h"
#include "AiHttp.h"

#include <windows.h>
#include <string>
#include <vector>

namespace {

std::wstring toW(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
std::string toN(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::string getText(HWND dlg, int id) {
    int len = ::GetWindowTextLength(::GetDlgItem(dlg, id));
    std::wstring w(len, L'\0');
    if (len) ::GetDlgItemText(dlg, id, &w[0], len + 1);
    return toN(w);
}
void setText(HWND dlg, int id, const std::string& s) {
    ::SetDlgItemText(dlg, id, toW(s).c_str());
}

// Build the selected provider with its overrides and decrypted key.
ansi::AiProvider currentProvider() {
    std::vector<ansi::AiProvider> defs = ansi::defaultProviders();
    int i = g_settings.aiSelected;
    if (i < 0 || i >= (int)defs.size()) i = 0;
    ansi::AiProvider p = defs[i];
    if (i < (int)g_settings.ai.size()) {
        if (!g_settings.ai[i].baseUrl.empty()) p.baseUrl = g_settings.ai[i].baseUrl;
        if (!g_settings.ai[i].model.empty())   p.model   = g_settings.ai[i].model;
        p.apiKey = dpapiUnprotect(g_settings.ai[i].apiKeyEnc);
    }
    return p;
}

// ----- Generate dialog -----------------------------------------------------

std::string g_prompt;
bool        g_animation = false;

INT_PTR CALLBACK genProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
        case WM_INITDIALOG: {
            ansi::AiProvider p = currentProvider();
            setText(dlg, IDC_GEN_PROVIDER, "Using: " + p.name + " (" + p.model + ")");
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    g_prompt = getText(dlg, IDC_GEN_PROMPT);
                    g_animation = ::IsDlgButtonChecked(dlg, IDC_GEN_ANIMATION) == BST_CHECKED;
                    ::EndDialog(dlg, IDOK);
                    return TRUE;
                case IDCANCEL:
                    ::EndDialog(dlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

void runGeneration() {
    ansi::AiProvider p = currentProvider();
    if (p.apiKey.empty() && p.name != "Ollama") {
        ::MessageBox(nppData._nppHandle,
            TEXT("No API key set for this provider. Open 'AI Settings...' to add one ")
            TEXT("(or select Ollama for a local server)."),
            NPP_PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::string system = ansi::aiSystemPrompt(g_animation);
    ansi::AiRequest req = ansi::buildRequest(p, system, g_prompt);

    HCURSOR prev = ::SetCursor(::LoadCursor(nullptr, IDC_WAIT));
    std::string body, err;
    long status = 0;
    bool ok = httpPostJson(req, body, status, err);
    ::SetCursor(prev);

    if (!ok) {
        ::MessageBox(nppData._nppHandle, toW(err).c_str(), NPP_PLUGIN_NAME, MB_OK | MB_ICONERROR);
        return;
    }
    ansi::AiResult res = ansi::parseResponse(p.kind, body, status);
    if (!res.ok) {
        ::MessageBox(nppData._nppHandle, toW(res.error).c_str(), NPP_PLUGIN_NAME, MB_OK | MB_ICONERROR);
        return;
    }

    std::string text = ansi::normalizeEscapes(ansi::stripCodeFence(res.text));
    openInNewTabAndRender(text, g_animation);
}

// ----- AI Settings dialog --------------------------------------------------

int g_uiIndex = 0;  // provider currently shown in the AI settings dialog

// Copy the dialog's edit fields into settings for provider `idx` (encrypting
// the key).
void storeProvider(HWND dlg, int idx) {
    if (idx < 0 || idx >= (int)g_settings.ai.size()) return;
    g_settings.ai[idx].baseUrl   = getText(dlg, IDC_AI_BASE);
    g_settings.ai[idx].model     = getText(dlg, IDC_AI_MODEL);
    std::string key = getText(dlg, IDC_AI_KEY);
    g_settings.ai[idx].apiKeyEnc = key.empty() ? std::string() : dpapiProtect(key);
}

// Load provider `idx` from settings into the dialog's edit fields.
void loadProvider(HWND dlg, int idx) {
    if (idx < 0 || idx >= (int)g_settings.ai.size()) return;
    setText(dlg, IDC_AI_BASE,  g_settings.ai[idx].baseUrl);
    setText(dlg, IDC_AI_MODEL, g_settings.ai[idx].model);
    setText(dlg, IDC_AI_KEY,   dpapiUnprotect(g_settings.ai[idx].apiKeyEnc));
}

INT_PTR CALLBACK aiProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
        case WM_INITDIALOG: {
            std::vector<ansi::AiProvider> defs = ansi::defaultProviders();
            for (const ansi::AiProvider& d : defs)
                ::SendDlgItemMessage(dlg, IDC_AI_PROVIDER, CB_ADDSTRING, 0,
                                     reinterpret_cast<LPARAM>(toW(d.name).c_str()));
            g_uiIndex = g_settings.aiSelected;
            ::SendDlgItemMessage(dlg, IDC_AI_PROVIDER, CB_SETCURSEL, g_uiIndex, 0);
            loadProvider(dlg, g_uiIndex);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_AI_PROVIDER:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        storeProvider(dlg, g_uiIndex);          // save edits for the old one
                        int sel = (int)::SendDlgItemMessage(dlg, IDC_AI_PROVIDER, CB_GETCURSEL, 0, 0);
                        if (sel >= 0) { g_uiIndex = sel; loadProvider(dlg, sel); }
                    }
                    return TRUE;
                case IDOK:
                    storeProvider(dlg, g_uiIndex);
                    g_settings.aiSelected = g_uiIndex;
                    persistSettings();
                    ::EndDialog(dlg, IDOK);
                    return TRUE;
                case IDCANCEL:
                    ::EndDialog(dlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

} // namespace

void generateAnsiAi() {
    ensureSettings();
    if (::DialogBoxParam(g_hModule, MAKEINTRESOURCE(IDD_GENERATE),
                         nppData._nppHandle, genProc, 0) == IDOK) {
        if (!g_prompt.empty()) runGeneration();
    }
}

void aiSettings() {
    ensureSettings();
    ::DialogBoxParam(g_hModule, MAKEINTRESOURCE(IDD_AI),
                     nppData._nppHandle, aiProc, 0);
}
