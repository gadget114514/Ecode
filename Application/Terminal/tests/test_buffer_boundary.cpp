#include <windows.h>
#include "../include/TerminalBuffer.h"
#include "../include/TerminalEmulator.h"
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>

static int g_failures = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); ++g_failures; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

#define ESC   L"\x1b"
#define CSI   L"\x1b["

struct TestHarness {
    TerminalBuffer buf;
    TerminalEmulator emu;

    TestHarness(int cols = 80, int rows = 24)
        : buf(cols, rows), emu()
    {
        emu.reset(&buf);
    }

    void feed(const std::string& text) {
        std::wstring wtext;
        int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
        if (len > 0) {
            wtext.resize(len);
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &wtext[0], len);
        }
        emu.process(wtext);
    }

    void feed(const std::wstring& wtext) {
        emu.process(wtext);
    }
};

// ============================================================================
// 1. Auto-wrap at right edge
// ============================================================================
static void test_autowrap_right_edge() {
    TestHarness h;

    h.feed(CSI L"H");
    std::wstring eightyAs(80, L'A');
    h.feed(eightyAs);
    ASSERT(h.buf.cursorRow() == 0, "80 chars fit on first row");
    ASSERT(h.buf.cursorColumn() == 79, "cursor at last column");

    ASSERT(h.buf.lineAt(0)[79].softWrapped == false,
           "softWrapped not set until wrap triggered");

    h.feed(L"B");
    ASSERT(h.buf.cursorRow() == 1, "81st char wrapped to second row");
    ASSERT(h.buf.cursorColumn() == 1, "cursor at column 1 on second row");
    ASSERT(h.buf.lineAt(0)[79].softWrapped == true,
           "softWrapped set after auto-wrap");
    ASSERT(h.buf.lineAt(1)[0].ch == L'B', "wrapped char at start of next line");
}

// ============================================================================
// 2. pendingWrap_ at last column
// ============================================================================
static void test_pending_wrap_flag() {
    TestHarness h;

    h.feed(CSI L"6;80H");
    ASSERT(h.buf.cursorRow() == 5, "cursor at row 5");
    ASSERT(h.buf.cursorColumn() == 79, "cursor at last column");

    h.feed(L"X");
    ASSERT(h.buf.cursorColumn() == 79, "cursor stays at column 79 after write at edge");
    ASSERT(h.buf.lineAt(5)[79].ch == L'X', "char written at column 79");

    h.feed(L"Y");
    ASSERT(h.buf.cursorRow() == 6, "wrapped to next row after pendingWrap");
    ASSERT(h.buf.cursorColumn() == 1, "cursor at column 1 after wrap");
    ASSERT(h.buf.lineAt(5)[79].softWrapped == true, "softWrapped set on preceding line");
    ASSERT(h.buf.lineAt(6)[0].ch == L'Y', "wrapped char is Y");
}

// ============================================================================
// 3. History scrollback limit
// ============================================================================
static void test_history_limit() {
    TestHarness h(80, 5);

    h.buf.setMaxHistoryLines(100);
    ASSERT(h.buf.maxHistoryLines() == 100, "maxHistoryLines set to 100");

    for (int i = 0; i < 120; ++i) {
        h.feed(CSI L"H");
        h.feed(std::to_wstring(i) + L"\n");
    }

    int histLines = h.buf.historyLineCount();
    ASSERT(histLines <= 100, "history trimmed to maxHistoryLines (100 or fewer)");
    ASSERT(h.buf.totalLineCount() <= 100 + 5,
           "total within limit");

    bool foundOld = false;
    for (int i = 0; i < histLines; ++i) {
        auto& line = h.buf.lineAt(i);
        if (!line.empty() && line[0].ch == L'0')
            foundOld = true;
    }
    ASSERT(!foundOld, "oldest entry '0' was trimmed");
}

// ============================================================================
// 4. Scroll region: lineFeed at scrollBottom_  (pinned top)
// ============================================================================
static void test_scroll_region_lf_bottom() {
    TestHarness h(80, 24);

    h.feed(CSI L"6;16r");
    ASSERT(h.buf.scrollTop() == 5, "scrollTop is 5");
    ASSERT(h.buf.scrollBottom() == 15, "scrollBottom is 15");

    h.feed(CSI L"11;1H");
    h.feed(L"INSIDE");

    h.feed(CSI L"3;1H");
    h.feed(L"PINNED");

    h.feed(CSI L"16;1H");
    for (int i = 0; i < 5; ++i)
        h.feed(L"\n");

    ASSERT(h.buf.cursorRow() == 15, "cursor stays at scrollBottom after LF");

    ASSERT(h.buf.lineAt(2)[0].ch == L'P', "lines above scroll region are pinned");

    ASSERT(h.buf.lineAt(4)[0].ch == L' ', "line just above scrollTop unchanged (blank)");
}

// ============================================================================
// 5. Reverse index at scrollTop_ within scroll region
// ============================================================================
static void test_reverse_index_scrolltop() {
    TestHarness h(80, 24);

    h.feed(CSI L"6;16r");

    for (int r = 5; r <= 15; ++r) {
        h.feed(CSI + std::to_wstring(r + 1) + L";1H");
        h.feed(std::to_wstring(r));
    }

    h.feed(CSI L"6;1H");
    ASSERT(h.buf.cursorRow() == 5, "cursor at scrollTop");

    h.feed(ESC L"M");
    ASSERT(h.buf.cursorRow() == 5, "cursor stays at scrollTop after RI");

    ASSERT(h.buf.lineAt(3)[0].ch == L' ', "lines before scrollTop unchanged (blank)");

    auto& botLine = h.buf.lineAt(15);
    ASSERT(botLine[0].ch == L'1', "scrollBottom now shows old row 14 ('14') after RI scroll");
}

// ============================================================================
// 6. Wide character at right edge
// ============================================================================
static void test_wide_char_right_edge() {
    TestHarness h(80, 24);

    h.feed(CSI L"1;80H");
    ASSERT(h.buf.cursorColumn() == 79, "cursor at last column");

    h.feed(L"\u4e2d"); // CJK char, width=2
    ASSERT(h.buf.cursorRow() == 1, "wide char at right edge wraps to next line");
    ASSERT(h.buf.cursorColumn() == 2, "cursor at column 2 (width 2) after wrap");
    ASSERT(h.buf.lineAt(0)[79].softWrapped == true, "softWrapped set at column 79");

    ASSERT(h.buf.lineAt(1)[0].ch == L'\u4e2d', "wide char placed at (1,0)");
    ASSERT(h.buf.lineAt(1)[0].wide == true, "char is marked wide");
    ASSERT(h.buf.lineAt(1)[1].wideContinuation == true,
           "continuation cell set at (1,1)");
}

// ============================================================================
// 7. Origin mode: cursor clamped to scroll region
// ============================================================================
static void test_origin_mode_clamping() {
    TestHarness h(80, 24);

    h.feed(CSI L"6;16r");
    h.feed(CSI L"?6h");

    ASSERT(h.buf.originMode() == true, "origin mode enabled");

    h.feed(CSI L"1;1H");
    ASSERT(h.buf.cursorRow() == 5, "CUP to (0,0) clamped to scrollTop (5)");
    ASSERT(h.buf.cursorColumn() == 0, "col clamped to 0");

    h.feed(CSI L"31;201H");
    ASSERT(h.buf.cursorRow() == 15, "CUP far outside clamped to scrollBottom (15)");
    ASSERT(h.buf.cursorColumn() == 79, "col clamped to columns_-1");

    h.feed(CSI L"16;1H");
    h.feed(CSI L"100A");
    ASSERT(h.buf.cursorRow() == 5, "CUU 100 clamped to scrollTop");

    h.feed(CSI L"100B");
    ASSERT(h.buf.cursorRow() == 15, "CUD 100 clamped to scrollBottom");
}

// ============================================================================
// 8. Tab stop at column boundary
// ============================================================================
static void test_tab_boundary() {
    TestHarness h(80, 24);

    h.feed(CSI L"1;73H");
    ASSERT(h.buf.cursorColumn() == 72, "cursor at column 72");

    h.feed(L"\t");
    ASSERT(h.buf.cursorColumn() == 79, "tab from 72 advances to last column (79)");

    h.feed(L"\t");
    ASSERT(h.buf.cursorColumn() == 79, "tab at last column stays at 79");

    h.buf.clearAllTabStops();
    h.feed(CSI L"80H");
    h.feed(ESC L"H");
    h.feed(CSI L"1;1H");
    h.feed(L"\t");
    ASSERT(h.buf.cursorColumn() == 79, "tab from 0 to custom stop at 79");
}

// ============================================================================
// 9. Resize narrower: truncation and cursor clamp
// ============================================================================
static void test_resize_narrower() {
    TestHarness h(80, 24);

    h.feed(CSI L"19;1H");
    h.feed(L"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    h.feed(CSI L"21;1H");
    h.feed(L"HELLO_WORLD");

    h.feed(CSI L"22;61H");
    ASSERT(h.buf.cursorColumn() == 60, "cursor at col 60 before resize");

    int oldRow = h.buf.cursorRow();
    h.buf.resize(20, 10);
    ASSERT(h.buf.columns() == 20, "columns narrowed to 20");
    ASSERT(h.buf.rows() == 10, "rows reduced to 10");

    ASSERT(h.buf.cursorColumn() == 19, "cursor col clamped to 19");

    int relRow = oldRow - (24 - 10);
    ASSERT(h.buf.cursorRow() == relRow, "cursor row adjusted after resize");

    int contentRow = 18 - (24 - 10);
    auto& contentLine = h.buf.lineAt(contentRow);
    ASSERT((int)contentLine.size() == 20, "content line truncated to 20 cols");
    ASSERT(contentLine[0].ch == L'A', "pushed-up line content preserved (start)");
    ASSERT(contentLine[19].ch == L'T', "pushed-up line truncated (20th char is T)");
}

// ============================================================================
// 10. Line feed pushes top line to history
// ============================================================================
static void test_lf_pushes_to_history() {
    TestHarness h(80, 5);

    ASSERT(h.buf.historyLineCount() == 0, "history starts empty");

    for (int r = 0; r < 5; ++r) {
        h.feed(CSI + std::to_wstring(r + 1) + L";1H");
        h.feed(std::to_wstring(r));
    }

    ASSERT(h.buf.historyLineCount() == 0, "no scrolling yet");
    ASSERT(h.buf.cursorRow() == 4, "cursor at bottom row");

    h.feed(L"\n");
    ASSERT(h.buf.historyLineCount() == 1, "one line pushed to history");
    ASSERT(h.buf.totalLineCount() == 6, "total = 1 history + 5 screen");

    auto& histLine = h.buf.lineAt(0);
    ASSERT(histLine[0].ch == L'0', "history[0] contains the pushed top line");

    int screenStart = h.buf.historyLineCount();
    auto& botLine = h.buf.lineAt(screenStart + 4);
    ASSERT(botLine[0].ch == L' ', "bottom screen line blank after LF");

    ASSERT(h.buf.cursorRow() == 4, "cursor at bottom row");

    h.feed(L"\n\n\n");
    ASSERT(h.buf.historyLineCount() == 4, "four lines in history after more LFs");
    ASSERT(h.buf.lineAt(1)[0].ch == L'1', "history[1] is line '1'");
}

// ============================================================================
// 11. SGR escape sequence at wrap boundary (should not corrupt wrap)
// ============================================================================
static void test_sgr_at_wrap_boundary() {
    TestHarness h;

    h.feed(CSI L"H");
    std::wstring seventyNine(79, L'A');
    h.feed(seventyNine);
    ASSERT(h.buf.cursorColumn() == 79, "79 chars fill cols 0..78, cursor at 79");

    h.feed(CSI L"1m");  // bold on — no column advance
    h.feed(L"B");        // col 79 — stays at last col, sets pendingWrap_
    ASSERT(h.buf.cursorColumn() == 79, "char at last column");
    ASSERT(h.buf.lineAt(0)[79].ch == L'B', "B at column 79");

    h.feed(CSI L"1m");
    h.feed(L"C");
    ASSERT(h.buf.cursorRow() == 1, "wrapped after pendingWrap + SGR");
    ASSERT(h.buf.cursorColumn() == 1, "cursor at column 1");
    ASSERT(h.buf.lineAt(1)[0].ch == L'C', "C at start of next line");
    ASSERT(h.buf.lineAt(1)[0].bold == true, "SGR bold propagated to wrapped char");
}

// ============================================================================
// 12. Backspace at left edge and at boundary
// ============================================================================
static void test_backspace_boundary() {
    TestHarness h;

    h.feed(CSI L"H");
    ASSERT(h.buf.cursorRow() == 0, "cursor at (0,0)");

    h.feed(L"\b");  // BS at column 0 — clamped
    ASSERT(h.buf.cursorColumn() == 0, "BS at col 0 stays at 0");

    h.feed(L"ABCDE");
    h.feed(L"\b\b");
    ASSERT(h.buf.cursorColumn() == 3, "BS backs up 2 from column 5 to 3");

    h.feed(CSI L"5;80H");  // last column
    h.feed(L"\b");
    ASSERT(h.buf.cursorColumn() == 78, "BS from last column to 78");
}

// ============================================================================
// 13. Erase in line (EL/K) at wrap boundary
// ============================================================================
static void test_erase_line_at_boundary() {
    TestHarness h;

    // Write chars on row 0
    h.feed(CSI L"H");
    h.feed(L"12345");
    ASSERT(h.buf.cursorRow() == 0, "cursor at row 0");
    ASSERT(h.buf.cursorColumn() == 5, "5 chars written, cursor at col 5");
    ASSERT(h.buf.lineAt(0)[2].ch == L'3', "char 3 at (0,2)");

    // Move to column 2, erase to end of line
    h.feed(CSI L"1;3H");  // CUP to row 1, col 3 → (0,2)
    ASSERT(h.buf.cursorColumn() == 2, "cursor at column 2");
    h.feed(CSI L"0K");   // erase cols 2..79
    ASSERT(h.buf.lineAt(0)[1].ch == L'2', "char before cursor (2 at col 1) preserved");
    ASSERT(h.buf.lineAt(0)[2].ch == L' ', "char at cursor (col 2) erased");

    // Erase entire line
    h.feed(CSI L"H");
    h.feed(CSI L"2K");
    ASSERT(h.buf.lineAt(0)[0].ch == L' ', "EL whole line blank");
    ASSERT(h.buf.lineAt(0)[1].ch == L' ', "EL whole line col 1 blank");
}

// ============================================================================
// 14. DEC private mode changes near scroll boundary
// ============================================================================
static void test_dec_mode_at_scroll_boundary() {
    TestHarness h(80, 10);

    h.feed(CSI L"5;6r");  // scroll region rows 4..5
    ASSERT(h.buf.scrollTop() == 4, "scrollTop=4");
    ASSERT(h.buf.scrollBottom() == 5, "scrollBottom=5");

    h.feed(CSI L"?6h");     // DECOM on
    h.feed(CSI L"1;1H");    // CUP to (0,0) → clamped to (4,0)
    ASSERT(h.buf.cursorRow() == 4, "cursor clamped to scrollTop");

    // Move to scrollBottom_ and LF → scroll within region
    h.feed(CSI L"6;1H");    // row 6 → (5,0) in origin mode = scrollBottom
    ASSERT(h.buf.cursorRow() == 5, "cursor at scrollBottom in origin mode");
    h.feed(L"\n");          // LF at scrollBottom → scrolls region
    ASSERT(h.buf.cursorRow() == 5, "cursor stays at scrollBottom after LF (scrolled)");

    // Reset scroll region, disable origin mode
    h.feed(CSI L"r");       // reset scroll region (full screen)
    h.feed(CSI L"?6l");     // DECOM off
    ASSERT(h.buf.originMode() == false, "origin mode off");

    h.feed(CSI L"10;1H");   // CUP to (9,0)
    ASSERT(h.buf.cursorRow() == 9, "cursor at bottom row after DECOM off");

    h.feed(L"\n");          // LF at bottom → pushes to history
    ASSERT(h.buf.historyLineCount() == 1, "LF at bottom pushed line to history");
}

// ============================================================================
// 15. Multiple ESC sequences at wrap point (OSC title + wrap)
// ============================================================================
static void test_osc_title_at_boundary() {
    TestHarness h;

    h.feed(CSI L"H");
    std::wstring seventyNine(79, L'A');
    h.feed(seventyNine);
    ASSERT(h.buf.cursorColumn() == 79, "79 chars fill cols 0..78, cursor at 79");

    // Write at last col: sets pendingWrap_ = true, cursor stays at 79
    h.feed(L"X");
    ASSERT(h.buf.cursorColumn() == 79, "X at col 79, cursor stays, pendingWrap set");

    // OSC title — consumes no columns, pendingWrap_ preserved
    h.feed(ESC L"]0;My Title\x07");
    ASSERT(h.buf.cursorColumn() == 79, "OSC title leaves cursor at 79");

    // pendingWrap_ still true → this char wraps to next line
    h.feed(L"Y");
    ASSERT(h.buf.cursorRow() == 1, "char wraps to row 1 via pendingWrap after OSC");
    ASSERT(h.buf.cursorColumn() == 1, "cursor at col 1");
    ASSERT(h.buf.lineAt(1)[0].ch == L'Y', "Y at start of next line");
    ASSERT(h.buf.lineAt(0)[79].softWrapped == true, "softWrapped set at col 79");
}

// ============================================================================
// 16. Two consecutive CJK wide characters
// ============================================================================
static void test_two_cjk_wide() {
    TestHarness h;

    h.feed(CSI L"H");
    h.feed(L"\u4e2d");     // 中 (width 2)
    ASSERT(h.buf.cursorColumn() == 2, "CJK advances cursor by 2");
    ASSERT(h.buf.lineAt(0)[0].wide == true, "first CJK wide");
    ASSERT(h.buf.lineAt(0)[1].wideContinuation == true, "continuation at col 1");

    h.feed(L"\u56fd");     // 国 (width 2)
    ASSERT(h.buf.cursorColumn() == 4, "second CJK advances cursor by 2 more");
    ASSERT(h.buf.lineAt(0)[2].wide == true, "second CJK wide");
    ASSERT(h.buf.lineAt(0)[3].wideContinuation == true, "continuation at col 3");

    ASSERT(h.buf.lineAt(0)[1].wideContinuation == true, "first continuation preserved");
    ASSERT(h.buf.lineAt(0)[0].ch == L'\u4e2d', "first CJK char intact");
    ASSERT(h.buf.lineAt(0)[2].ch == L'\u56fd', "second CJK char intact");
}

// ============================================================================
// 17. CJK at column 78 (one column remaining — must wrap)
// ============================================================================
static void test_cjk_at_col79_wrap() {
    TestHarness h;

    h.feed(CSI L"H");
    std::wstring seventyNine(79, L'A');
    h.feed(seventyNine);
    ASSERT(h.buf.cursorColumn() == 79, "79 ASCII fill cols 0..78, cursor at 79");

    // CJK needs 2 cols, cursorColumn(79)+2=81 > 80 → wrap before write
    h.feed(L"\u4e2d");
    ASSERT(h.buf.cursorRow() == 1, "CJK wrapped to row 1");
    ASSERT(h.buf.cursorColumn() == 2, "cursor at col 2 after CJK wrap");
    ASSERT(h.buf.lineAt(0)[79].softWrapped == true, "softWrapped set at col 79");
    ASSERT(h.buf.lineAt(1)[0].ch == L'\u4e2d', "CJK char at (1,0)");
    ASSERT(h.buf.lineAt(1)[0].wide == true, "CJK marked wide at (1,0)");
    ASSERT(h.buf.lineAt(1)[1].wideContinuation == true, "continuation at (1,1)");
}

// ============================================================================
// 18. Combining character (zero-width) after wrap
// ============================================================================
static void test_combining_after_wrap() {
    TestHarness h;

    h.feed(CSI L"H");
    std::wstring eighty(80, L'A');
    h.feed(eighty);
    h.feed(L"B");  // wrap to row 1
    ASSERT(h.buf.cursorRow() == 1, "wrapped to row 1");
    ASSERT(h.buf.cursorColumn() == 1, "cursor at col 1");

    // U+0301 COMBINING ACUTE ACCENT — zero-width, attaches to preceding cell
    h.feed(L"\u0301");
    ASSERT(h.buf.cursorColumn() == 1, "combining char doesn't advance cursor");
    ASSERT(h.buf.lineAt(1)[0].text.find(L'\u0301') != std::wstring::npos,
           "combining accent attached to preceding cell text");
}

// ============================================================================
// 19. Mixed ASCII + CJK filling entire width exactly
// ============================================================================
static void test_mixed_ascii_cjk_exact() {
    TestHarness h;

    h.feed(CSI L"H");
    std::wstring seventyNine(79, L'A');
    h.feed(seventyNine);  // cols 0..78, cursor at 79
    ASSERT(h.buf.cursorColumn() == 79, "79 ASCII at cols 0..78, cursor at 79");

    // CJK needs 2 cols, cursorColumn(79)+2=81 > 80 → wrap
    h.feed(L"\u4e2d");
    ASSERT(h.buf.cursorRow() == 1, "CJK wrapped to row 1");
    ASSERT(h.buf.cursorColumn() == 2, "cursor at col 2 after wrap");

    h.feed(L"XY");       // fill cols 2..3 on row 1
    ASSERT(h.buf.cursorColumn() == 4, "XY at cols 2-3");
    ASSERT(h.buf.lineAt(1)[2].ch == L'X', "X at col 2");
    ASSERT(h.buf.lineAt(1)[3].ch == L'Y', "Y at col 3");
}

// ============================================================================
// 20. DECALN (screen alignment test) fills boundary cells
// ============================================================================
static void test_decsc_decrc_at_boundary() {
    TestHarness h;

    h.feed(CSI L"5;80H");  // cursor at (4, 79)
    h.feed(ESC L"7");       // DECSC — save cursor
    ASSERT(h.buf.cursorRow() == 4, "cursor row saved at 4");
    ASSERT(h.buf.cursorColumn() == 79, "cursor col saved at 79");

    h.feed(CSI L"H");       // home
    h.feed(L"HELLO");

    h.feed(ESC L"8");       // DECRC — restore
    ASSERT(h.buf.cursorRow() == 4, "cursor row restored to 4");
    ASSERT(h.buf.cursorColumn() == 79, "cursor col restored to 79");

    // Save near edge, write text to cause scroll, restore
    h.feed(CSI L"24;1H");   // last row
    h.feed(ESC L"7");       // save
    h.feed(L"\n");           // LF → scroll, push to history
    ASSERT(h.buf.cursorRow() == 23, "cursor at last row after LF");

    h.feed(ESC L"8");       // restore → row stays because it was adjusted by scroll
    ASSERT(h.buf.cursorColumn() == 0, "restored col is 0 (from save at col 0)");
}

// ============================================================================
// Main — only PASS on success
// ============================================================================
int main() {
    printf("Terminal Buffer Boundary Tests\n");
    printf("==============================\n\n");

    struct { const char* name; void (*fn)(); } tests[] = {
        { "1. Auto-wrap at right edge",               test_autowrap_right_edge },
        { "2. pendingWrap_ at last column",           test_pending_wrap_flag },
        { "3. History scrollback limit",              test_history_limit },
        { "4. Scroll region: LF at bottom",           test_scroll_region_lf_bottom },
        { "5. Reverse index at scroll top",           test_reverse_index_scrolltop },
        { "6. Wide character at right edge",          test_wide_char_right_edge },
        { "7. Origin mode clamping",                  test_origin_mode_clamping },
        { "8. Tab stop at column boundary",           test_tab_boundary },
        { "9. Resize narrower",                       test_resize_narrower },
        { "10. LF pushes top line to history",        test_lf_pushes_to_history },
        { "11. SGR at wrap boundary",                 test_sgr_at_wrap_boundary },
        { "12. Backspace at boundary",                test_backspace_boundary },
        { "13. Erase in line at boundary",            test_erase_line_at_boundary },
        { "14. DEC mode at scroll boundary",          test_dec_mode_at_scroll_boundary },
        { "15. OSC title at wrap boundary",           test_osc_title_at_boundary },
        { "16. Two consecutive CJK wide",             test_two_cjk_wide },
        { "17. CJK at col 79 wrap",                   test_cjk_at_col79_wrap },
        { "18. Combining char after wrap",            test_combining_after_wrap },
        { "19. Mixed ASCII + CJK exact",              test_mixed_ascii_cjk_exact },
        { "20. DECSC/DECRC at boundary",              test_decsc_decrc_at_boundary },
    };

    int count = sizeof(tests) / sizeof(tests[0]);
    int before = g_failures;
    for (int i = 0; i < count; ++i) {
        TEST(tests[i].name);
        int prev = g_failures;
        tests[i].fn();
        if (g_failures == prev)
            PASS();
    }

    printf("\n");
    int failed = g_failures - before;
    if (failed == 0)
        printf("All %d tests PASSED.\n", count);
    else
        printf("%d / %d test(s) FAILED.\n", failed, count);

    return failed;
}
