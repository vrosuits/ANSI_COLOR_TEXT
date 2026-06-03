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

// AiJson.h - a small, dependency-free JSON reader plus the string helpers the
// AI client needs (JSON string escaping and base64). Platform-independent and
// host-tested; used to build request bodies and pull text out of AI responses.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ansi {

// A parsed JSON value. Numbers are kept as their raw text so integers round-trip
// exactly; callers usually only need strings, objects, and arrays.
struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    std::string text;  // String value, or raw number text.
    std::vector<JsonValue> items;                       // Array
    std::vector<std::pair<std::string, JsonValue>> members; // Object

    bool isString() const { return type == Type::String; }
    bool isObject() const { return type == Type::Object; }
    bool isArray()  const { return type == Type::Array; }

    // Object lookup by key (nullptr if absent or not an object).
    const JsonValue* find(const std::string& key) const;
    // Array element by index (nullptr if out of range or not an array).
    const JsonValue* at(size_t i) const;
    // String value or "" if this is not a string.
    const std::string& asString() const;
};

// Parse `in` into `out`. Returns false on malformed input.
bool parseJson(const std::string& in, JsonValue& out);

// Escape a UTF-8 string for inclusion as a JSON string value (no surrounding
// quotes added).
std::string jsonEscape(const std::string& s);

// Standard base64 (with padding).
std::string base64Encode(const std::string& bytes);
std::string base64Decode(const std::string& b64);

} // namespace ansi
