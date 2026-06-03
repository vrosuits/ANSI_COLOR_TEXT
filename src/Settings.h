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

// Settings.h - user-configurable plugin settings, persisted to an .ini file in
// the Notepad++ plugin config directory. The defaults match the renderer's and
// plugin's standard behavior; the config dialog edits these.
#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include "AnsiScreen.h"

// Per-provider AI settings. baseUrl/model override the built-in defaults;
// apiKeyEnc is the DPAPI-encrypted, base64-encoded key as persisted (empty = no
// key). The provider name/kind come from ansi::defaultProviders() by index.
struct AiProviderSettings {
    std::string baseUrl;
    std::string model;
    std::string apiKeyEnc;
    std::string systemPrompt;  // custom system prompt; empty = built-in default
};

// All plugin-tunable options in one place.
struct PluginSettings {
    // --- rendering (mapped to ansi::ScreenConfig) ---
    int  maxWidth            = 80;   // wrap column (0 = no wrap)
    int  screenHeight        = 0;    // 0 = unbounded; >0 enables scroll regions
    int  tabWidth            = 8;
    bool eraseUsesBackground = true;
    bool scrollRegionEnabled = true;

    // --- appearance ---
    bool blackBackground     = true; // default background for rendering

    // --- animation playback ---
    int  blinkIntervalMs     = 500;  // blink toggle period
    int  animDelayMs         = 40;   // delay between animation frames
    int  animChunkBytes      = 8;    // bytes revealed per animation frame

    // --- view ---
    // Which view to show automatically when an ANSI file is imported or
    // AI-generated: 0 = Color (rendered), 1 = Raw escape codes, 2 = Raw \e symbols.
    int  defaultView         = 0;

    // --- AI generation ---
    int aiSelected = 0;                       // index into ansi::defaultProviders()
    std::vector<AiProviderSettings> ai;       // parallel to defaultProviders()

    // Build the renderer config from these settings.
    ansi::ScreenConfig toScreenConfig() const {
        ansi::ScreenConfig c;
        c.maxWidth            = maxWidth < 0 ? 0 : static_cast<size_t>(maxWidth);
        c.height              = screenHeight < 0 ? 0 : static_cast<size_t>(screenHeight);
        c.tabWidth            = tabWidth;
        c.eraseUsesBackground = eraseUsesBackground;
        c.scrollRegionEnabled = scrollRegionEnabled;
        return c;
    }
};

// Load settings from <configDir>\AnsiColorText.ini (missing keys keep defaults).
void loadSettings(const TCHAR* configDir, PluginSettings& s);

// Persist settings to <configDir>\AnsiColorText.ini.
void saveSettings(const TCHAR* configDir, const PluginSettings& s);
