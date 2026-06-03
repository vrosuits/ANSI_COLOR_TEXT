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

#include "Settings.h"
#include "AiClient.h"

#include <string>
#include <vector>

namespace {

const TCHAR* kFile    = TEXT("AnsiColorText.ini");
const TCHAR* kSection = TEXT("ansicolor");

std::wstring iniPath(const TCHAR* configDir) {
    std::wstring p = configDir;
    if (!p.empty() && p.back() != L'\\' && p.back() != L'/') p += L'\\';
    p += kFile;
    return p;
}

std::wstring toW(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string toN(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

int getInt(const std::wstring& path, const TCHAR* key, int dflt) {
    return static_cast<int>(::GetPrivateProfileInt(kSection, key, dflt, path.c_str()));
}

void putInt(const std::wstring& path, const TCHAR* key, int value) {
    TCHAR buf[32];
    wsprintf(buf, TEXT("%d"), value);
    ::WritePrivateProfileString(kSection, key, buf, path.c_str());
}

std::string getStr(const std::wstring& path, const TCHAR* key, const std::string& dflt) {
    TCHAR buf[4096];
    ::GetPrivateProfileString(kSection, key, toW(dflt).c_str(), buf, ARRAYSIZE(buf), path.c_str());
    return toN(buf);
}

void putStr(const std::wstring& path, const TCHAR* key, const std::string& value) {
    ::WritePrivateProfileString(kSection, key, toW(value).c_str(), path.c_str());
}

// Per-provider ini key, e.g. "ai2model".
std::wstring aiKey(int i, const TCHAR* suffix) {
    TCHAR b[64];
    wsprintf(b, TEXT("ai%d%s"), i, suffix);
    return b;
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
    s.defaultView         = getInt(path, TEXT("defaultView"),         s.defaultView);

    // Guard against nonsensical persisted values.
    if (s.tabWidth < 1)        s.tabWidth = 1;
    if (s.blinkIntervalMs < 50) s.blinkIntervalMs = 50;
    if (s.animDelayMs < 1)     s.animDelayMs = 1;
    if (s.animChunkBytes < 1)  s.animChunkBytes = 1;
    if (s.defaultView < 0 || s.defaultView > 2) s.defaultView = 0;

    // AI providers: start from the built-in defaults, then override from the ini.
    std::vector<ansi::AiProvider> defs = ansi::defaultProviders();
    s.ai.resize(defs.size());
    for (size_t i = 0; i < defs.size(); ++i) {
        int idx = static_cast<int>(i);
        s.ai[i].baseUrl   = getStr(path, aiKey(idx, TEXT("base")).c_str(),  defs[i].baseUrl);
        s.ai[i].model     = getStr(path, aiKey(idx, TEXT("model")).c_str(), defs[i].model);
        s.ai[i].apiKeyEnc = getStr(path, aiKey(idx, TEXT("key")).c_str(),   std::string());
    }
    s.aiSelected = getInt(path, TEXT("aiSelected"), 0);
    if (s.aiSelected < 0 || s.aiSelected >= static_cast<int>(s.ai.size())) s.aiSelected = 0;
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
    putInt(path, TEXT("defaultView"),         s.defaultView);

    putInt(path, TEXT("aiSelected"), s.aiSelected);
    for (size_t i = 0; i < s.ai.size(); ++i) {
        int idx = static_cast<int>(i);
        putStr(path, aiKey(idx, TEXT("base")).c_str(),  s.ai[i].baseUrl);
        putStr(path, aiKey(idx, TEXT("model")).c_str(), s.ai[i].model);
        putStr(path, aiKey(idx, TEXT("key")).c_str(),   s.ai[i].apiKeyEnc);
    }
}
