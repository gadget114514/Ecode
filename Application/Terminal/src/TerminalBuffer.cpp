#include "../include/TerminalBuffer.h"
#include <algorithm>
#include <cassert>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static int clamp(int v, int lo, int hi) { return std::max(lo, std::min(v, hi)); }

// ---------------------------------------------------------------------------
// ctor
// ---------------------------------------------------------------------------
TerminalBuffer::TerminalBuffer(int columns, int rows) {
    resize(columns, rows);
    resetTabStops();
    // Default ANSI palette
    static const TermColor kPalette[16] = {
        TermColor::fromRgb( 12,  12,  12), TermColor::fromRgb(197,  15,  31),
        TermColor::fromRgb( 19, 161,  14), TermColor::fromRgb(193, 156,   0),
        TermColor::fromRgb(  0,  55, 218), TermColor::fromRgb(136,  23, 152),
        TermColor::fromRgb( 58, 150, 221), TermColor::fromRgb(204, 204, 204),
        TermColor::fromRgb(118, 118, 118), TermColor::fromRgb(231,  72,  86),
        TermColor::fromRgb( 22, 198,  12), TermColor::fromRgb(249, 241, 165),
        TermColor::fromRgb( 59, 120, 255), TermColor::fromRgb(180,   0, 158),
        TermColor::fromRgb( 97, 214, 214), TermColor::fromRgb(242, 242, 242),
    };
    for (int i = 0; i < 16; i++)
        palette_[i] = kPalette[i];
}

// ---------------------------------------------------------------------------
// resize
// ---------------------------------------------------------------------------
void TerminalBuffer::resize(int columns, int rows) {
    pendingWrap_     = false;
    const int oldColumns     = columns_;
    const int oldRows        = rows_;
    const int oldScrollTop   = scrollTop_;
    const int oldScrollBottom= scrollBottom_;

    columns_ = std::max(20, columns);
    rows_    = std::max(5,  rows);

    // preserve scroll region proportionally
    if (oldScrollTop == 0 && oldScrollBottom == oldRows - 1) {
        scrollTop_    = 0;
        scrollBottom_ = rows_ - 1;
    } else {
        scrollTop_    = clamp(oldScrollTop, 0, rows_ - 1);
        scrollBottom_ = clamp(oldScrollBottom + rows_ - oldRows, scrollTop_, rows_ - 1);
    }

    // resize alternate / main screens in-place
    resizeLines(mainScreen_,      oldColumns);
    resizeLines(alternateScreen_, oldColumns);

    // build new active screen from old content
    std::vector<Line> oldScreen = screen_;
    screen_.clear();
    screen_.reserve(rows_);

    const int keepRows = std::min(rows_, (int)oldScreen.size());
    const int start    = std::max(0, (int)oldScreen.size() - keepRows);
    for (int i = 0; i < keepRows; ++i) {
        Line line = oldScreen[start + i];
        line.resize(columns_);
        for (int c = oldColumns; c < columns_; ++c) line[c] = TerminalCell();
        for (int c = 0; c < columns_; ++c)
            if (line[c].text.empty() && !line[c].wideContinuation)
                line[c] = TerminalCell();
        normalizeWideCells(line);
        screen_.push_back(std::move(line));
    }
    // Count rows inserted at the top; shift cursor to stay at same content row
    const int insertCount = rows_ - (int)screen_.size();
    while ((int)screen_.size() < rows_)
        screen_.insert(screen_.begin(), blankLine());
    if (insertCount > 0) {
        cursorRow_         += insertCount;
        savedCursorRow_    += insertCount;
        savedCursorRowAlt_ += insertCount;
    } else {
        const int discardCount = (std::max)(0, (int)oldScreen.size() - rows_);
        cursorRow_         -= discardCount;
        savedCursorRow_    -= discardCount;
        savedCursorRowAlt_ -= discardCount;
    }

    clampCursor();

    savedCursorRow_    = clamp(savedCursorRow_,    0, rows_ - 1);
    savedCursorRowAlt_ = clamp(savedCursorRowAlt_, 0, rows_ - 1);

    // Resize tab stops if columns changed
    if (columns_ != (int)tabStops_.size()) {
        tabStops_.resize(columns_, false);
        for (int col = 8; col < columns_; col += 8)
            tabStops_[col] = true;
    }
}

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------
void TerminalBuffer::clearScreen() {
    pendingWrap_ = false;
    screen_.assign(rows_, blankLine());
    cursorRow_ = cursorColumn_ = 0;
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
}

void TerminalBuffer::clearAll() {
    history_.clear();
    clearScreen();
}

void TerminalBuffer::clearLine(int mode, const TerminalCell& attrs) {
    pendingWrap_ = false;
    if (screen_.empty()) return;
    Line& line = screen_[cursorRow_];
    int start = 0, end = columns_ - 1;
    if      (mode == 0) start = cursorColumn_;
    else if (mode == 1) end   = cursorColumn_;
    for (int c = start; c <= end && c < (int)line.size(); ++c)
        eraseCell(cursorRow_, c, attrs);
}

void TerminalBuffer::clearScreenMode(int mode, const TerminalCell& attrs) {
    pendingWrap_ = false;
    if (mode == 3) { history_.clear(); return; }
    if (mode == 2) { for (int r = 0; r < rows_; ++r) screen_[r] = blankLine(attrs); return; }
    if (mode == 0) {
        clearLine(0, attrs);
        for (int r = cursorRow_ + 1; r < rows_; ++r) screen_[r] = blankLine(attrs);
        return;
    }
    if (mode == 1) {
        for (int r = 0; r < cursorRow_; ++r) screen_[r] = blankLine(attrs);
        clearLine(1, attrs);
    }
}

// ---------------------------------------------------------------------------
// character output
// ---------------------------------------------------------------------------
void TerminalBuffer::putChar(wchar_t ch, const TerminalCell& attributes) {
    std::wstring s(1, ch);
    putText(s, attributes);
}

void TerminalBuffer::putText(const std::wstring& text, const TerminalCell& attributes) {
    if (text.empty()) return;

    const int width = characterWidth(text);

    // combining / zero-width: attach to preceding cell
    if (width == 0) {
        int targetColumn = cursorColumn_ - 1;
        if (targetColumn >= 0 && screen_[cursorRow_][targetColumn].wideContinuation)
            --targetColumn;
        if (targetColumn >= 0)
            screen_[cursorRow_][targetColumn].text += text;
        return;
    }

    if (pendingWrap_) {
        // mark current line as soft-wrapped before advancing
        if (!screen_.empty())
            screen_[cursorRow_][std::max(0, columns_ - 1)].softWrapped = true;
        carriageReturn();
        lineFeed();
        pendingWrap_ = false;
    }

    // ZWJ cluster: attach to preceding cell
    int prevCol = cursorColumn_ - 1;
    if (prevCol >= 0 && screen_[cursorRow_][prevCol].wideContinuation)
        --prevCol;
    if (prevCol >= 0 && !screen_[cursorRow_][prevCol].text.empty() &&
        screen_[cursorRow_][prevCol].text.back() == wchar_t(0x200d)) {
        screen_[cursorRow_][prevCol].text += text;
        return;
    }

    // autowrap: if character doesn't fit and DECAWM is set, wrap now.
    // When DECAWM is off the character at the right margin is overwritten in-place.
    if (autoWrapEnabled_ && cursorColumn_ + width > columns_) {
        if (!screen_.empty())
            screen_[cursorRow_][std::max(0, columns_ - 1)].softWrapped = true;
        carriageReturn();
        lineFeed();
    }

    TerminalCell cell = attributes;
    cell.text = text;
    cell.ch   = text[0];
    cell.wide = (width == 2);
    cell.wideContinuation = false;
    cell.softWrapped      = false;

    eraseCell(cursorRow_, cursorColumn_);
    if (width == 2 && cursorColumn_ + 1 < columns_)
        eraseCell(cursorRow_, cursorColumn_ + 1);

    screen_[cursorRow_][cursorColumn_] = cell;

    if (width == 2 && cursorColumn_ + 1 < columns_) {
        TerminalCell cont;
        cont.text.clear();
        cont.wideContinuation = true;
        screen_[cursorRow_][cursorColumn_ + 1] = cont;
    }

    cursorColumn_ += width;
    if (cursorColumn_ >= columns_) {
        cursorColumn_ = columns_ - 1;
        pendingWrap_  = autoWrapEnabled_;
    }
}

// ---------------------------------------------------------------------------
// cursor movement
// ---------------------------------------------------------------------------
void TerminalBuffer::carriageReturn()              { pendingWrap_ = false; cursorColumn_ = 0; }

void TerminalBuffer::lineFeed() {
    pendingWrap_ = false;
    if      (cursorRow_ == scrollBottom_) scrollUpLines(1);
    else if (cursorRow_ < rows_ - 1)      ++cursorRow_;
}

void TerminalBuffer::reverseIndex() {
    pendingWrap_ = false;
    if      (cursorRow_ == scrollTop_) scrollDownLines(1);
    else if (cursorRow_ > 0)           --cursorRow_;
}

void TerminalBuffer::backspace() {
    pendingWrap_ = false;
    if (cursorColumn_ <= 0) return;
    --cursorColumn_;
    if (cursorColumn_ > 0 && screen_[cursorRow_][cursorColumn_].wideContinuation)
        --cursorColumn_;
}

void TerminalBuffer::tab() {
    pendingWrap_ = false;
    for (int col = cursorColumn_ + 1; col < columns_; ++col) {
        if ((size_t)col < tabStops_.size() && tabStops_[col]) {
            cursorColumn_ = col;
            return;
        }
    }
    cursorColumn_ = columns_ - 1;
}

void TerminalBuffer::setTabStop() {
    if ((size_t)cursorColumn_ < tabStops_.size())
        tabStops_[cursorColumn_] = true;
}

void TerminalBuffer::clearTabStop() {
    if ((size_t)cursorColumn_ < tabStops_.size())
        tabStops_[cursorColumn_] = false;
}

void TerminalBuffer::clearAllTabStops() {
    for (size_t i = 0; i < tabStops_.size(); ++i)
        tabStops_[i] = false;
}

void TerminalBuffer::resetTabStops() {
    tabStops_.assign(columns_, false);
    for (int col = 8; col < columns_; col += 8)
        tabStops_[col] = true;
}

void TerminalBuffer::moveCursorRelative(int rowDelta, int columnDelta) {
    pendingWrap_ = false;
    cursorRow_    += rowDelta;
    cursorColumn_ += columnDelta;
    clampCursor();
}

void TerminalBuffer::moveCursorTo(int row, int column) {
    pendingWrap_  = false;
    cursorRow_    = originMode_ ? scrollTop_ + row : row;
    cursorColumn_ = column;
    clampCursor();
}

void TerminalBuffer::moveCursorColumn(int column) {
    pendingWrap_ = false; cursorColumn_ = column; clampCursor();
}

void TerminalBuffer::moveCursorRow(int row) {
    pendingWrap_ = false;
    cursorRow_   = originMode_ ? scrollTop_ + row : row;
    clampCursor();
}

void TerminalBuffer::moveCursorNextLine(int count) {
    pendingWrap_  = false;
    const int oldRow = cursorRow_;
    cursorRow_   += std::max(1, count);
    cursorColumn_ = 0;
    // When starting inside the scroll region, stop at the bottom margin.
    if (oldRow >= scrollTop_ && oldRow <= scrollBottom_ && cursorRow_ > scrollBottom_)
        cursorRow_ = scrollBottom_;
    clampCursor();
}

void TerminalBuffer::moveCursorPreviousLine(int count) {
    pendingWrap_  = false;
    const int oldRow = cursorRow_;
    cursorRow_   -= std::max(1, count);
    cursorColumn_ = 0;
    // When starting inside the scroll region, stop at the top margin.
    if (oldRow >= scrollTop_ && oldRow <= scrollBottom_ && cursorRow_ < scrollTop_)
        cursorRow_ = scrollTop_;
    clampCursor();
}

void TerminalBuffer::saveCursor() {
    if (alternateScreenActive_) {
        savedCursorRowAlt_     = cursorRow_;
        savedCursorColumnAlt_  = cursorColumn_;
        savedCursorVisibleAlt_ = cursorVisible_;
        savedPendingWrapAlt_   = pendingWrap_;
    } else {
        savedCursorRow_        = cursorRow_;
        savedCursorColumn_     = cursorColumn_;
        savedCursorVisible_    = cursorVisible_;
        savedPendingWrap_      = pendingWrap_;
    }
}

void TerminalBuffer::restoreCursor() {
    if (alternateScreenActive_) {
        cursorRow_     = savedCursorRowAlt_;
        cursorColumn_  = savedCursorColumnAlt_;
        cursorVisible_ = savedCursorVisibleAlt_;
        pendingWrap_   = savedPendingWrapAlt_;
    } else {
        cursorRow_     = savedCursorRow_;
        cursorColumn_  = savedCursorColumn_;
        cursorVisible_ = savedCursorVisible_;
        pendingWrap_   = savedPendingWrap_;
    }
    clampCursor();
}

// ---------------------------------------------------------------------------
// character editing
// ---------------------------------------------------------------------------
void TerminalBuffer::eraseCharacters(int count, const TerminalCell& attrs) {
    pendingWrap_ = false;
    const int end = std::min(columns_ - 1, cursorColumn_ + std::max(1, count) - 1);
    for (int c = cursorColumn_; c <= end; ++c) eraseCell(cursorRow_, c, attrs);
}

void TerminalBuffer::insertCharacters(int count) {
    pendingWrap_ = false;
    Line& line = screen_[cursorRow_];
    const int amount = std::min(std::max(1, count), columns_ - cursorColumn_);
    for (int c = columns_ - 1; c >= cursorColumn_ + amount; --c) line[c] = line[c - amount];
    for (int c = cursorColumn_; c < cursorColumn_ + amount; ++c) line[c] = TerminalCell();
    normalizeWideCells(line);
}

void TerminalBuffer::deleteCharacters(int count) {
    pendingWrap_ = false;
    Line& line = screen_[cursorRow_];
    const int amount = std::min(std::max(1, count), columns_ - cursorColumn_);
    for (int c = cursorColumn_; c < columns_ - amount; ++c) line[c] = line[c + amount];
    for (int c = columns_ - amount; c < columns_; ++c)      line[c] = TerminalCell();
    normalizeWideCells(line);
}

void TerminalBuffer::repeatPreviousChar(int count) {
    if (cursorColumn_ <= 0 || screen_.empty()) return;
    int prevCol = cursorColumn_ - 1;
    if (screen_[cursorRow_][prevCol].wideContinuation && prevCol > 0) --prevCol;
    const TerminalCell prev = screen_[cursorRow_][prevCol];
    for (int i = 0; i < std::max(1, count) && cursorColumn_ < columns_; ++i)
        putText(prev.text, prev);
}

// ---------------------------------------------------------------------------
// line editing
// ---------------------------------------------------------------------------
void TerminalBuffer::insertLines(int count) {
    pendingWrap_ = false;
    if (cursorRow_ < scrollTop_ || cursorRow_ > scrollBottom_) return;
    const int amount = std::min(std::max(1, count), scrollBottom_ - cursorRow_ + 1);
    // rotate で末尾 amount 行をカーソル行に持ち上げ、そこを blank に（O(n) 1回）
    std::rotate(screen_.begin() + cursorRow_,
                screen_.begin() + scrollBottom_ + 1 - amount,
                screen_.begin() + scrollBottom_ + 1);
    for (int r = cursorRow_; r < cursorRow_ + amount; ++r)
        screen_[r] = blankLine();
}

void TerminalBuffer::deleteLines(int count) {
    pendingWrap_ = false;
    if (cursorRow_ < scrollTop_ || cursorRow_ > scrollBottom_) return;
    const int amount = std::min(std::max(1, count), scrollBottom_ - cursorRow_ + 1);
    // rotate でカーソル行から amount 行上へ詰め、末尾を blank に（O(n) 1回）
    std::rotate(screen_.begin() + cursorRow_,
                screen_.begin() + cursorRow_ + amount,
                screen_.begin() + scrollBottom_ + 1);
    for (int r = scrollBottom_ - amount + 1; r <= scrollBottom_; ++r)
        screen_[r] = blankLine();
}

void TerminalBuffer::scrollUpLines(int count) {
    const int amount = std::min(std::max(1, count), scrollBottom_ - scrollTop_ + 1);
    for (int i = 0; i < amount; ++i) {
        if (scrollTop_ == 0 && scrollBottom_ == rows_ - 1) {
            history_.push_back(screen_.front());
            screen_.erase(screen_.begin());
            screen_.push_back(blankLine());
            trimHistory();
        } else {
            screen_.erase(screen_.begin() + scrollTop_);
            screen_.insert(screen_.begin() + scrollBottom_, blankLine());
        }
    }
}

void TerminalBuffer::scrollDownLines(int count) {
    const int amount = std::min(std::max(1, count), scrollBottom_ - scrollTop_ + 1);
    for (int i = 0; i < amount; ++i) {
        screen_.erase(screen_.begin() + scrollBottom_);
        screen_.insert(screen_.begin() + scrollTop_, blankLine());
    }
}

// ---------------------------------------------------------------------------
// scroll region / modes
// ---------------------------------------------------------------------------
void TerminalBuffer::setScrollRegion(int top, int bottom) {
    pendingWrap_  = false;
    scrollTop_    = clamp(top,    0, rows_ - 1);
    scrollBottom_ = clamp(bottom, scrollTop_, rows_ - 1);
    cursorRow_    = originMode_ ? scrollTop_ : 0;
    cursorColumn_ = 0;
    clampCursor();
}

void TerminalBuffer::resetScrollRegion() {
    pendingWrap_  = false;
    scrollTop_    = 0;
    scrollBottom_ = rows_ - 1;
}

void TerminalBuffer::softReset() {
    // DECSTR: Soft Terminal Reset — resets modes but not screen
    pendingWrap_ = false;
    // scroll region
    scrollTop_    = 0;
    scrollBottom_ = rows_ - 1;
    // modes
    originMode_     = false;
    autoWrapEnabled_ = true;
    cursorVisible_  = true;
    cursorBlink_    = true;
    cursorShape_    = CursorShape::Block;
    cursorFilled_   = true;
    cursorWidthPct_  = 100;
    cursorHeightPct_ = 100;
    reverseVideo_   = false;
    mouseTrackingMode_ = 0;
    sgrMouseEnabled_ = false;
    focusEventReportingEnabled_ = false;
    applicationCursorMode_ = false;
    bracketedPasteEnabled_ = false;
    // cursor home
    cursorRow_    = 0;
    cursorColumn_ = 0;
    savedCursorRow_    = 0;
    savedCursorColumn_ = 0;
    // tab stops
    resetTabStops();
}

void TerminalBuffer::setOriginMode(bool enabled) {
    pendingWrap_ = false;
    originMode_  = enabled;
    moveCursorTo(0, 0);
}

bool TerminalBuffer::originMode()      const { return originMode_; }

void TerminalBuffer::setAutoWrapEnabled(bool enabled) {
    autoWrapEnabled_ = enabled;
    if (!autoWrapEnabled_) pendingWrap_ = false;
}

bool TerminalBuffer::autoWrapEnabled() const { return autoWrapEnabled_; }

// ---------------------------------------------------------------------------
// accessors
// ---------------------------------------------------------------------------
int TerminalBuffer::columns()          const { return columns_; }
int TerminalBuffer::rows()             const { return rows_; }
int TerminalBuffer::cursorRow()        const { return cursorRow_; }
int TerminalBuffer::cursorColumn()     const { return std::min(cursorColumn_, columns_ - 1); }
int TerminalBuffer::cursorColumnRaw()  const { return cursorColumn_; }
int TerminalBuffer::historyLineCount() const { return (int)history_.size(); }
int TerminalBuffer::totalLineCount()   const { return (int)(history_.size() + screen_.size()); }

const TerminalBuffer::Line& TerminalBuffer::lineAt(int logicalRow) const {
    if (logicalRow < (int)history_.size()) return history_[logicalRow];
    return screen_[logicalRow - (int)history_.size()];
}

int  TerminalBuffer::scrollTop()    const { return scrollTop_; }
int  TerminalBuffer::scrollBottom() const { return scrollBottom_; }
bool TerminalBuffer::hasScrollRegion() const {
    return scrollTop_ > 0 || scrollBottom_ < rows_ - 1;
}

// ---------------------------------------------------------------------------
// feature flags (mostly forwarded to renderer / view)
// ---------------------------------------------------------------------------
void TerminalBuffer::setBracketedPasteEnabled(bool v)      { bracketedPasteEnabled_ = v; }
bool TerminalBuffer::bracketedPasteEnabled()         const { return bracketedPasteEnabled_; }
void TerminalBuffer::setSyncOutputEnabled(bool v)          { syncOutputEnabled_ = v; }
bool TerminalBuffer::syncOutputEnabled()             const { return syncOutputEnabled_; }
void TerminalBuffer::setMouseTrackingMode(int mode)        { mouseTrackingMode_ = mode; }
int  TerminalBuffer::mouseTrackingMode()             const { return mouseTrackingMode_; }
void TerminalBuffer::setSgrMouseEnabled(bool v)            { sgrMouseEnabled_ = v; }
bool TerminalBuffer::sgrMouseEnabled()               const { return sgrMouseEnabled_; }
void TerminalBuffer::setApplicationCursorMode(bool v)      { applicationCursorMode_ = v; }
bool TerminalBuffer::applicationCursorMode()         const { return applicationCursorMode_; }
void TerminalBuffer::setFocusEventReportingEnabled(bool v) { focusEventReportingEnabled_ = v; }
bool TerminalBuffer::focusEventReportingEnabled()    const { return focusEventReportingEnabled_; }
void TerminalBuffer::setModifyOtherKeysMode(int m)         { modifyOtherKeysMode_ = std::max(0,m); }
int  TerminalBuffer::modifyOtherKeysMode()           const { return modifyOtherKeysMode_; }
void TerminalBuffer::setKittyKeyboardFlags(int f)          { kittyKeyboardFlags_ = std::max(0,f); }
int  TerminalBuffer::kittyKeyboardFlags()            const { return kittyKeyboardFlags_; }
bool TerminalBuffer::kittyKeyboardEnabled()          const { return kittyKeyboardFlags_ != 0; }
void TerminalBuffer::setCellSize(int w, int h)             { cellWidth_ = std::max(1,w); cellHeight_ = std::max(1,h); }
int  TerminalBuffer::cellWidth()                     const { return cellWidth_; }
int  TerminalBuffer::cellHeight()                    const { return cellHeight_; }
void TerminalBuffer::setCursorVisible(bool v)              { cursorVisible_ = v; }
bool TerminalBuffer::cursorVisible()                 const { return cursorVisible_; }
void TerminalBuffer::setCursorBlink(bool v)                { cursorBlink_ = v; }
bool TerminalBuffer::cursorBlink()                   const { return cursorBlink_; }
void TerminalBuffer::setCursorShape(CursorShape s)         { cursorShape_ = s; }
TerminalBuffer::CursorShape TerminalBuffer::cursorShape()  const { return cursorShape_; }
void TerminalBuffer::setCursorFilled(bool v)               { cursorFilled_ = v; }
bool TerminalBuffer::cursorFilled()                  const { return cursorFilled_; }
void TerminalBuffer::setCursorScale(int wPct, int hPct)   { cursorWidthPct_ = std::clamp(wPct, 0, 100); cursorHeightPct_ = std::clamp(hPct, 0, 100); }
int  TerminalBuffer::cursorWidthPct()               const { return cursorWidthPct_; }
int  TerminalBuffer::cursorHeightPct()              const { return cursorHeightPct_; }

// ---------------------------------------------------------------------------
// OSC 4/10/11/12 colour overrides
// ---------------------------------------------------------------------------
void TerminalBuffer::setDefaultFgColor(const TermColor& c)  { defaultFg_ = c; }
TermColor TerminalBuffer::defaultFgColor()             const { return defaultFg_; }
void TerminalBuffer::setDefaultBgColor(const TermColor& c)  { defaultBg_ = c; }
TermColor TerminalBuffer::defaultBgColor()             const { return defaultBg_; }
void TerminalBuffer::setCursorColor(const TermColor& c)     { cursorColor_ = c; }
void TerminalBuffer::resetCursorColor()                     { cursorColor_ = TermColor(); }
TermColor TerminalBuffer::cursorColor()                const { return cursorColor_; }
void TerminalBuffer::setReverseVideo(bool v)                 { reverseVideo_ = v; }
bool TerminalBuffer::reverseVideo()                    const { return reverseVideo_; }
void TerminalBuffer::setPaletteColor(int i, const TermColor& c) { if (i >= 0 && i < 16) palette_[i] = c; }
TermColor TerminalBuffer::paletteColor(int i)          const { return (i >= 0 && i < 16) ? palette_[i] : TermColor(); }

// ---------------------------------------------------------------------------
// alternate screen
// ---------------------------------------------------------------------------
void TerminalBuffer::useAlternateScreen(bool enabled) {
    if (alternateScreenActive_ == enabled) return;

    if (enabled) {
        mainHistory_       = history_;
        mainScreen_        = screen_;
        mainCursorRow_     = cursorRow_;
        mainCursorColumn_  = cursorColumn_;
        mainCursorVisible_ = cursorVisible_;  // カーソル表示状態を保存
        alternateScreen_.assign(rows_, blankLine());
        screen_ = alternateScreen_;
        history_.clear();
        pendingWrap_ = false;
        cursorRow_    = originMode_ ? scrollTop_ : 0;
        cursorColumn_ = 0;
        scrollTop_    = 0;
        scrollBottom_ = rows_ - 1;
    } else {
        alternateScreen_  = screen_;
        history_          = mainHistory_;
        screen_           = mainScreen_.empty() ? screen_ : mainScreen_;
        pendingWrap_      = false;
        cursorRow_        = mainCursorRow_;
        cursorColumn_     = mainCursorColumn_;
        cursorVisible_    = mainCursorVisible_;  // カーソル表示状態を復元
        scrollTop_        = 0;
        scrollBottom_     = rows_ - 1;
    }
    alternateScreenActive_ = enabled;
    clampCursor();
}

bool TerminalBuffer::alternateScreenActive() const { return alternateScreenActive_; }

void TerminalBuffer::savePrivateMode(int mode) {
    switch (mode) {
    case 1:    savedPrivateMode1_     = applicationCursorMode_; break;
    case 25:   savedPrivateMode25_    = cursorVisible_;         break;
    case 1049:
    case 1047:
    case 47:   savedPrivateMode1049_  = alternateScreenActive_; break;
    case 2026: savedPrivateMode2026_  = cursorFilled_;          break;
    }
}

void TerminalBuffer::restorePrivateMode(int mode) {
    switch (mode) {
    case 1:    setApplicationCursorMode(savedPrivateMode1_); break;
    case 25:   setCursorVisible(savedPrivateMode25_);         break;
    case 1049:
    case 1047:
    case 47:   useAlternateScreen(savedPrivateMode1049_); break;
    case 2026: setCursorFilled(savedPrivateMode2026_); break;
    }
}

// ---------------------------------------------------------------------------
// private helpers
// ---------------------------------------------------------------------------
TerminalBuffer::Line TerminalBuffer::blankLine(const TerminalCell& attrs) const {
    Line line(columns_, attrs);
    for (auto& cell : line) {
        cell.ch = L' ';
        cell.text = L" ";
        cell.wide = false;
        cell.wideContinuation = false;
        cell.softWrapped = false;
    }
    return line;
}

// Unicode character width — combined from termdock + standard wcwidth tables
int TerminalBuffer::characterWidth(wchar_t ch) {
    const unsigned u = (unsigned)ch;

    // Zero-width combining / non-printing ranges
    if (u == 0x00ad
        || (u >= 0x0300 && u <= 0x036f)
        || (u >= 0x0483 && u <= 0x0489)
        || (u >= 0x0591 && u <= 0x05bd)
        || u == 0x05bf || (u >= 0x05c1 && u <= 0x05c2)
        || (u >= 0x05c4 && u <= 0x05c5) || u == 0x05c7
        || (u >= 0x0610 && u <= 0x061a)
        || (u >= 0x064b && u <= 0x065f) || u == 0x0670
        || (u >= 0x06d6 && u <= 0x06dc) || (u >= 0x06df && u <= 0x06e4)
        || (u >= 0x06e7 && u <= 0x06e8) || (u >= 0x06ea && u <= 0x06ed)
        || u == 0x0711 || (u >= 0x0730 && u <= 0x074a)
        || (u >= 0x07a6 && u <= 0x07b0) || (u >= 0x07eb && u <= 0x07f3)
        || (u >= 0x0816 && u <= 0x0823) || (u >= 0x081b && u <= 0x082d)
        || (u >= 0x0900 && u <= 0x0903) || (u >= 0x093a && u <= 0x093c)
        || (u >= 0x0941 && u <= 0x0948) || u == 0x094d
        || (u >= 0x1ab0 && u <= 0x1ace)
        || (u >= 0x1b00 && u <= 0x1b03)
        || (u >= 0x1dc0 && u <= 0x1dff)
        || (u >= 0x200b && u <= 0x200f)
        || (u >= 0x202a && u <= 0x202e)
        || (u >= 0x2060 && u <= 0x206f)
        || (u >= 0x20d0 && u <= 0x20ff)
        || (u >= 0xfe00 && u <= 0xfe0f)
        || (u >= 0xfe20 && u <= 0xfe2f))
        return 0;

    // Double-width (CJK, full-width, emoji)
    if ((u >= 0x1100 && u <= 0x115f)
        || u == 0x2329 || u == 0x232a
        || (u >= 0x2e80 && u <= 0xa4cf)
        || (u >= 0xac00 && u <= 0xd7a3)
        || (u >= 0xf900 && u <= 0xfaff)
        || (u >= 0xfe10 && u <= 0xfe19)
        || (u >= 0xfe30 && u <= 0xfe6f)
        || (u >= 0xff00 && u <= 0xff60)
        || (u >= 0xffe0 && u <= 0xffe6)
        || (u >= 0xd800 && u <= 0xdfff)    // surrogate pairs (emoji etc.)
        || (u >= 0x1f300 && u <= 0x1f64f)
        || (u >= 0x1f900 && u <= 0x1f9ff))
        return 2;

    return 1;
}

int TerminalBuffer::characterWidth(const std::wstring& text) {
    int w = 0;
    for (wchar_t c : text) w = std::max(w, characterWidth(c));
    return w;
}

void TerminalBuffer::normalizeWideCells(Line& line) {
    for (int c = 0; c < (int)line.size(); ++c) {
        TerminalCell& cell = line[c];
        if (cell.wideContinuation) {
            const bool valid = c > 0 && line[c-1].wide && !line[c-1].text.empty();
            if (!valid) cell = TerminalCell();
            continue;
        }
        if (cell.wide) {
            const bool hasCont = (c + 1 < (int)line.size()) && line[c+1].wideContinuation;
            if (!hasCont) { cell = TerminalCell(); continue; }
            if (cell.text.empty()) { cell = TerminalCell(); line[c+1] = TerminalCell(); }
            continue;
        }
        if (c + 1 < (int)line.size() && line[c+1].wideContinuation)
            line[c+1] = TerminalCell();
    }
}

void TerminalBuffer::resizeLines(std::vector<Line>& lines, int oldColumns) {
    std::vector<Line> oldScreen = lines;
    lines.clear();
    lines.reserve(rows_);

    const int keepRows = std::min(rows_, (int)oldScreen.size());
    const int start    = std::max(0, (int)oldScreen.size() - keepRows);
    for (int i = 0; i < keepRows; ++i) {
        Line line = oldScreen[start + i];
        line.resize(columns_);
        for (int c = oldColumns; c < columns_; ++c) line[c] = TerminalCell();
        for (int c = 0; c < columns_; ++c)
            if (line[c].text.empty() && !line[c].wideContinuation)
                line[c] = TerminalCell();
        normalizeWideCells(line);
        lines.push_back(std::move(line));
    }
    while ((int)lines.size() < rows_)
        lines.insert(lines.begin(), blankLine());
}

void TerminalBuffer::eraseCell(int row, int column, const TerminalCell& attrs) {
    if (row < 0 || row >= (int)screen_.size()) return;
    if (column < 0 || column >= (int)screen_[row].size()) return;
    Line& line = screen_[row];
    auto makeBlank = [&]() {
        TerminalCell b = attrs;
        b.ch = L' '; b.text = L" ";
        b.wide = false; b.wideContinuation = false; b.softWrapped = false;
        return b;
    };
    if (line[column].wideContinuation && column > 0) line[column-1] = makeBlank();
    if (line[column].wide && column + 1 < (int)line.size()) line[column+1] = makeBlank();
    line[column] = makeBlank();
}

// ---------------------------------------------------------------------------
// OSC 1337 inline image — place at cursor
// ---------------------------------------------------------------------------
void TerminalBuffer::placeImage(uint64_t imageId, int widthCells, int heightCells) {
    if (imageId == 0 || widthCells < 1 || heightCells < 1) return;
    if (screen_.empty()) return;

    // Handle pending wrap before image
    if (pendingWrap_) {
        if (!screen_.empty())
            screen_[cursorRow_][std::max(0, columns_ - 1)].softWrapped = true;
        carriageReturn();
        lineFeed();
        pendingWrap_ = false;
    }

    int startCol = cursorColumn_;
    int startRow = cursorRow_;

    // Clamp to screen
    widthCells  = std::min(widthCells,  columns_ - startCol);
    if (widthCells < 1) return;

    // Determine how many rows we need
    int rowsNeeded = heightCells;
    int rowsAvail = rows_ - startRow;
    if (rowsNeeded > rowsAvail) {
        // Scroll as needed
        int extra = rowsNeeded - rowsAvail;
        for (int i = 0; i < extra; ++i) {
            lineFeed();
            startRow = cursorRow_; // cursorRow_ may have changed due to scroll
        }
    }

    // Adjust height to available screen rows
    int maxHeight = rows_ - startRow;
    heightCells = std::min(heightCells, maxHeight);
    if (heightCells < 1) return;

    uint8_t idx = 0;
    for (int r = 0; r < heightCells; ++r) {
        int row = startRow + r;
        for (int c = 0; c < widthCells; ++c) {
            int col = startCol + c;
            TerminalCell cell;
            if (r == 0 && c == 0) {
                // Base cell
                cell.imageId = imageId;
                cell.imageWidthCells = (int16_t)widthCells;
                cell.imageHeightCells = (int16_t)heightCells;
                cell.imagePlaceholderIndex = 0;
            } else {
                // Continuation cell
                cell.imageId = imageId;
                cell.imagePlaceholderIndex = ++idx;
            }
            screen_[row][col] = cell;
        }
    }

    // Advance cursor to the row below the image
    cursorRow_ = startRow + heightCells;
    cursorColumn_ = 0;
    if (cursorRow_ >= rows_) {
        cursorRow_ = rows_ - 1;
        lineFeed(); // triggers scroll if at bottom
    }
}

// ---------------------------------------------------------------------------
// OSC 1337 inline image — clear from all buffers
// ---------------------------------------------------------------------------
void TerminalBuffer::clearImage(uint64_t imageId) {
    if (imageId == 0) return;

    auto clearImageInLines = [&](std::vector<Line>& lines) {
        for (auto& line : lines) {
            for (auto& cell : line) {
                if (cell.imageId == imageId) {
                    cell = TerminalCell();
                }
            }
        }
    };

    clearImageInLines(screen_);
    clearImageInLines(history_);
    clearImageInLines(mainScreen_);
    clearImageInLines(mainHistory_);
    clearImageInLines(alternateScreen_);
}

void TerminalBuffer::fillRect(int top, int left, int bottom, int right, wchar_t ch, const TerminalCell& attributes) {
    top    = clamp(top,    0, rows_    - 1);
    left   = clamp(left,   0, columns_ - 1);
    bottom = clamp(bottom, 0, rows_    - 1);
    right  = clamp(right,  0, columns_ - 1);
    if (top > bottom || left > right) return;
    for (int r = top; r <= bottom; ++r) {
        for (int c = left; c <= right; ++c) {
            if (screen_[r][c].wideContinuation && c > 0)
                screen_[r][c-1] = TerminalCell();
            if (screen_[r][c].wide && c + 1 < columns_)
                screen_[r][c+1] = TerminalCell();
            TerminalCell cell = attributes;
            cell.ch = ch;
            cell.text = std::wstring(1, ch);
            cell.wide = false;
            cell.wideContinuation = false;
            cell.softWrapped = false;
            if (characterWidth(ch) == 2) {
                cell.wide = true;
                if (c + 1 < columns_)
                    screen_[r][c+1] = TerminalCell();
            }
            screen_[r][c] = cell;
        }
    }
}

void TerminalBuffer::eraseRect(int top, int left, int bottom, int right, const TerminalCell& attrs) {
    top    = clamp(top,    0, rows_    - 1);
    left   = clamp(left,   0, columns_ - 1);
    bottom = clamp(bottom, 0, rows_    - 1);
    right  = clamp(right,  0, columns_ - 1);
    if (top > bottom || left > right) return;
    for (int r = top; r <= bottom; ++r)
        for (int c = left; c <= right; ++c)
            eraseCell(r, c, attrs);
}

void TerminalBuffer::copyRect(int top, int left, int bottom, int right,
                              int destTop, int destLeft) {
    top    = clamp(top,    0, rows_    - 1);
    left   = clamp(left,   0, columns_ - 1);
    bottom = clamp(bottom, 0, rows_    - 1);
    right  = clamp(right,  0, columns_ - 1);
    if (top > bottom || left > right) return;

    int width  = right  - left  + 1;
    int height = bottom - top   + 1;

    // Snapshot source region
    std::vector<std::vector<TerminalCell>> src(height);
    for (int r = 0; r < height; ++r)
        src[r].assign(screen_[top + r].begin() + left,
                      screen_[top + r].begin() + left + width);

    // Compute destination with overlap-safe iteration
    int dTop    = clamp(destTop,    0, rows_    - 1);
    int dLeft   = clamp(destLeft,   0, columns_ - 1);
    int dBottom = clamp(destTop + height - 1, 0, rows_ - 1);
    int dRight  = clamp(destLeft + width  - 1, 0, columns_ - 1);
    if (dTop > dBottom || dLeft > dRight) return;

    // Determine iteration order to avoid self-overlap corruption
    int rStart, rEnd, rStep;
    if (destTop <= top) { rStart = 0; rEnd = height; rStep = 1; }
    else                { rStart = height - 1; rEnd = -1; rStep = -1; }

    int cStart, cEnd, cStep;
    if (destLeft <= left) { cStart = 0; cEnd = width; cStep = 1; }
    else                  { cStart = width - 1; cEnd = -1; cStep = -1; }

    for (int r = rStart; r != rEnd; r += rStep) {
        int dr = dTop + r;
        for (int c = cStart; c != cEnd; c += cStep) {
            int dc = dLeft + c;
            eraseCell(dr, dc);
            screen_[dr][dc] = src[r][c];
        }
    }
}

void TerminalBuffer::setAttributeChangeExtent(int extent) {
    attrChangeExtent_ = extent;
    if (attrChangeExtent_ < 1) attrChangeExtent_ = 1;
    if (attrChangeExtent_ > 3) attrChangeExtent_ = 3;
}

int TerminalBuffer::attributeChangeExtent() const {
    return attrChangeExtent_;
}

static void applySgrToCell(TerminalCell& cell, const std::vector<int>& sgr) {
    for (size_t i = 0; i < sgr.size(); ++i) {
        int v = sgr[i];
        switch (v) {
        case 0:  cell = TerminalCell(); break;
        case 1:  cell.bold = true; break;
        case 2:  cell.dim = true; break;
        case 3:  cell.italic = true; break;
        case 4:  cell.underline = true; break;
        case 5:  cell.blink = true; break;
        case 7:  cell.inverse = true; break;
        case 9:  cell.strikethrough = true; break;
        case 21: cell.bold = false; break;
        case 22: cell.bold = false; cell.dim = false; break;
        case 23: cell.italic = false; break;
        case 24: cell.underline = false; break;
        case 25: cell.blink = false; break;
        case 27: cell.inverse = false; break;
        case 29: cell.strikethrough = false; break;
        case 38: case 48: case 58: {
            TermColor c;
            if (i + 2 < sgr.size() && sgr[i+1] == 2) {
                c = TermColor::fromRgb(
                    (uint8_t)clamp(sgr[i+2], 0, 255),
                    (uint8_t)clamp(sgr[i+3], 0, 255),
                    (uint8_t)clamp(sgr[i+4], 0, 255));
                i += 4;
            } else if (i + 1 < sgr.size() && sgr[i+1] == 5) {
                int idx = (i + 2 < sgr.size()) ? sgr[i+2] : 0;
                c = TermColor::fromRgb(0, 0, 0);
                (void)idx;
                i += 2;
            }
            if (v == 38) { cell.foreground = c; cell.decorColor = c; }
            else if (v == 48) cell.background = c;
            else if (v == 58) cell.decorColor = c;
            break;
        }
        case 39: cell.foreground = DefaultFg(); cell.decorColor = DefaultDecor(); break;
        case 49: cell.background = DefaultBg(); break;
        case 59: cell.decorColor = DefaultDecor(); break;
        default:
            if (v >= 30 && v <= 37) {
                cell.foreground = TermColor::fromRgb(0,0,0);
                cell.decorColor = cell.foreground;
            } else if (v >= 40 && v <= 47) {
                cell.background = TermColor::fromRgb(0,0,0);
            }
            break;
        }
    }
}

void TerminalBuffer::changeRectAttr(int top, int left, int bottom, int right,
                                    const std::vector<int>& sgrParams) {
    top    = clamp(top,    0, rows_    - 1);
    left   = clamp(left,   0, columns_ - 1);
    bottom = clamp(bottom, 0, rows_    - 1);
    right  = clamp(right,  0, columns_ - 1);
    if (top > bottom || left > right) return;
    for (int r = top; r <= bottom; ++r)
        for (int c = left; c <= right; ++c)
            applySgrToCell(screen_[r][c], sgrParams);
}

void TerminalBuffer::reverseRectAttr(int top, int left, int bottom, int right) {
    top    = clamp(top,    0, rows_    - 1);
    left   = clamp(left,   0, columns_ - 1);
    bottom = clamp(bottom, 0, rows_    - 1);
    right  = clamp(right,  0, columns_ - 1);
    if (top > bottom || left > right) return;
    for (int r = top; r <= bottom; ++r) {
        for (int c = left; c <= right; ++c) {
            auto& cell = screen_[r][c];
            std::swap(cell.foreground, cell.background);
            cell.bold          = !cell.bold;
            cell.dim           = !cell.dim;
            cell.italic        = !cell.italic;
            cell.underline     = !cell.underline;
            cell.blink         = !cell.blink;
            cell.inverse       = !cell.inverse;
            cell.strikethrough = !cell.strikethrough;
        }
    }
}

void TerminalBuffer::clampCursor() {
    // In origin mode the cursor is constrained to the scroll region.
    // In normal mode it is constrained to the physical screen.
    const int rowMin = originMode_ ? scrollTop_    : 0;
    const int rowMax = originMode_ ? scrollBottom_ : rows_ - 1;
    cursorRow_    = clamp(cursorRow_,    rowMin, rowMax);
    cursorColumn_ = clamp(cursorColumn_, 0, columns_ - 1);
}

void TerminalBuffer::trimHistory() {
    // 通常は 1 行だけ溢れるので if で十分（while は O(n²) になるため使わない）
    if ((int)history_.size() > maxHistoryLines_)
        history_.erase(history_.begin());
}

void TerminalBuffer::setMaxHistoryLines(int n) {
    maxHistoryLines_ = std::max(100, n);   // 最低 100 行は保証
    // 新しい上限を超えている分を即時トリム
    while ((int)history_.size() > maxHistoryLines_)
        history_.erase(history_.begin());
}

int TerminalBuffer::maxHistoryLines() const { return maxHistoryLines_; }
