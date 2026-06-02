// PluginDefinition.h - plugin metadata, lifecycle, and menu commands.
#pragma once

#include "PluginInterface.h"

const TCHAR NPP_PLUGIN_NAME[] = TEXT("ANSI Color Text");

// Number of menu commands exposed by the plugin (see commandMenuInit).
const int nbFunc = 4;

// Called from setInfo(): stash the Notepad++/Scintilla handles.
void pluginInit(HANDLE hModule);
void pluginCleanUp();
void commandMenuInit();
void commandMenuCleanUp();
bool setCommand(size_t index, const TCHAR* cmdName, PFUNCPLUGIN pFunc, ShortcutKey* sk, bool checkOnInit);

// Menu command handlers.
void renderAnsi();        // parse + colorize the current document
void toggleBackground();  // switch black <-> white background and re-render
void showAbout();
void editFlash();         // toggle simulated blink (placeholder for now)

extern NppData   nppData;
extern FuncItem  funcItem[nbFunc];
extern bool      g_blackBackground;
