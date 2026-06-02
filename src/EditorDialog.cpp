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

// EditorDialog.cpp - the "Apply ANSI Color to Selection" dialog (IDD_EDITOR).
// Lets the user pick a foreground/background color (via the native color
// chooser) and SGR attributes, then wraps the current selection with the
// corresponding escape sequence.
#include "PluginDefinition.h"
#include "resource.h"
#include "AnsiColor.h"

#include <windows.h>
#include <commdlg.h>

namespace {

// Chosen colors persist between opens; custom-color slots are kept for the
// native ChooseColor dialog.
COLORREF g_fg = RGB(229, 229, 229);
COLORREF g_bg = RGB(0, 0, 0);
COLORREF g_custom[16] = {0};

bool isChecked(HWND dlg, int id) { return ::IsDlgButtonChecked(dlg, id) == BST_CHECKED; }
void setCheck(HWND dlg, int id, bool on) { ::CheckDlgButton(dlg, id, on ? BST_CHECKED : BST_UNCHECKED); }

bool pickColor(HWND owner, COLORREF& io) {
    CHOOSECOLOR cc = {0};
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = owner;
    cc.rgbResult    = io;
    cc.lpCustColors = g_custom;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    if (::ChooseColor(&cc)) { io = cc.rgbResult; return true; }
    return false;
}

ansi::Color toColor(COLORREF c) {
    return ansi::Color(GetRValue(c), GetGValue(c), GetBValue(c));
}

INT_PTR CALLBACK dlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
        case WM_INITDIALOG:
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_FG_PICK:
                    if (pickColor(dlg, g_fg)) setCheck(dlg, IDC_FG_SET, true);
                    return TRUE;
                case IDC_BG_PICK:
                    if (pickColor(dlg, g_bg)) setCheck(dlg, IDC_BG_SET, true);
                    return TRUE;
                case IDOK: {
                    ansi::Attr a;
                    if (isChecked(dlg, IDC_FG_SET)) a.fore = toColor(g_fg);
                    if (isChecked(dlg, IDC_BG_SET)) a.back = toColor(g_bg);
                    if (isChecked(dlg, IDC_AT_BOLD))      a.flags |= ansi::AF_Bold;
                    if (isChecked(dlg, IDC_AT_ITALIC))    a.flags |= ansi::AF_Italic;
                    if (isChecked(dlg, IDC_AT_UNDERLINE)) a.flags |= ansi::AF_Underline;
                    if (isChecked(dlg, IDC_AT_BLINK))     a.flags |= ansi::AF_Blink;
                    if (isChecked(dlg, IDC_AT_INVERSE))   a.flags |= ansi::AF_Inverse;
                    if (isChecked(dlg, IDC_AT_STRIKE))    a.flags |= ansi::AF_Strike;
                    ::EndDialog(dlg, IDOK);
                    applyAttrToSelection(a);
                    return TRUE;
                }
                case IDCANCEL:
                    ::EndDialog(dlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

} // namespace

void applyColor() {
    ::DialogBoxParam(g_hModule, MAKEINTRESOURCE(IDD_EDITOR),
                     nppData._nppHandle, dlgProc, 0);
}
