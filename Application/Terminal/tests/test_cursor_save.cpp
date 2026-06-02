#include <windows.h>
#include "../include/TerminalBuffer.h"
#include "../include/TerminalEmulator.h"
#include <cstdio>
#include <cassert>
#include <string>

static int g_failures = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); ++g_failures; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

// MSVC-safe escape sequences (break \x1b7 into \x1b "7" to avoid hex greed)
#define ESC   L"\x1b"
#define CSI   L"\x1b["
#define DECSC L"\x1b" L"7"
#define DECRC L"\x1b" L"8"

// Helper: create a buffer and emulator pair
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
// Test: CSI s / CSI u (SCO cursor save/restore)
// ============================================================================
static void test_csi_s_u() {
    TestHarness h;

    // Move cursor to row 5, col 10 (1-indexed: 6;11)
    h.feed(CSI L"6;11H");
    ASSERT(h.buf.cursorRow() == 5, "row should be 5 (0-indexed)");
    ASSERT(h.buf.cursorColumn() == 10, "col should be 10 (0-indexed)");

    // Save cursor
    h.feed(CSI L"s");
    ASSERT(h.buf.cursorRow() == 5, "row unchanged after save");
    ASSERT(h.buf.cursorColumn() == 10, "col unchanged after save");

    // Move cursor to a different position (0,0)
    h.feed(CSI L"H");
    ASSERT(h.buf.cursorRow() == 0, "row moved to 0");
    ASSERT(h.buf.cursorColumn() == 0, "col moved to 0");

    // Type some text (advances cursor)
    h.feed(L"Hello");
    ASSERT(h.buf.cursorRow() == 0, "row still 0");
    ASSERT(h.buf.cursorColumn() == 5, "col advanced to 5");

    // Restore cursor
    h.feed(CSI L"u");
    ASSERT(h.buf.cursorRow() == 5, "row restored to 5");
    ASSERT(h.buf.cursorColumn() == 10, "col restored to 10");
}

// ============================================================================
// Test: ESC 7 / ESC 8 (DECSC/DECRC)
// ============================================================================
static void test_decsc_decrc() {
    TestHarness h;

    // Move cursor to row 10, col 20
    h.feed(CSI L"11;21H");
    ASSERT(h.buf.cursorRow() == 10, "row should be 10");
    ASSERT(h.buf.cursorColumn() == 20, "col should be 20");

    // Save cursor with DECSC
    h.feed(DECSC);
    ASSERT(h.buf.cursorRow() == 10, "row unchanged after DECSC");
    ASSERT(h.buf.cursorColumn() == 20, "col unchanged after DECSC");

    // Move and type
    h.feed(CSI L"1;1HTest");
    ASSERT(h.buf.cursorRow() == 0, "row moved to 0");
    ASSERT(h.buf.cursorColumn() == 4, "col advanced to 4");

    // Restore with DECRC
    h.feed(DECRC);
    ASSERT(h.buf.cursorRow() == 10, "row restored to 10");
    ASSERT(h.buf.cursorColumn() == 20, "col restored to 20");
}

// ============================================================================
// Test: Multiple save/restore nesting (single slot — overwrites)
// ============================================================================
static void test_multi_save() {
    TestHarness h;

    h.feed(CSI L"5;5H");
    h.feed(CSI L"s");       // save (4,4)
    h.feed(CSI L"10;10H");
    h.feed(CSI L"s");       // save (9,9) — overwrites previous
    h.feed(CSI L"1;1H");
    h.feed(CSI L"u");       // restore → (9,9), NOT (4,4)
    ASSERT(h.buf.cursorRow() == 9, "row restored to 9 (last saved)");
    ASSERT(h.buf.cursorColumn() == 9, "col restored to 9 (last saved)");
}

// ============================================================================
// Test: pendingWrap_ save/restore
// ============================================================================
static void test_pending_wrap() {
    TestHarness h;

    // Move cursor to last column and write a char to trigger pendingWrap_
    h.feed(CSI L"1;80H");   // row 0, col 79
    h.feed(L"X");            // write at (0,79), cursor stays at (0,79), pendingWrap_=true
    ASSERT(h.buf.cursorColumn() == 79, "col at right margin after write");

    // Save cursor — should capture pendingWrap_=true
    h.feed(CSI L"s");

    // Move away then restore — pendingWrap_ must be preserved
    h.feed(CSI L"1;1H");    // move to (0,0) — clears pendingWrap_
    h.feed(CSI L"u");       // restore to (0,79) with pendingWrap_=true

    // With pendingWrap_=true, 'A' auto-wraps to next line.
    // With pendingWrap_=false, 'A' overwrites col 79.
    h.feed(L"A");
    ASSERT(h.buf.cursorRow() == 1, "row advanced to 1 (auto-wrap occurred)");
    ASSERT(h.buf.cursorColumn() == 1, "col is 1 after wrap + 'A'");
}

// ============================================================================
// Test: Alternate screen isolation
// ============================================================================
static void test_alt_screen() {
    TestHarness h;

    h.feed(CSI L"5;5H");
    h.feed(CSI L"s");       // save main screen cursor (4,4)

    h.feed(CSI L"?1049h");  // enter alternate screen
    h.feed(CSI L"10;10H");
    h.feed(CSI L"s");       // save alt screen cursor (9,9)
    h.feed(CSI L"1;1H");

    h.feed(CSI L"u");       // restore alt screen cursor → (9,9)
    ASSERT(h.buf.cursorRow() == 9, "row restored to 9 in alt screen");
    ASSERT(h.buf.cursorColumn() == 9, "col restored to 9 in alt screen");

    h.feed(CSI L"?1049l");  // exit alternate screen → back to main
    ASSERT(h.buf.cursorRow() == 4, "row restored to 4 (main screen)");
    ASSERT(h.buf.cursorColumn() == 4, "col restored to 4 (main screen)");
}

// ============================================================================
// Test: Private mode ?s / ?r does NOT interfere with cursor save
// ============================================================================
static void test_private_mode_no_interfere() {
    TestHarness h;

    h.feed(CSI L"5;5H");
    h.feed(CSI L"s");           // cursor save (4,4)
    h.feed(CSI L"?25l");        // hide cursor
    h.feed(CSI L"?s");          // save private modes (doesn't touch cursor save)
    h.feed(CSI L"10;10H");
    h.feed(CSI L"?r");          // restore private modes (doesn't touch cursor save)
    h.feed(CSI L"u");           // cursor restore → (4,4)
    ASSERT(h.buf.cursorRow() == 4, "row restored to 4 after private mode ops");
    ASSERT(h.buf.cursorColumn() == 4, "col restored to 4 after private mode ops");
}

// ============================================================================
// Test: CSI u with bare numeric params — should still restore cursor
// ============================================================================
static void test_csi_u_numeric_params() {
    TestHarness h;

    h.feed(CSI L"5;5H");
    h.feed(CSI L"s");       // save (4,4)
    h.feed(CSI L"10;10H");
    h.feed(CSI L"0u");      // CSI u with param 0 → should restore cursor
    ASSERT(h.buf.cursorRow() == 4, "row restored to 4 with CSI 0 u");
    ASSERT(h.buf.cursorColumn() == 4, "col restored to 4 with CSI 0 u");
}

// ============================================================================
// Test: CSI ? u should NOT restore cursor — query Kitty protocol
// ============================================================================
static void test_csi_query_u() {
    TestHarness h;

    h.feed(CSI L"5;5H");
    h.feed(CSI L"s");       // save (4,4)
    h.feed(CSI L"10;10H");
    h.feed(CSI L"?u");      // CSI ? u — should NOT restore cursor
    // With no response callback set, ?u is essentially a no-op
    ASSERT(h.buf.cursorRow() == 9, "row unchanged by CSI ? u (not restore)");
    ASSERT(h.buf.cursorColumn() == 9, "col unchanged by CSI ? u (not restore)");
}

int main() {
    printf("Terminal Cursor Save/Restore Tests\n");
    printf("==================================\n\n");

    TEST("CSI s / CSI u");            test_csi_s_u();               PASS();
    TEST("DECSC / DECRC (ESC 7/8)");  test_decsc_decrc();           PASS();
    TEST("Multiple save (overwrite)"); test_multi_save();           PASS();
    TEST("pendingWrap_ save/restore"); test_pending_wrap();         PASS();
    TEST("Alternate screen isolation"); test_alt_screen();           PASS();
    TEST("Private mode ?s/?r isolation"); test_private_mode_no_interfere(); PASS();
    TEST("CSI u with numeric params"); test_csi_u_numeric_params(); PASS();
    TEST("CSI ? u (Kitty query)");     test_csi_query_u();          PASS();

    printf("\n");
    if (g_failures > 0) {
        printf("FAILURES: %d\n", g_failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
