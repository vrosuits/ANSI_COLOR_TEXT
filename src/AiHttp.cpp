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

#include "AiHttp.h"
#include "AiJson.h"   // base64 for the DPAPI blob

#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>

#include <string>

namespace {

std::wstring toWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

// RAII for the WinHTTP handles.
struct Handle {
    HINTERNET h = nullptr;
    ~Handle() { if (h) ::WinHttpCloseHandle(h); }
};

} // namespace

bool httpPostJson(const ansi::AiRequest& req, std::string& outBody,
                  long& outStatus, std::string& outErr) {
    outBody.clear();
    outStatus = 0;

    std::wstring wurl = toWide(req.url);

    URL_COMPONENTS uc = {0};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {0}, path[2048] = {0};
    uc.lpszHostName = host;     uc.dwHostNameLength = ARRAYSIZE(host);
    uc.lpszUrlPath  = path;     uc.dwUrlPathLength  = ARRAYSIZE(path);
    if (!::WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
        outErr = "Could not parse the provider URL.";
        return false;
    }
    bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    Handle session;
    session.h = ::WinHttpOpen(L"ANSI-Color-Text/1.0",
                              WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.h) { outErr = "WinHttpOpen failed."; return false; }
    // AI calls can be slow: 60s resolve/connect/send/receive.
    ::WinHttpSetTimeouts(session.h, 60000, 60000, 60000, 60000);

    Handle connect;
    connect.h = ::WinHttpConnect(session.h, host, uc.nPort, 0);
    if (!connect.h) { outErr = "WinHttpConnect failed."; return false; }

    Handle request;
    request.h = ::WinHttpOpenRequest(connect.h, L"POST", path, nullptr,
                                     WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     https ? WINHTTP_FLAG_SECURE : 0);
    if (!request.h) { outErr = "WinHttpOpenRequest failed."; return false; }

    // Join headers with CRLF.
    std::wstring headers;
    for (const std::string& h : req.headers) {
        headers += toWide(h);
        headers += L"\r\n";
    }

    BOOL sent = ::WinHttpSendRequest(
        request.h,
        headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        headers.empty() ? 0 : static_cast<DWORD>(headers.size()),
        const_cast<char*>(req.body.data()), static_cast<DWORD>(req.body.size()),
        static_cast<DWORD>(req.body.size()), 0);
    if (!sent) { outErr = "Request failed to send (check the base URL / network)."; return false; }

    if (!::WinHttpReceiveResponse(request.h, nullptr)) {
        outErr = "No response from the provider.";
        return false;
    }

    // Status code.
    DWORD status = 0, len = sizeof(status);
    ::WinHttpQueryHeaders(request.h,
                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &status, &len,
                          WINHTTP_NO_HEADER_INDEX);
    outStatus = static_cast<long>(status);

    // Body.
    for (;;) {
        DWORD avail = 0;
        if (!::WinHttpQueryDataAvailable(request.h, &avail)) break;
        if (avail == 0) break;
        std::string chunk(avail, '\0');
        DWORD read = 0;
        if (!::WinHttpReadData(request.h, &chunk[0], avail, &read)) break;
        chunk.resize(read);
        outBody += chunk;
        if (read == 0) break;
    }
    return true;
}

// --- DPAPI ---------------------------------------------------------------

std::string dpapiProtect(const std::string& plain) {
    if (plain.empty()) return std::string();
    DATA_BLOB in;
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));
    in.cbData = static_cast<DWORD>(plain.size());
    DATA_BLOB out = {0};
    if (!::CryptProtectData(&in, L"ansi-color-text api key", nullptr, nullptr,
                            nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return std::string();
    }
    std::string blob(reinterpret_cast<char*>(out.pbData), out.cbData);
    ::LocalFree(out.pbData);
    return ansi::base64Encode(blob);
}

std::string dpapiUnprotect(const std::string& base64Blob) {
    if (base64Blob.empty()) return std::string();
    std::string blob = ansi::base64Decode(base64Blob);
    if (blob.empty()) return std::string();
    DATA_BLOB in;
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(blob.data()));
    in.cbData = static_cast<DWORD>(blob.size());
    DATA_BLOB out = {0};
    if (!::CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                              CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return std::string();
    }
    std::string plain(reinterpret_cast<char*>(out.pbData), out.cbData);
    ::LocalFree(out.pbData);
    return plain;
}
