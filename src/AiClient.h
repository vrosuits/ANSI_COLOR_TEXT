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

// AiClient.h - provider-agnostic request building and response parsing for AI
// ANSI-art generation. Platform-independent and host-tested; the actual HTTP
// call (WinHTTP) lives in the plugin layer.
#pragma once

#include <string>
#include <vector>

namespace ansi {

// Wire shape of a provider's chat API. OpenAI, Grok, DeepSeek, and Ollama all
// speak the OpenAI-compatible shape; Anthropic is the one variant.
enum class AiKind { OpenAICompatible, Anthropic };

struct AiProvider {
    std::string name;     // display name, e.g. "OpenAI"
    AiKind      kind = AiKind::OpenAICompatible;
    std::string baseUrl;  // e.g. "https://api.openai.com"
    std::string apiKey;
    std::string model;    // e.g. "gpt-4o"
};

// An HTTP request ready for the transport layer to POST.
struct AiRequest {
    std::string              url;      // full request URL
    std::vector<std::string> headers;  // each "Name: value"
    std::string              body;     // JSON request body
};

// Outcome of parsing a provider response.
struct AiResult {
    bool        ok = false;
    std::string text;   // generated content (on success)
    std::string error;  // human-readable error (on failure)
};

// The five built-in providers with sensible default base URLs and models
// (api keys empty). Ollama defaults to a local server.
std::vector<AiProvider> defaultProviders();

// The system prompt that instructs the model to emit raw ANSI art using SGR
// colors/attributes and Unicode box-drawing/shading. When `animation` is true it
// asks for a cursor-addressed ANSI animation suitable for timed playback.
std::string aiSystemPrompt(bool animation);

// Build the POST request for `provider` from a system + user prompt.
AiRequest buildRequest(const AiProvider& provider,
                       const std::string& system,
                       const std::string& user);

// Parse a provider's HTTP response body (with its status code) into text/error.
AiResult parseResponse(AiKind kind, const std::string& body, long httpStatus);

// Strip a single Markdown code fence (```...```), if the whole text is wrapped
// in one. Models frequently wrap output even when told not to.
std::string stripCodeFence(const std::string& text);

// Convert textual escape introducers a model may emit (\e, \033, \x1b, )
// into real ESC (0x1b) bytes. Real ESC bytes are left untouched.
std::string normalizeEscapes(const std::string& text);

} // namespace ansi
