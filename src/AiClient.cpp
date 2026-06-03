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

#include "AiClient.h"
#include "AiJson.h"

namespace ansi {
namespace {

std::string trimTrailingSlash(const std::string& s) {
    std::string r = s;
    while (!r.empty() && (r.back() == '/' || r.back() == '\\')) r.pop_back();
    return r;
}

// JSON object member: "key":"<escaped value>"
std::string jstr(const std::string& key, const std::string& value) {
    return "\"" + key + "\":\"" + jsonEscape(value) + "\"";
}

} // namespace

std::vector<AiProvider> defaultProviders() {
    return {
        {"OpenAI",   AiKind::OpenAICompatible, "https://api.openai.com",  "", "gpt-4o"},
        {"Anthropic", AiKind::Anthropic,       "https://api.anthropic.com", "", "claude-sonnet-4-5"},
        {"Grok",     AiKind::OpenAICompatible, "https://api.x.ai",        "", "grok-2-latest"},
        {"DeepSeek", AiKind::OpenAICompatible, "https://api.deepseek.com", "", "deepseek-chat"},
        {"Ollama",   AiKind::OpenAICompatible, "http://localhost:11434",  "", "llama3.1"},
    };
}

std::string aiSystemPrompt(bool animation) {
    std::string p =
        "You are an expert ANSI/SGR text artist. You output raw ANSI text only - "
        "no Markdown, no code fences, no explanations, no commentary.\n\n"
        "CRITICAL - the escape character: every sequence begins with the ESC "
        "control character, byte value 0x1b. Emit the actual 0x1b byte. If your "
        "tokenizer cannot produce a raw control byte, write the three letters "
        "ESC immediately followed by '[' (e.g. ESC[31m) and nothing else - do NOT "
        "write \\e, \\033, \\x1b, or the word 'escape'. Example: ESC[31m sets red, "
        "ESC[0m resets.\n"
        "CRITICAL - all other characters: output every box-drawing, shading and "
        "Unicode glyph as its REAL character (real UTF-8 bytes). NEVER escape a "
        "character as \\xE2\\x95\\x90, \\u2550, &#9552; or any \\xNN / \\uNNNN form - "
        "write the actual glyph itself.\n"
        "Color: 16-color (30-37/90-97 fg, 40-47/100-107 bg), 256-color "
        "(ESC[38;5;Nm / ESC[48;5;Nm), and 24-bit true color "
        "(ESC[38;2;R;G;Bm / ESC[48;2;R;G;Bm).\n"
        "Attributes: bold ESC[1m, italic ESC[3m, underline ESC[4m, blink ESC[5m, "
        "inverse ESC[7m, strikethrough ESC[9m. Always reset with ESC[0m.\n"
        "Box drawing (Unicode, output the actual glyphs as UTF-8): single line "
        "U+2500-U+257F (corners U+250C U+2510 U+2514 U+2518, tees U+251C U+2524 "
        "U+252C U+2534 U+253C); double line U+2550 U+2551 with corners U+2554 "
        "U+2557 U+255A U+255D; rounded corners U+256D U+256E U+2570 U+256F; "
        "shading blocks light U+2591 medium U+2592 dark U+2593 full U+2588.\n"
        "Always finish styled runs with ESC[0m so colors do not bleed.\n";
    if (animation) {
        p +=
            "\nProduce an ANSI ANIMATION: clear the screen with ESC[2J then home "
            "with ESC[H, and use cursor positioning (ESC[row;colH) to redraw frames "
            "over time. Draw each new element before erasing the previous so no "
            "frame is blank. The output is replayed progressively, so order the "
            "byte stream as the animation should unfold.";
    } else {
        p += "\nProduce static ANSI art sized for an 80-column terminal.";
    }
    return p;
}

AiRequest buildRequest(const AiProvider& provider, const std::string& system,
                       const std::string& user) {
    AiRequest req;
    std::string base = trimTrailingSlash(provider.baseUrl);
    req.headers.emplace_back("Content-Type: application/json");

    if (provider.kind == AiKind::Anthropic) {
        req.url = base + "/v1/messages";
        req.headers.emplace_back("x-api-key: " + provider.apiKey);
        req.headers.emplace_back("anthropic-version: 2023-06-01");
        req.body = "{" + jstr("model", provider.model) +
                   ",\"max_tokens\":4096," + jstr("system", system) +
                   ",\"messages\":[{\"role\":\"user\",\"content\":\"" +
                   jsonEscape(user) + "\"}]}";
    } else {
        req.url = base + "/v1/chat/completions";
        req.headers.emplace_back("Authorization: Bearer " + provider.apiKey);
        req.body = "{" + jstr("model", provider.model) +
                   ",\"messages\":[{\"role\":\"system\",\"content\":\"" +
                   jsonEscape(system) +
                   "\"},{\"role\":\"user\",\"content\":\"" + jsonEscape(user) +
                   "\"}],\"temperature\":0.8,\"stream\":false}";
    }
    return req;
}

AiResult parseResponse(AiKind kind, const std::string& body, long httpStatus) {
    AiResult r;
    JsonValue root;
    if (!parseJson(body, root)) {
        r.error = "Provider returned a response that is not valid JSON.";
        return r;
    }

    // Both shapes report errors under "error" (Anthropic nests message there).
    if (const JsonValue* err = root.find("error")) {
        const JsonValue* msg = err->isObject() ? err->find("message") : nullptr;
        r.error = (msg && msg->isString()) ? msg->asString()
                                           : "Provider returned an error.";
        return r;
    }
    if (httpStatus >= 400) {
        r.error = "HTTP error " + std::to_string(httpStatus) + " from provider.";
        return r;
    }

    if (kind == AiKind::Anthropic) {
        if (const JsonValue* content = root.find("content")) {
            if (const JsonValue* first = content->at(0)) {
                if (const JsonValue* text = first->find("text")) {
                    if (text->isString()) { r.ok = true; r.text = text->asString(); return r; }
                }
            }
        }
    } else {
        if (const JsonValue* choices = root.find("choices")) {
            if (const JsonValue* first = choices->at(0)) {
                if (const JsonValue* msg = first->find("message")) {
                    if (const JsonValue* content = msg->find("content")) {
                        if (content->isString()) { r.ok = true; r.text = content->asString(); return r; }
                    }
                }
            }
        }
    }
    r.error = "No generated content found in the provider response.";
    return r;
}

std::string stripCodeFence(const std::string& text) {
    // Find first non-space.
    size_t s = text.find_first_not_of(" \t\r\n");
    if (s == std::string::npos) return text;
    if (text.compare(s, 3, "```") != 0) return text;

    // Skip the opening fence and any language tag up to the newline.
    size_t nl = text.find('\n', s);
    if (nl == std::string::npos) return text;
    size_t bodyStart = nl + 1;

    // Find the closing fence.
    size_t close = text.rfind("```");
    if (close == std::string::npos || close < bodyStart) return text;

    // Trim a single trailing newline before the closing fence.
    size_t bodyEnd = close;
    if (bodyEnd > bodyStart && text[bodyEnd - 1] == '\n') --bodyEnd;
    if (bodyEnd > bodyStart && text[bodyEnd - 1] == '\r') --bodyEnd;
    return text.substr(bodyStart, bodyEnd - bodyStart);
}

std::string normalizeEscapes(const std::string& text) {
    auto isHex = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            char n = text[i + 1];
            if (n == 'e' || n == 'E') { out += '\x1b'; i += 2; continue; }
            if ((n == 'x' || n == 'X') && i + 3 < text.size() &&
                text[i + 2] == '1' && (text[i + 3] == 'b' || text[i + 3] == 'B')) {
                out += '\x1b'; i += 4; continue;
            }
            if ((n == 'u' || n == 'U') && i + 5 < text.size() &&
                text[i + 2] == '0' && text[i + 3] == '0' && text[i + 4] == '1' &&
                (text[i + 5] == 'b' || text[i + 5] == 'B')) {
                out += '\x1b'; i += 6; continue;
            }
            if (n == '0' && i + 3 < text.size() && text[i + 2] == '3' && text[i + 3] == '3') {
                // octal \033, but not \0333... (next char must not extend it)
                bool more = (i + 4 < text.size()) && isHex(text[i + 4]) && text[i + 4] >= '0' && text[i + 4] <= '7';
                if (!more) { out += '\x1b'; i += 4; continue; }
            }
        }
        out += text[i];
        ++i;
    }
    return out;
}

} // namespace ansi
