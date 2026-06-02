#include "AnsiStyler.h"

#include <utility> // std::swap
#include <vector>

namespace ansi {
namespace {

// Resolve a span's attribute into the (fore, back, flags) actually handed to a
// Scintilla style: applies the inverse swap and drops non-style flags.
struct Resolved {
    Color    fore;
    Color    back;
    uint32_t flags; // AF_Bold | AF_Italic | AF_Underline only

    bool operator==(const Resolved& o) const {
        return fore == o.fore && back == o.back && flags == o.flags;
    }
};

Resolved resolve(const Attr& a, const StylerConfig& cfg) {
    Color fore = a.fore;
    Color back = a.back;

    if (a.flags & AF_Inverse) {
        // Swap fg/bg. Fill in editor defaults first so the swap is visible even
        // when one channel was left at the default.
        if (!fore.set) fore = cfg.defaultFore;
        if (!back.set) back = cfg.defaultBack;
        std::swap(fore, back);
    }

    uint32_t flags = a.flags & (AF_Bold | AF_Italic | AF_Underline);
    return Resolved{fore, back, flags};
}

} // namespace

StyleResult applyToEditor(const ParsedDocument& doc, IEditor& editor, const StylerConfig& cfg) {
    editor.setText(doc.text);

    StyleResult result;

    // De-duplicate resolved attributes into a compact style table so that, e.g.,
    // a file using 8 colors needs only 8 styles even across thousands of spans.
    std::vector<Resolved> table;
    table.reserve(64);

    const int capacity = cfg.maxStyle - cfg.baseStyle + 1;

    for (const Span& span : doc.spans) {
        if (span.length == 0) continue;

        Resolved r = resolve(span.attr, cfg);

        // Find or assign a style slot for this resolved appearance.
        int slot = -1;
        for (size_t k = 0; k < table.size(); ++k) {
            if (table[k] == r) { slot = static_cast<int>(k); break; }
        }
        if (slot < 0) {
            if (static_cast<int>(table.size()) < capacity) {
                slot = static_cast<int>(table.size());
                table.push_back(r);
                editor.defineStyle(cfg.baseStyle + slot, r.fore, r.back, r.flags);
            } else {
                // Out of style numbers: reuse slot 0 so text still renders,
                // and flag the degradation to the caller.
                result.budgetExceeded = true;
                slot = 0;
            }
        }

        editor.styleRange(span.start, span.length, cfg.baseStyle + slot);

        if (span.attr.flags & AF_Strike)
            editor.strikeRange(span.start, span.length);
    }

    result.stylesUsed = static_cast<int>(table.size());
    return result;
}

} // namespace ansi
