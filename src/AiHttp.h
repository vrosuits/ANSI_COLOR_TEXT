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

// AiHttp.h - Windows transport + key crypto for the AI client (WinHTTP POST and
// DPAPI protect/unprotect). Win32-only; the request/response shaping is in the
// platform-independent AiClient.
#pragma once

#include "AiClient.h"
#include <string>

// POST req.body to req.url with req.headers. On success returns true and fills
// outBody (raw response) and outStatus (HTTP status). On a transport failure
// returns false and fills outErr.
bool httpPostJson(const ansi::AiRequest& req, std::string& outBody,
                  long& outStatus, std::string& outErr);

// DPAPI (CryptProtectData) round-trip, with the encrypted blob base64-encoded so
// it can live in a text .ini. Empty input yields an empty string.
std::string dpapiProtect(const std::string& plain);
std::string dpapiUnprotect(const std::string& base64Blob);
