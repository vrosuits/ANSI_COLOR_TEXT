#include "Settings.h"

#include <string>

namespace {

const TCHAR* kFile    = TEXT("AnsiColorText.ini");
const TCHAR* kSection = TEXT("ansicolor");

std::wstring iniPath(const TCHAR* configDir) {
    std::wstring p = configDir;
    if (!p.empty() && p.back() != L'\\' && p.back() != L'/') p += L'\\';
    p += kFile;
    return p;
}

int getInt(const std::wstring& path, const TCHAR* key, int dflt) {
    return static_cast<int>(::GetPrivateProfileInt(kSection, key, dflt, path.c_str()));
}

void putInt(const std::wstring& path, const TCHAR* key, int value) {
    TCHAR buf[32];
    wsprintf(buf, TEXT("%d"), value);
    ::WritePrivateProfileString(kSection, key, buf, path.c_str());
}

} // namespace

void loadSettings(const TCHAR* configDir, PluginSettings& s) {
    std::wstring path = iniPath(configDir);
    s.maxWidth            = getInt(path, TEXT("maxWidth"),            s.maxWidth);
    s.screenHeight        = getInt(path, TEXT("screenHeight"),        s.screenHeight);
    s.tabWidth            = getInt(path, TEXT("tabWidth"),            s.tabWidth);
    s.eraseUsesBackground = getInt(path, TEXT("eraseUsesBackground"), s.eraseUsesBackground ? 1 : 0) != 0;
    s.scrollRegionEnabled = getInt(path, TEXT("scrollRegionEnabled"), s.scrollRegionEnabled ? 1 : 0) != 0;
    s.blackBackground     = getInt(path, TEXT("blackBackground"),     s.blackBackground ? 1 : 0) != 0;
    s.blinkIntervalMs     = getInt(path, TEXT("blinkIntervalMs"),     s.blinkIntervalMs);
    s.animDelayMs         = getInt(path, TEXT("animDelayMs"),         s.animDelayMs);
    s.animChunkBytes      = getInt(path, TEXT("animChunkBytes"),      s.animChunkBytes);

    // Guard against nonsensical persisted values.
    if (s.tabWidth < 1)        s.tabWidth = 1;
    if (s.blinkIntervalMs < 50) s.blinkIntervalMs = 50;
    if (s.animDelayMs < 1)     s.animDelayMs = 1;
    if (s.animChunkBytes < 1)  s.animChunkBytes = 1;
}

void saveSettings(const TCHAR* configDir, const PluginSettings& s) {
    std::wstring path = iniPath(configDir);
    putInt(path, TEXT("maxWidth"),            s.maxWidth);
    putInt(path, TEXT("screenHeight"),        s.screenHeight);
    putInt(path, TEXT("tabWidth"),            s.tabWidth);
    putInt(path, TEXT("eraseUsesBackground"), s.eraseUsesBackground ? 1 : 0);
    putInt(path, TEXT("scrollRegionEnabled"), s.scrollRegionEnabled ? 1 : 0);
    putInt(path, TEXT("blackBackground"),     s.blackBackground ? 1 : 0);
    putInt(path, TEXT("blinkIntervalMs"),     s.blinkIntervalMs);
    putInt(path, TEXT("animDelayMs"),         s.animDelayMs);
    putInt(path, TEXT("animChunkBytes"),      s.animChunkBytes);
}
