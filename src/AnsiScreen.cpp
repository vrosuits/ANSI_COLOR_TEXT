#include "AnsiScreen.h"
#include "AnsiSgr.h"

#include <vector>

namespace ansi {
namespace {

constexpr char ESC = '\x1b';

// One screen cell. `written` distinguishes a real (possibly blank) glyph from
// an empty cell the cursor merely skipped over, which lets us trim trailing
// emptiness and render skipped cells as plain spaces.
struct Cell {
    char ch      = ' ';
    Attr attr;
    bool written = false;
};

// A dynamically growing 2D grid + cursor. Coordinates are 0-based.
class Screen {
public:
    explicit Screen(size_t maxWidth) : maxWidth_(maxWidth) {}

    void setAttr(const Attr& a) { attr_ = a; }

    void carriageReturn() { col_ = 0; }

    void lineFeed() { ++row_; col_ = 0; }

    void moveUp(int n)    { row_ = (static_cast<size_t>(n) > row_) ? 0 : row_ - n; }
    void moveDown(int n)  { row_ += n; }
    void moveLeft(int n)  { col_ = (static_cast<size_t>(n) > col_) ? 0 : col_ - n; }
    void moveRight(int n) { col_ += n; }

    void columnTo(int n1based) { col_ = n1based > 0 ? static_cast<size_t>(n1based - 1) : 0; }
    void rowTo(int n1based)    { row_ = n1based > 0 ? static_cast<size_t>(n1based - 1) : 0; }
    void moveTo(int r1based, int c1based) { rowTo(r1based); columnTo(c1based); }

    void nextLine(int n) { row_ += n; col_ = 0; }
    void prevLine(int n) { moveUp(n); col_ = 0; }

    void saveCursor()    { savedRow_ = row_; savedCol_ = col_; }
    void restoreCursor() { row_ = savedRow_; col_ = savedCol_; }

    // Write one printable byte at the cursor, then advance (wrapping if a
    // maxWidth is set).
    void put(char c) {
        Cell& cell = at(row_, col_);
        cell.ch = c;
        cell.attr = attr_;
        cell.written = true;
        ++col_;
        if (maxWidth_ && col_ >= maxWidth_) { col_ = 0; ++row_; }
    }

    // ED (erase in display).
    void eraseDisplay(int mode) {
        if (mode == 2 || mode == 3) { grid_.clear(); return; }
        if (mode == 0) {                          // cursor -> end
            eraseLine(0);
            for (size_t r = row_ + 1; r < grid_.size(); ++r) grid_[r].clear();
        } else if (mode == 1) {                   // start -> cursor
            for (size_t r = 0; r < row_ && r < grid_.size(); ++r) grid_[r].clear();
            eraseLine(1);
        }
    }

    // EL (erase in line).
    void eraseLine(int mode) {
        if (row_ >= grid_.size()) return;
        std::vector<Cell>& line = grid_[row_];
        if (mode == 2) { line.clear(); return; }
        if (mode == 0) {                          // cursor -> EOL
            for (size_t c = col_; c < line.size(); ++c) line[c] = Cell{};
        } else if (mode == 1) {                   // BOL -> cursor
            for (size_t c = 0; c <= col_ && c < line.size(); ++c) line[c] = Cell{};
        }
    }

    // Flatten the grid into clean text + contiguous styled spans.
    ParsedDocument flatten() const {
        ParsedDocument doc;

        size_t lastRow = 0;
        bool   anyRow  = false;
        for (size_t r = 0; r < grid_.size(); ++r) {
            if (rowWidth(r) > 0) { lastRow = r; anyRow = true; }
        }
        if (!anyRow) return doc;

        Attr   curAttr;          // running attr for span building (starts default)
        size_t spanStart = 0;
        auto pushSpan = [&]() {
            size_t end = doc.text.size();
            if (end > spanStart)
                doc.spans.push_back(Span{spanStart, end - spanStart, curAttr});
            spanStart = end;
        };
        auto emit = [&](char ch, const Attr& a) {
            if (a != curAttr) { pushSpan(); curAttr = a; }
            doc.text.push_back(ch);
        };

        for (size_t r = 0; r <= lastRow; ++r) {
            size_t w = rowWidth(r);
            for (size_t c = 0; c < w; ++c) {
                const Cell& cell = (r < grid_.size() && c < grid_[r].size())
                                       ? grid_[r][c] : kBlank;
                emit(cell.written ? cell.ch : ' ', cell.attr);
            }
            if (r != lastRow) emit('\n', Attr{}); // newlines carry default attr
        }
        pushSpan();
        return doc;
    }

private:
    static const Cell kBlank;

    Cell& at(size_t r, size_t c) {
        if (r >= grid_.size()) grid_.resize(r + 1);
        std::vector<Cell>& line = grid_[r];
        if (c >= line.size()) line.resize(c + 1);
        return line[c];
    }

    // Index one past the last written cell in row r (0 if the row is empty).
    size_t rowWidth(size_t r) const {
        if (r >= grid_.size()) return 0;
        const std::vector<Cell>& line = grid_[r];
        size_t w = 0;
        for (size_t c = 0; c < line.size(); ++c)
            if (line[c].written) w = c + 1;
        return w;
    }

    std::vector<std::vector<Cell>> grid_;
    Attr   attr_;
    size_t row_ = 0, col_ = 0;
    size_t savedRow_ = 0, savedCol_ = 0;
    size_t maxWidth_ = 0;
};

const Cell Screen::kBlank{};

// First parameter of a CSI body with a per-op default (cursor moves default 1,
// erases default 0).
int firstParam(const std::string& body, int dflt) {
    std::vector<int> p = sgrParams(body);
    if (p.size() == 1 && p[0] == 0 && body.empty()) return dflt;
    return p[0];
}

} // namespace

ParsedDocument renderScreen(const std::string& input, size_t maxWidth) {
    Screen scr(maxWidth);
    Attr   attr;

    const size_t n = input.size();
    for (size_t i = 0; i < n;) {
        char c = input[i];

        if (c == ESC && i + 1 < n && input[i + 1] == '[') {
            size_t j = i + 2;
            while (j < n) {
                unsigned char fb = static_cast<unsigned char>(input[j]);
                if (fb >= 0x40 && fb <= 0x7E) break; // final byte
                ++j;
            }
            if (j < n) {
                char final = input[j];
                std::string body = input.substr(i + 2, j - (i + 2));
                switch (final) {
                    case 'm': applySgr(sgrParams(body), attr); scr.setAttr(attr); break;
                    case 'A': scr.moveUp(firstParam(body, 1));    break;
                    case 'B': scr.moveDown(firstParam(body, 1));  break;
                    case 'C': scr.moveRight(firstParam(body, 1)); break;
                    case 'D': scr.moveLeft(firstParam(body, 1));  break;
                    case 'E': scr.nextLine(firstParam(body, 1));  break;
                    case 'F': scr.prevLine(firstParam(body, 1));  break;
                    case 'G': scr.columnTo(firstParam(body, 1));  break;
                    case 'd': scr.rowTo(firstParam(body, 1));     break;
                    case 'H':
                    case 'f': {
                        std::vector<int> p = sgrParams(body);
                        int r = p.size() > 0 && p[0] > 0 ? p[0] : 1;
                        int cc = p.size() > 1 && p[1] > 0 ? p[1] : 1;
                        scr.moveTo(r, cc);
                        break;
                    }
                    case 'J': scr.eraseDisplay(firstParam(body, 0)); break;
                    case 'K': scr.eraseLine(firstParam(body, 0));    break;
                    case 's': scr.saveCursor();    break;
                    case 'u': scr.restoreCursor(); break;
                    default: break; // ignore other CSI
                }
                i = j + 1;
                continue;
            }
            // Unterminated escape: fall through and treat ESC as literal.
        }

        if (c == '\n')      scr.lineFeed();
        else if (c == '\r') scr.carriageReturn();
        else                scr.put(c);
        ++i;
    }

    return scr.flatten();
}

} // namespace ansi
