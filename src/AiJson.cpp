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

#include "AiJson.h"

#include <cstdint>

namespace ansi {

namespace {
const std::string kEmpty;

// Recursive-descent JSON parser over a cursor into `s`.
struct Parser {
    const std::string& s;
    size_t i = 0;
    bool ok = true;

    explicit Parser(const std::string& src) : s(src) {}

    void skipWs() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i;
            else break;
        }
    }

    bool atEnd() { return i >= s.size(); }
    char peek() { return i < s.size() ? s[i] : '\0'; }

    // Append the UTF-8 encoding of a Unicode code point.
    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    int hex4() {
        if (i + 4 > s.size()) { ok = false; return 0; }
        int v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else { ok = false; return 0; }
        }
        return v;
    }

    std::string parseString() {
        std::string out;
        ++i; // opening quote
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return out;
            if (c == '\\') {
                if (i >= s.size()) { ok = false; return out; }
                char e = s[i++];
                switch (e) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u': {
                        uint32_t cp = static_cast<uint32_t>(hex4());
                        if (!ok) return out;
                        // Combine a UTF-16 surrogate pair if present.
                        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() &&
                            s[i] == '\\' && s[i + 1] == 'u') {
                            i += 2;
                            uint32_t lo = static_cast<uint32_t>(hex4());
                            if (ok && lo >= 0xDC00 && lo <= 0xDFFF)
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: ok = false; return out;
                }
            } else {
                out += c;
            }
        }
        ok = false; // unterminated
        return out;
    }

    JsonValue parseValue() {
        skipWs();
        JsonValue v;
        if (atEnd()) { ok = false; return v; }
        char c = peek();
        switch (c) {
            case '"':
                v.type = JsonValue::Type::String;
                v.text = parseString();
                return v;
            case '{': return parseObject();
            case '[': return parseArray();
            case 't':
                if (s.compare(i, 4, "true") == 0) { i += 4; v.type = JsonValue::Type::Bool; v.boolean = true; }
                else ok = false;
                return v;
            case 'f':
                if (s.compare(i, 5, "false") == 0) { i += 5; v.type = JsonValue::Type::Bool; v.boolean = false; }
                else ok = false;
                return v;
            case 'n':
                if (s.compare(i, 4, "null") == 0) { i += 4; v.type = JsonValue::Type::Null; }
                else ok = false;
                return v;
            default:
                return parseNumber();
        }
    }

    JsonValue parseNumber() {
        JsonValue v;
        size_t start = i;
        if (peek() == '-') ++i;
        while (i < s.size()) {
            char c = s[i];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                c == '+' || c == '-') ++i;
            else break;
        }
        if (i == start) { ok = false; return v; }
        v.type = JsonValue::Type::Number;
        v.text = s.substr(start, i - start);
        return v;
    }

    JsonValue parseArray() {
        JsonValue v;
        v.type = JsonValue::Type::Array;
        ++i; // [
        skipWs();
        if (peek() == ']') { ++i; return v; }
        while (true) {
            v.items.push_back(parseValue());
            if (!ok) return v;
            skipWs();
            char c = peek();
            if (c == ',') { ++i; continue; }
            if (c == ']') { ++i; break; }
            ok = false; return v;
        }
        return v;
    }

    JsonValue parseObject() {
        JsonValue v;
        v.type = JsonValue::Type::Object;
        ++i; // {
        skipWs();
        if (peek() == '}') { ++i; return v; }
        while (true) {
            skipWs();
            if (peek() != '"') { ok = false; return v; }
            std::string key = parseString();
            if (!ok) return v;
            skipWs();
            if (peek() != ':') { ok = false; return v; }
            ++i;
            JsonValue val = parseValue();
            if (!ok) return v;
            v.members.emplace_back(std::move(key), std::move(val));
            skipWs();
            char c = peek();
            if (c == ',') { ++i; continue; }
            if (c == '}') { ++i; break; }
            ok = false; return v;
        }
        return v;
    }
};
} // namespace

const JsonValue* JsonValue::find(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& m : members)
        if (m.first == key) return &m.second;
    return nullptr;
}

const JsonValue* JsonValue::at(size_t idx) const {
    if (type != Type::Array || idx >= items.size()) return nullptr;
    return &items[idx];
}

const std::string& JsonValue::asString() const {
    return type == Type::String ? text : kEmpty;
}

bool parseJson(const std::string& in, JsonValue& out) {
    Parser p(in);
    out = p.parseValue();
    if (!p.ok) return false;
    p.skipWs();
    return p.atEnd();
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[(c >> 4) & 0xF];
                    out += hex[c & 0xF];
                } else {
                    out += static_cast<char>(c); // pass UTF-8 bytes through
                }
        }
    }
    return out;
}

std::string base64Encode(const std::string& in) {
    static const char* tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= in.size()) {
        uint32_t n = (static_cast<unsigned char>(in[i]) << 16) |
                     (static_cast<unsigned char>(in[i + 1]) << 8) |
                     (static_cast<unsigned char>(in[i + 2]));
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += tbl[(n >> 6) & 63];
        out += tbl[n & 63];
        i += 3;
    }
    size_t rem = in.size() - i;
    if (rem == 1) {
        uint32_t n = static_cast<unsigned char>(in[i]) << 16;
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += "==";
    } else if (rem == 2) {
        uint32_t n = (static_cast<unsigned char>(in[i]) << 16) |
                     (static_cast<unsigned char>(in[i + 1]) << 8);
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += tbl[(n >> 6) & 63];
        out += '=';
    }
    return out;
}

std::string base64Decode(const std::string& in) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string out;
    int buf = 0, bits = 0;
    for (char c : in) {
        if (c == '=') break;
        int v = val(c);
        if (v < 0) continue; // skip whitespace/newlines
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((buf >> bits) & 0xFF);
        }
    }
    return out;
}

} // namespace ansi
