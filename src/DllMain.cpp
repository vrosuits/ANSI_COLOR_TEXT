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

extern "C" __declspec(dllexport) void beNotified(SCNotification* /*notifyCode*/) {
    // No document/UI notifications are handled yet.
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT /*Message*/, WPARAM /*wParam*/, LPARAM /*lParam*/) {
    return TRUE;
}

// Notepad++ is Unicode-only; plugins must report Unicode support.
extern "C" __declspec(dllexport) BOOL isUnicode() {
    return TRUE;
}
