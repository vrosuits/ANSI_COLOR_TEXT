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

// PluginDefinition.h - plugin metadata, lifecycle, and menu commands.
#pragma once

#include "PluginInterface.h"
#include "Settings.h"
#include "AnsiColor.h"

const TCHAR NPP_PLUGIN_NAME[] = TEXT("ANSI Color Text");

// Number of menu commands exposed by the plugin (see commandMenuInit).
const int nbFunc = 11;

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
void applyColor();         // open the editor dialog to color the selection
void insertReset();        // insert an ESC[0m reset at the caret
void importAnsi();         // open an ANSI file into Notepad++
void exportAnsi();         // write the current buffer to an ANSI file
void showSettings();       // open the settings dialog (SettingsDialog.cpp)
void showAbout();

// Shared state (also used by SettingsDialog.cpp / EditorDialog.cpp).
extern NppData         nppData;
extern FuncItem        funcItem[nbFunc];
extern PluginSettings  g_settings;
extern HINSTANCE       g_hModule;

void ensureSettings();   // lazily load settings from the plugin config dir
void persistSettings();  // save g_settings back to the .ini
void reRenderCurrent();  // re-render the active document with current settings

// Wrap the current Scintilla selection with the SGR sequence for `a` plus a
// reset, inserting real escape bytes. Called by the editor dialog.
void applyAttrToSelection(const ansi::Attr& a);
