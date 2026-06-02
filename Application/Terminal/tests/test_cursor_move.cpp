#include <windows.h>
#include "../include/TerminalBuffer.h"
#include "../include/TerminalEmulator.h"
#include <cstdio>
#include <string>

static int g_failures = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); ++g_failures; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

#define CSI L"\x1b["

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
// CUU — Cursor Up (CSI n A)
// ============================================================================
static void test_cuu() {
    TestHarness h;

    h.feed(CSI L"10;10H");
    h.feed(CSI L"3A");
    ASSERT(h.buf.cursorRow() == 6, "CUU 3: row 9->6");
    ASSERT(h.buf.cursorColumn() == 9, "col unchanged");

    h.feed(CSI L"A");
    ASSERT(h.buf.cursorRow() == 5, "CUU default(1): row 6->5");

    h.feed(CSI L"0A");
    ASSERT(h.buf.cursorRow() == 4, "CUU 0: treated as 1, row 5->4");

    h.feed(CSI L"100A");
    ASSERT(h.buf.cursorRow() == 0, "CUU 100: clamped to top");
}

// ============================================================================
// CUD — Cursor Down (CSI n B)
// ============================================================================
static void test_cud() {
    TestHarness h;

    h.feed(CSI L"5;5H");
    h.feed(CSI L"3B");
    ASSERT(h.buf.cursorRow() == 7, "CUD 3: row 5->8");

    h.feed(CSI L"B");
    ASSERT(h.buf.cursorRow() == 8, "CUD default(1)");

    h.feed(CSI L"100B");
    ASSERT(h.buf.cursorRow() == 23, "CUD 100: clamped to bottom");
}

// ============================================================================
// CUF — Cursor Forward (CSI n C)
// ============================================================================
static void test_cuf() {
    TestHarness h;

    h.feed(CSI L"1;1H");
    h.feed(CSI L"10C");
    ASSERT(h.buf.cursorColumn() == 10, "CUF 10: col 0->10");

    h.feed(CSI L"C");
    ASSERT(h.buf.cursorColumn() == 11, "CUF default(1)");

    h.feed(CSI L"200C");
    ASSERT(h.buf.cursorColumn() == 79, "CUF 200: clamped to right edge");
}

// ============================================================================
// CUB — Cursor Backward (CSI n D)
// ============================================================================
static void test_cub() {
    TestHarness h;

    h.feed(CSI L"1;40H");
    h.feed(CSI L"10D");
    ASSERT(h.buf.cursorColumn() == 29, "CUB 10: col 39->29");

    h.feed(CSI L"D");
    ASSERT(h.buf.cursorColumn() == 28, "CUB default(1)");

    h.feed(CSI L"100D");
    ASSERT(h.buf.cursorColumn() == 0, "CUB 100: clamped to left edge");
}

// ============================================================================
// CNL — Cursor Next Line (CSI n E)
// ============================================================================
static void test_cnl() {
    TestHarness h;

    h.feed(CSI L"5;20H");
    h.feed(CSI L"2E");
    ASSERT(h.buf.cursorRow() == 6, "CNL 2: row 4->6");
    ASSERT(h.buf.cursorColumn() == 0, "CNL: col reset to 0");

    h.feed(CSI L"E");
    ASSERT(h.buf.cursorRow() == 7, "CNL default(1): row 6->7");
    ASSERT(h.buf.cursorColumn() == 0, "CNL: col still 0");
}

// ============================================================================
// CPL — Cursor Previous Line (CSI n F)
// ============================================================================
static void test_cpl() {
    TestHarness h;

    h.feed(CSI L"10;20H");
    h.feed(CSI L"3F");
    ASSERT(h.buf.cursorRow() == 6, "CPL 3: row 9->6");
    ASSERT(h.buf.cursorColumn() == 0, "CPL: col reset to 0");

    h.feed(CSI L"F");
    ASSERT(h.buf.cursorRow() == 5, "CPL default(1): row 6->5");
    ASSERT(h.buf.cursorColumn() == 0, "CPL: col still 0");
}

// ============================================================================
// CHA — Cursor Horizontal Absolute (CSI n G)
// ============================================================================
static void test_cha() {
    TestHarness h;

    h.feed(CSI L"5;5H");
    h.feed(CSI L"30G");
    ASSERT(h.buf.cursorColumn() == 29, "CHA 30: col->29");

    h.feed(CSI L"G");
    ASSERT(h.buf.cursorColumn() == 0, "CHA default(1): col->0");

    h.feed(CSI L"80G");
    ASSERT(h.buf.cursorColumn() == 79, "CHA 80: col->79 (1-based)");

    h.feed(CSI L"0G");
    ASSERT(h.buf.cursorColumn() == 0, "CHA 0: col->0");
}

// ============================================================================
// CUP — Cursor Position (CSI row ; col H)
// ============================================================================
static void test_cup() {
    TestHarness h;

    h.feed(CSI L"5;10H");
    ASSERT(h.buf.cursorRow() == 4, "CUP 5;10: row->4 (0-indexed)");
    ASSERT(h.buf.cursorColumn() == 9, "CUP 5;10: col->9 (0-indexed)");

    h.feed(CSI L"H");
    ASSERT(h.buf.cursorRow() == 0, "CUP default: row->0");
    ASSERT(h.buf.cursorColumn() == 0, "CUP default: col->0");

    h.feed(CSI L"100;200H");
    ASSERT(h.buf.cursorRow() == 23, "CUP oversized: clamped to bottom");
    ASSERT(h.buf.cursorColumn() == 79, "CUP oversized: clamped to right");

    h.feed(CSI L"0;0H");
    ASSERT(h.buf.cursorRow() == 0, "CUP 0;0: row->0");
    ASSERT(h.buf.cursorColumn() == 0, "CUP 0;0: col->0");
}

// ============================================================================
// HVP — Horizontal Vertical Position (CSI row ; col f)
// ============================================================================
static void test_hvp() {
    TestHarness h;

    h.feed(CSI L"10;20f");
    ASSERT(h.buf.cursorRow() == 9, "HVP 10;20: row->9");
    ASSERT(h.buf.cursorColumn() == 19, "HVP 10;20: col->19");

    h.feed(CSI L"f");
    ASSERT(h.buf.cursorRow() == 0, "HVP default: row->0");
    ASSERT(h.buf.cursorColumn() == 0, "HVP default: col->0");
}

// ============================================================================
// VPA — Vertical Position Absolute (CSI n d)
// ============================================================================
static void test_vpa() {
    TestHarness h;

    h.feed(CSI L"10;10H");
    h.feed(CSI L"15d");
    ASSERT(h.buf.cursorRow() == 14, "VPA 15: row->14");

    h.feed(CSI L"d");
    ASSERT(h.buf.cursorRow() == 0, "VPA default(1): row->0");

    h.feed(CSI L"0d");
    ASSERT(h.buf.cursorRow() == 0, "VPA 0: row->0");
}

// ============================================================================
// HPR — Horizontal Position Relative (CSI n a)
// ============================================================================
static void test_hpr() {
    TestHarness h;

    h.feed(CSI L"1;1H");
    h.feed(CSI L"15a");
    ASSERT(h.buf.cursorColumn() == 15, "HPR 15: col->15");

    h.feed(CSI L"a");
    ASSERT(h.buf.cursorColumn() == 16, "HPR default(1)");
}

// ============================================================================
// Combined movement: cursor sequences preserve column across vertical moves
// ============================================================================
static void test_column_preserved() {
    TestHarness h;

    h.feed(CSI L"12;30H");
    ASSERT(h.buf.cursorColumn() == 29, "col=29");

    h.feed(CSI L"2A");
    ASSERT(h.buf.cursorRow() == 9, "row->9");
    ASSERT(h.buf.cursorColumn() == 29, "col preserved after CUU");

    h.feed(CSI L"2B");
    ASSERT(h.buf.cursorRow() == 11, "row->11");
    ASSERT(h.buf.cursorColumn() == 29, "col preserved after CUD");

    h.feed(CSI L"1;1H");
    h.feed(CSI L"12;30f");
    ASSERT(h.buf.cursorColumn() == 29, "HVP col=29");
}

// ============================================================================
// Origin mode interaction with CUP / VPA / CUU / CUD
// ============================================================================
static void test_origin_mode() {
    TestHarness h(80, 24);

    h.feed(CSI L"5;10r");
    h.feed(CSI L"?6h");

    h.feed(CSI L"1;1H");
    ASSERT(h.buf.cursorRow() == 4, "origin mode CUP 1;1 -> row=scrollTop");
    ASSERT(h.buf.cursorColumn() == 0, "origin mode CUP 1;1 -> col=0");

    h.feed(CSI L"5;5H");
    ASSERT(h.buf.cursorRow() == 8, "origin mode CUP 5;5 -> row=4+4");
    ASSERT(h.buf.cursorColumn() == 4, "origin mode CUP 5;5 -> col=4");

    h.feed(CSI L"1;1H");
    h.feed(CSI L"100B");
    ASSERT(h.buf.cursorRow() == 9, "origin mode CUD clamped to scrollBottom");

    h.feed(CSI L"100A");
    ASSERT(h.buf.cursorRow() == 4, "origin mode CUU clamped to scrollTop");

    h.feed(CSI L"?6l");
    h.feed(CSI L"10;10H");
    ASSERT(h.buf.cursorRow() == 9, "origin mode off: normal CUP");
    ASSERT(h.buf.cursorColumn() == 9, "origin mode off: normal col");
}

// ============================================================================
// CUP/CHA/VPA with 0-index params
// ============================================================================
static void test_zero_params() {
    TestHarness h;

    h.feed(CSI L"0;0H");
    ASSERT(h.buf.cursorRow() == 0, "CUP 0;0 row=0");
    ASSERT(h.buf.cursorColumn() == 0, "CUP 0;0 col=0");

    h.feed(CSI L"10;20H");
    h.feed(CSI L"0G");
    ASSERT(h.buf.cursorColumn() == 0, "CHA 0 col=0");

    h.feed(CSI L"0d");
    ASSERT(h.buf.cursorRow() == 0, "VPA 0 row=0");
}

// ============================================================================
// CNL and CPL scroll region boundary
// ============================================================================
static void test_cnl_cpl_scroll_region() {
    TestHarness h;

    h.feed(CSI L"5;10r");
    h.feed(CSI L"5;1H");
    ASSERT(h.buf.cursorRow() == 4, "cursor at scroll top");

    h.feed(CSI L"10E");
    ASSERT(h.buf.cursorRow() == 9, "CNL stopped at scroll bottom");

    h.feed(CSI L"F");
    ASSERT(h.buf.cursorRow() == 8, "CPL from scroll bottom");

    h.feed(CSI L"10F");
    ASSERT(h.buf.cursorRow() == 4, "CPL stopped at scroll top");
}

int main() {
    printf("Terminal Cursor Movement Escape Sequence Tests\n");
    printf("==============================================\n\n");

    TEST("CUU (CSI A)");                  test_cuu();                  PASS();
    TEST("CUD (CSI B)");                  test_cud();                  PASS();
    TEST("CUF (CSI C)");                  test_cuf();                  PASS();
    TEST("CUB (CSI D)");                  test_cub();                  PASS();
    TEST("CNL (CSI E)");                  test_cnl();                  PASS();
    TEST("CPL (CSI F)");                  test_cpl();                  PASS();
    TEST("CHA (CSI G)");                  test_cha();                  PASS();
    TEST("CUP (CSI H)");                  test_cup();                  PASS();
    TEST("HVP (CSI f)");                  test_hvp();                  PASS();
    TEST("VPA (CSI d)");                  test_vpa();                  PASS();
    TEST("HPR (CSI a)");                  test_hpr();                  PASS();
    TEST("Column preserved across moves"); test_column_preserved();    PASS();
    TEST("Origin mode");                  test_origin_mode();          PASS();
    TEST("Zero params");                  test_zero_params();          PASS();
    TEST("CNL/CPL scroll region");        test_cnl_cpl_scroll_region();PASS();

    printf("\n");
    if (g_failures > 0) {
        printf("FAILURES: %d\n", g_failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
