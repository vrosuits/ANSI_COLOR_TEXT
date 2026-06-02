// Settings.h - user-configurable plugin settings, persisted to an .ini file in
// the Notepad++ plugin config directory. The defaults match the renderer's and
// plugin's standard behavior; the config dialog edits these.
#pragma once

#include <windows.h>
#include "AnsiScreen.h"

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
