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

// SettingsDialog.cpp - the modal settings dialog (resource IDD_SETTINGS).
#include "PluginDefinition.h"
#include "resource.h"

#include <windows.h>

namespace {

void setInt(HWND dlg, int id, int value) {
    TCHAR buf[32];
    wsprintf(buf, TEXT("%d"), value);
    ::SetDlgItemText(dlg, id, buf);
}

int getInt(HWND dlg, int id, int dflt) {
    BOOL ok = FALSE;
    int v = static_cast<int>(::GetDlgItemInt(dlg, id, &ok, FALSE));
    return ok ? v : dflt;
}

void checkBox(HWND dlg, int id, bool on) {
    ::CheckDlgButton(dlg, id, on ? BST_CHECKED : BST_UNCHECKED);
}

bool isChecked(HWND dlg, int id) {
    return ::IsDlgButtonChecked(dlg, id) == BST_CHECKED;
}

// Populate the controls from g_settings.
void load(HWND dlg) {
    setInt(dlg, IDC_MAXWIDTH,  g_settings.maxWidth);
    setInt(dlg, IDC_HEIGHT,    g_settings.screenHeight);
    setInt(dlg, IDC_TABWIDTH,  g_settings.tabWidth);
    checkBox(dlg, IDC_ERASEBG,   g_settings.eraseUsesBackground);
    checkBox(dlg, IDC_SCROLLRGN, g_settings.scrollRegionEnabled);
    checkBox(dlg, IDC_BG_BLACK,  g_settings.blackBackground);
    checkBox(dlg, IDC_BG_WHITE, !g_settings.blackBackground);
    setInt(dlg, IDC_BLINK,     g_settings.blinkIntervalMs);
    setInt(dlg, IDC_ANIMDELAY, g_settings.animDelayMs);
    setInt(dlg, IDC_ANIMCHUNK, g_settings.animChunkBytes);

    HWND combo = ::GetDlgItem(dlg, IDC_DEFAULTVIEW);
    ::SendMessage(combo, CB_RESETCONTENT, 0, 0);
    ::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(TEXT("Color (rendered)")));
    ::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(TEXT("Raw - escape codes")));
    ::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(TEXT("Raw - \\e symbols")));
    int v = (g_settings.defaultView >= 0 && g_settings.defaultView <= 2) ? g_settings.defaultView : 0;
    ::SendMessage(combo, CB_SETCURSEL, static_cast<WPARAM>(v), 0);
}

// Read the controls back into g_settings, clamping to sane ranges.
void store(HWND dlg) {
    g_settings.maxWidth     = getInt(dlg, IDC_MAXWIDTH, g_settings.maxWidth);
    g_settings.screenHeight = getInt(dlg, IDC_HEIGHT,   g_settings.screenHeight);
    g_settings.tabWidth     = getInt(dlg, IDC_TABWIDTH, g_settings.tabWidth);
    g_settings.eraseUsesBackground = isChecked(dlg, IDC_ERASEBG);
    g_settings.scrollRegionEnabled = isChecked(dlg, IDC_SCROLLRGN);
    g_settings.blackBackground     = isChecked(dlg, IDC_BG_BLACK);
    g_settings.blinkIntervalMs = getInt(dlg, IDC_BLINK,     g_settings.blinkIntervalMs);
    g_settings.animDelayMs     = getInt(dlg, IDC_ANIMDELAY, g_settings.animDelayMs);
    g_settings.animChunkBytes  = getInt(dlg, IDC_ANIMCHUNK, g_settings.animChunkBytes);

    if (g_settings.maxWidth < 0)         g_settings.maxWidth = 0;
    if (g_settings.screenHeight < 0)     g_settings.screenHeight = 0;
    if (g_settings.tabWidth < 1)         g_settings.tabWidth = 1;
    if (g_settings.blinkIntervalMs < 50) g_settings.blinkIntervalMs = 50;
    if (g_settings.animDelayMs < 1)      g_settings.animDelayMs = 1;
    if (g_settings.animChunkBytes < 1)   g_settings.animChunkBytes = 1;

    LRESULT sel = ::SendMessage(::GetDlgItem(dlg, IDC_DEFAULTVIEW), CB_GETCURSEL, 0, 0);
    if (sel != CB_ERR) g_settings.defaultView = static_cast<int>(sel);
}

INT_PTR CALLBACK dlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
        case WM_INITDIALOG:
            load(dlg);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    store(dlg);
                    persistSettings();
                    ::EndDialog(dlg, IDOK);
                    reRenderCurrent();   // apply immediately
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

void showSettings() {
    ensureSettings();
    ::DialogBoxParam(g_hModule, MAKEINTRESOURCE(IDD_SETTINGS),
                     nppData._nppHandle, dlgProc, 0);
}
