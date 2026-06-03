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

// DllMain.cpp - the DLL entry point and the exports Notepad++ calls to load a
// plugin. See https://npp-user-manual.org/docs/plugin-communication/ for the
// plugin interface contract.
#include "PluginDefinition.h"

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID /*lpReserved*/) {
    switch (reasonForCall) {
        case DLL_PROCESS_ATTACH:
            pluginInit(hModule);
            commandMenuInit();
            break;
        case DLL_PROCESS_DETACH:
            commandMenuCleanUp();
            pluginCleanUp();
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData) {
    nppData = notpadPlusData;
}

extern "C" __declspec(dllexport) const TCHAR* getName() {
    return NPP_PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF) {
    *nbF = nbFunc;
    return funcItem;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notifyCode) {
    handleNotification(notifyCode);
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT /*Message*/, WPARAM /*wParam*/, LPARAM /*lParam*/) {
    return TRUE;
}

// Notepad++ is Unicode-only; plugins must report Unicode support.
extern "C" __declspec(dllexport) BOOL isUnicode() {
    return TRUE;
}
