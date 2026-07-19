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

    void feed(const std::wstring& wtext) {
        emu.process(wtext);
    }
};

// 1. Truncated CSI recovery: "AB" ESC[1 ESC[2K -> line cleared from col 2; no literal 2K cells
static void test_truncated_csi_recovery() {
    TestHarness h;
    h.feed(L"AB");
    // Feed truncated CSI: L"\x1b[1" followed by L"\x1b[2K"
    h.feed(L"\x1b[1\x1b[2K");
    
    // Check cursor column
    ASSERT(h.buf.cursorColumn() == 2, "cursorColumn is 2");
    
    // Check cell contents. "A", "B" should still be there, and no literal "2" or "K" should be printed
    const auto& line = h.buf.lineAt(0);
    ASSERT(line[0].ch == L'A', "cell 0 has A");
    ASSERT(line[1].ch == L'B', "cell 1 has B");
    ASSERT(line[2].ch == L'\0', "cell 2 is empty");
    ASSERT(line[3].ch == L'\0', "cell 3 is empty");
}

// 2. BS inside CSI executed: "AB" ESC[ \b "3" m -> cursor moved back by the BS; SGR 3 applied; no dropped BS
static void test_bs_inside_csi_executed() {
    TestHarness h;
    h.feed(L"AB");
    // BS inside CSI: L"\x1b[\b3m" (parameter is 3, SGR 3 is italic)
    h.feed(L"\x1b[\b3mC");
    
    // The BS should move cursor back to column 1
    // The SGR 3 (italic) should be applied to "C" printed at column 1
    ASSERT(h.buf.cursorColumn() == 2, "cursorColumn is 2 (from 1 + 1 width)");
    const auto& line = h.buf.lineAt(0);
    ASSERT(line[0].ch == L'A', "cell 0 remains A");
    ASSERT(line[1].ch == L'C', "cell 1 has C");
    ASSERT(line[1].italic == true, "C is italic");
}

// 3. CAN aborts CSI: ESC[1;3 CAN "X" -> X printed as text, no cursor move
static void test_can_aborts_csi() {
    TestHarness h;
    h.feed(L"\x1b[1;3\x18X");
    ASSERT(h.buf.cursorColumn() == 1, "cursorColumn is 1");
    const auto& line = h.buf.lineAt(0);
    ASSERT(line[0].ch == L'X', "cell 0 has X");
}

// 4. DECALN: ESC#8 -> screen filled with E, cursor home, no literal 8
static void test_decaln() {
    TestHarness h(80, 24);
    h.feed(L"\x1b#8");
    ASSERT(h.buf.cursorRow() == 0, "cursorRow is 0");
    ASSERT(h.buf.cursorColumn() == 0, "cursorColumn is 0");
    const auto& line = h.buf.lineAt(0);
    ASSERT(line[0].ch == L'E', "cell 0 has E");
    ASSERT(line[79].ch == L'E', "cell 79 has E");
    const auto& lastLine = h.buf.lineAt(23);
    ASSERT(lastLine[0].ch == L'E', "last row cell 0 has E");
    ASSERT(lastLine[79].ch == L'E', "last row cell 79 has E");
}

// 5. ESC%G swallowed: ESC%G "hi" -> only hi printed
static void test_esc_percent_g_swallowed() {
    TestHarness h;
    h.feed(L"\x1b%Ghi");
    ASSERT(h.buf.cursorColumn() == 2, "cursorColumn is 2");
    const auto& line = h.buf.lineAt(0);
    ASSERT(line[0].ch == L'h', "cell 0 has h");
    ASSERT(line[1].ch == L'i', "cell 1 has i");
}

// 6. OSC cap self-heal: ESC]0; + >cap junk + "ok" -> terminal recovers, ok printed
static void test_osc_cap_self_heal() {
    TestHarness h;
    std::wstring junk(4194304 + 10, L'A');
    h.feed(L"\x1b]0;" + junk + L"ok");
    ASSERT(h.buf.cursorColumn() == 2, "cursorColumn is 2");
    const auto& line = h.buf.lineAt(0);
    ASSERT(line[0].ch == L'o', "cell 0 has o");
    ASSERT(line[1].ch == L'k', "cell 1 has k");
}

// 7. Chunk split mid-CSI: ESC[3 / (separate process call) D -> cursor left 3 — regression guard
static void test_chunk_split_mid_csi() {
    TestHarness h;
    h.feed(L"ABCDE");
    ASSERT(h.buf.cursorColumn() == 5, "initial col is 5");
    h.feed(L"\x1b[3");
    h.feed(L"D");
    ASSERT(h.buf.cursorColumn() == 2, "cursorColumn moved left 3 to 2");
}

// 8. Non-BMP width: 𝐀 (U+1D400) -> occupies 1 cell
static void test_non_bmp_width() {
    TestHarness h;
    h.feed(L"\xD835\xDC00"); // U+1D400 Mathematical Bold Capital A
    ASSERT(h.buf.cursorColumn() == 1, "U+1D400 is width 1");
    const auto& line = h.buf.lineAt(0);
    ASSERT(line[0].wide == false, "U+1D400 cell is not wide");
}

// 9. Ambiguous width CP932: ■ with ambiguous-wide on -> occupies 2 cells; off → 1 cell
static void test_ambiguous_width_cp932() {
    // off
    {
        TestHarness h;
        h.buf.setAmbiguousWide(false);
        h.feed(L"\x25a0"); // ■
        ASSERT(h.buf.cursorColumn() == 1, "■ is width 1 when ambiguous-wide is off");
        const auto& line = h.buf.lineAt(0);
        ASSERT(line[0].wide == false, "cell is not wide when ambiguous-wide is off");
    }
    // on
    {
        TestHarness h;
        h.buf.setAmbiguousWide(true);
        h.feed(L"\x25a0"); // ■
        ASSERT(h.buf.cursorColumn() == 2, "■ is width 2 when ambiguous-wide is on");
        const auto& line = h.buf.lineAt(0);
        ASSERT(line[0].wide == true, "cell is wide when ambiguous-wide is on");
    }
}

int main() {
    printf("Terminal ESC Recovery and Width Alignment Tests\n");
    printf("=============================================\n\n");

    TEST("Truncated CSI recovery");       test_truncated_csi_recovery();    PASS();
    TEST("BS inside CSI executed");       test_bs_inside_csi_executed();    PASS();
    TEST("CAN aborts CSI");               test_can_aborts_csi();            PASS();
    TEST("DECALN");                       test_decaln();                    PASS();
    TEST("ESC%%G swallowed");             test_esc_percent_g_swallowed();   PASS();
    TEST("OSC cap self-heal");            test_osc_cap_self_heal();         PASS();
    TEST("Chunk split mid-CSI");          test_chunk_split_mid_csi();       PASS();
    TEST("Non-BMP width");                test_non_bmp_width();             PASS();
    TEST("Ambiguous width CP932");        test_ambiguous_width_cp932();     PASS();

    printf("\n");
    if (g_failures > 0) {
        printf("FAILURES: %d\n", g_failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
