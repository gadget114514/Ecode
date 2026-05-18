#pragma once

#include "TerminalCell.h"
#include <vector>
#include <string>
#include <algorithm>

// ---------------------------------------------------------------------------
// TerminalBuffer
//
// A VT terminal grid.  Ported from termdock-main (Qt → std) and extended
// with features from terminalpp:
//   - applicationCursorMode flag
//   - repeatPreviousChar (REP, CSI b)
//   - softWrapped flag per cell
// ---------------------------------------------------------------------------
class TerminalBuffer {
public:
    using Line = std::vector<TerminalCell>;

    enum class CursorShape { Block, Underline, Bar };

    explicit TerminalBuffer(int columns = 120, int rows = 30);

    // --- screen management ---
    void resize(int columns, int rows);
    void clearScreen();
    void clearAll();
    void clearLine(int mode);       // 0=to EOL, 1=to BOL, 2=whole
    void clearScreenMode(int mode); // 0=to EOS, 1=to BOS, 2=all, 3=history

    // --- character output ---
    void putChar(wchar_t ch, const TerminalCell& attributes = TerminalCell());
    void putText(const std::wstring& text, const TerminalCell& attributes = TerminalCell());
    void repeatPreviousChar(int count); // REP (CSI b) — terminalpp

    // --- cursor movement ---
    void carriageReturn();
    void lineFeed();
    void reverseIndex();
    void backspace();
    void tab();
    void moveCursorRelative(int rowDelta, int columnDelta);
    void moveCursorTo(int row, int column);
    void moveCursorColumn(int column);
    void moveCursorRow(int row);
    void moveCursorNextLine(int count);
    void moveCursorPreviousLine(int count);
    void saveCursor();
    void restoreCursor();

    // --- character editing ---
    void eraseCharacters(int count);
    void insertCharacters(int count);
    void deleteCharacters(int count);

    // --- line editing ---
    void insertLines(int count);
    void deleteLines(int count);
    void scrollUpLines(int count);
    void scrollDownLines(int count);

    // --- scroll region / modes ---
    void setScrollRegion(int top, int bottom);
    void resetScrollRegion();
    void setOriginMode(bool enabled);
    bool originMode() const;
    void setAutoWrapEnabled(bool enabled);
    bool autoWrapEnabled() const;

    // --- accessors ---
    int columns()         const;
    int rows()            const;
    int cursorRow()       const;
    int cursorColumn()    const;
    int cursorColumnRaw() const;
    int historyLineCount()const;
    int totalLineCount()  const;
    const Line& lineAt(int logicalRow) const;

    // --- feature flags (read by TerminalView / TerminalEmulator) ---
    void setBracketedPasteEnabled(bool v);
    bool bracketedPasteEnabled()         const;

    // mouse
    void setMouseTrackingMode(int mode); // 0=off,1000=normal,1002=btn,1003=all
    int  mouseTrackingMode()             const;
    void setSgrMouseEnabled(bool v);     // ?1006 — SGR mouse encoding
    bool sgrMouseEnabled()               const;

    // application cursor mode (?1) — terminalpp
    void setApplicationCursorMode(bool v);
    bool applicationCursorMode()         const;

    // focus events (?1004)
    void setFocusEventReportingEnabled(bool v);
    bool focusEventReportingEnabled()    const;

    // modifyOtherKeys / Kitty keyboard protocol
    void setModifyOtherKeysMode(int mode);
    int  modifyOtherKeysMode()           const;
    void setKittyKeyboardFlags(int flags);
    int  kittyKeyboardFlags()            const;
    bool kittyKeyboardEnabled()          const;

    // cell metrics (for renderer)
    void setCellSize(int width, int height);
    int  cellWidth()  const;
    int  cellHeight() const;

    // cursor appearance
    void setCursorVisible(bool v);
    bool cursorVisible()  const;
    void setCursorBlink(bool v);
    bool cursorBlink()    const;
    void setCursorShape(CursorShape shape);
    CursorShape cursorShape() const;

    // alternate screen (?47 / ?1049)
    void useAlternateScreen(bool enabled);
    bool alternateScreenActive() const;

    // --- static utilities ---
    static int characterWidth(wchar_t ch);
    static int characterWidth(const std::wstring& text);

private:
    Line blankLine() const;
    void normalizeWideCells(Line& line);
    void resizeLines(std::vector<Line>& lines, int oldColumns);
    void eraseCell(int row, int column);
    void clampCursor();
    void trimHistory();

    int columns_      = 120;
    int rows_         = 30;
    int cursorRow_    = 0;
    int cursorColumn_ = 0;
    int savedCursorRow_    = 0;
    int savedCursorColumn_ = 0;
    int scrollTop_    = 0;
    int scrollBottom_ = 29;
    int maxHistoryLines_ = 10000;

    // feature flags
    bool bracketedPasteEnabled_      = false;
    int  mouseTrackingMode_          = 0;
    bool sgrMouseEnabled_            = false;
    bool applicationCursorMode_      = false; // terminalpp
    bool focusEventReportingEnabled_ = false;
    int  modifyOtherKeysMode_        = 0;
    int  kittyKeyboardFlags_         = 0;
    int  cellWidth_  = 8;
    int  cellHeight_ = 16;

    // cursor appearance
    bool cursorVisible_ = true;
    bool cursorBlink_   = true;
    CursorShape cursorShape_ = CursorShape::Block;

    // modes
    bool originMode_     = false;
    bool autoWrapEnabled_= true;
    bool pendingWrap_    = false;
    bool alternateScreenActive_ = false;

    // screen data
    std::vector<Line> history_;
    std::vector<Line> screen_;
    std::vector<Line> mainHistory_;
    std::vector<Line> mainScreen_;
    std::vector<Line> alternateScreen_;
    int mainCursorRow_    = 0;
    int mainCursorColumn_ = 0;
};
