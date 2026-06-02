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

// Result of applying: how many distinct styles were needed and whether the
// style budget was exhausted (true-color-heavy art can exceed maxStyle).
struct StyleResult {
    int  stylesUsed = 0;
    bool budgetExceeded = false;
};

// Push `doc` into `editor`: set the clean text, allocate de-duplicated styles,
// style every span, and mark strike ranges. Blink/conceal are recorded by the
// parser but not rendered here (Scintilla cannot natively).
StyleResult applyToEditor(const ParsedDocument& doc, IEditor& editor, const StylerConfig& cfg);

} // namespace ansi
