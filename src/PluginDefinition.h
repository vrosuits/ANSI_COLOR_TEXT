// PluginDefinition.h - plugin metadata, lifecycle, and menu commands.
#pragma once

#include "PluginInterface.h"
#include "Settings.h"

const TCHAR NPP_PLUGIN_NAME[] = TEXT("ANSI Color Text");

// Number of menu commands exposed by the plugin (see commandMenuInit).
const int nbFunc = 7;

// Lifecycle.
void pluginInit(HANDLE hModule);
void pluginCleanUp();
void commandMenuInit();
void commandMenuCleanUp();
bool setCommand(size_t index, const TCHAR* cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey* sk, bool checkOnInit);

// Menu command handlers.
void renderAnsi();         // parse + colorize the current document
void playAnimation();      // play the document as a timed ANSI animation
void stopAnimationCmd();   // stop a running animation
void toggleBackground();   // switch black <-> white background and re-render
void editFlash();          // pause/resume blink animation
void showSettings();       // open the settings dialog (SettingsDialog.cpp)
void showAbout();

// Shared state (also used by SettingsDialog.cpp).
extern NppData         nppData;
extern FuncItem        funcItem[nbFunc];
extern PluginSettings  g_settings;
extern HINSTANCE       g_hModule;

void ensureSettings();   // lazily load settings from the plugin config dir
void persistSettings();  // save g_settings back to the .ini
void reRenderCurrent();  // re-render the active document with current settings
