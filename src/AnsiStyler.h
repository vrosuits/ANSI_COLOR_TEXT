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

// AnsiStyler.h - applies a ParsedDocument's spans onto an editor.
//
// The styler is deliberately decoupled from Scintilla: it talks to an abstract
// IEditor, so its logic (color resolution, inverse swap, style dedup + cap) can
// be unit-tested on a host. The plugin implements IEditor against Scintilla
// (see ScintillaEditor in the plugin glue).
#pragma once

#include "AnsiColor.h"

namespace ansi {

// The editor operations the styler needs. Coordinates are byte offsets into the
// text previously passed to setText().
class IEditor {
public:
    virtual ~IEditor() = default;

    // Replace the whole document with escape-free text.
    virtual void setText(const std::string& text) = 0;

    // Define style number `style` with the given resolved colors and flags.
    // An unset Color means "leave the editor default for that channel".
    // Only AF_Bold / AF_Italic / AF_Underline are meaningful here.
    virtual void defineStyle(int style, const Color& fore, const Color& back, uint32_t flags) = 0;

    // Apply style number `style` to [start, start+length) of the document.
    virtual void styleRange(size_t start, size_t length, int style) = 0;

    // Mark [start, start+length) with a strikethrough indicator (Scintilla
    // styles have no strike attribute, so this is drawn separately).
    virtual void strikeRange(size_t start, size_t length) = 0;
};

struct StylerConfig {
    int   baseStyle = 64;   // first style number to allocate (keep clear of
    int   maxStyle  = 255;  // Notepad++/lexer styles; STYLE_MAX is 255)
    Color defaultFore{0, 0, 0};         // editor default fg, used for inverse swap
    Color defaultBack{255, 255, 255};   // editor default bg, used for inverse swap
};

// A run that carries the SGR blink attribute. Scintilla cannot blink natively,
// so the styler allocates two styles per blink appearance: `onStyle` (normal)
// and `offStyle` (foreground == background, i.e. invisible). The plugin's timer
// toggles which one is applied to animate the blink. Coordinates are byte
// offsets into the document text.
struct BlinkRange {
    size_t start    = 0;
    size_t length   = 0;
    int    onStyle  = 0;
    int    offStyle = 0;
};

// Result of applying: how many distinct styles were needed, whether the style
// budget was exhausted (true-color-heavy art can exceed maxStyle), and the
// ranges that should blink.
struct StyleResult {
    int                     stylesUsed = 0;
    bool                    budgetExceeded = false;
    std::vector<BlinkRange> blinkRanges;
};

// Push `doc` into `editor`: set the clean text, allocate de-duplicated styles,
// style every span, and mark strike ranges. Blink/conceal are recorded by the
// parser but not rendered here (Scintilla cannot natively).
StyleResult applyToEditor(const ParsedDocument& doc, IEditor& editor, const StylerConfig& cfg);

} // namespace ansi
