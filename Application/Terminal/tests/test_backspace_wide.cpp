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
// Backspace over fullwidth characters (kanji)
// ============================================================================

// Test 1: Buffer-level backspace - direct cursor movement
// Position cursor at col 4, call backspace() twice
// Should move exactly one column per call: col 4 -> 3 -> 2
// Bug would move two columns on second backspace: col 4 -> 3 -> 1
static void test_backspace_fullwidth_buffer() {
    TestHarness h;

    // Position cursor at column 4 manually
    h.buf.moveCursorColumn(4);
    ASSERT(h.buf.cursorColumn() == 4, "cursor positioned at col 4");

    // First backspace: col 4 -> col 3 (should move exactly 1 column)
    h.buf.backspace();
    ASSERT(h.buf.cursorColumn() == 3, "after first backspace, cursor at col 3");

    // Second backspace: col 3 -> col 2 (should move exactly 1 column, not jump to 1)
    h.buf.backspace();
    ASSERT(h.buf.cursorColumn() == 2, "after second backspace, cursor at col 2 (not col 1 with bug)");

    // Third and fourth backspace to verify continued correct behavior
    h.buf.backspace();
    ASSERT(h.buf.cursorColumn() == 1, "after third backspace, cursor at col 1");

    h.buf.backspace();
    ASSERT(h.buf.cursorColumn() == 0, "after fourth backspace, cursor at col 0");
}

// Test 2: Emulator-level cursor tracking during fullwidth character processing
// Process "あい" (two fullwidth chars) then backspace twice
// Verifies cursor stops at correct position (col 2) not extra movement
static void test_backspace_fullwidth_emulator() {
    TestHarness h;

    // Process two fullwidth characters
    h.feed(L"あい");
    int col_after_fullwidth = h.buf.cursorColumn();
    ASSERT(col_after_fullwidth >= 4, "fullwidth chars position cursor at least at col 4");

    // Backspace once - cursor should move back exactly 1 column
    h.feed(L"\b");
    int col_after_first_bs = h.buf.cursorColumn();
    ASSERT(col_after_first_bs == col_after_fullwidth - 1, "first backspace moves exactly 1 column");

    // Backspace again - should move 1 column, not 2 (the bug)
    h.feed(L"\b");
    int col_after_second_bs = h.buf.cursorColumn();
    ASSERT(col_after_second_bs == col_after_first_bs - 1, "second backspace also moves exactly 1 column (bug fix)");
}

// Test 3: ASCII guard - verify backspace still works for regular ASCII
// Process "abc" then backspace 3 times to verify normal ASCII still works
static void test_backspace_ascii_guard() {
    TestHarness h;

    // Write "abc"
    h.feed(L"abc");
    ASSERT(h.buf.cursorColumn() == 3, "after 'abc', cursor at col 3");

    // Backspace once
    h.feed(L"\b");
    ASSERT(h.buf.cursorColumn() == 2, "after first \\b, cursor at col 2");

    // Backspace again
    h.feed(L"\b");
    ASSERT(h.buf.cursorColumn() == 1, "after second \\b, cursor at col 1");

    // Backspace once more
    h.feed(L"\b");
    ASSERT(h.buf.cursorColumn() == 0, "after third \\b, cursor at col 0");
}

// Test 4: Backspace at column 0 (boundary)
// Verify backspace does nothing when already at column 0
static void test_backspace_at_column_zero() {
    TestHarness h;

    // Cursor starts at column 0
    ASSERT(h.buf.cursorColumn() == 0, "cursor starts at col 0");

    // Backspace should not move cursor
    h.buf.backspace();
    ASSERT(h.buf.cursorColumn() == 0, "backspace at col 0 stays at col 0");
}

// Test 5: Multiple consecutive backspaces with fullwidth mix
// Process "aあb" (ASCII, fullwidth, ASCII) then backspace multiple times
// Verifies each backspace moves exactly one column
static void test_backspace_mixed_width() {
    TestHarness h;

    // Write mixed ASCII and fullwidth
    h.feed(L"aあb");
    // Positions: a=0, あ(base)=1, あ(cont)=2, b=3, cursor should be at 4
    int start_col = h.buf.cursorColumn();
    ASSERT(start_col >= 3, "cursor positioned after mixed chars");

    // Backspace once - remove 'b'
    h.feed(L"\b");
    int col_after_1 = h.buf.cursorColumn();
    ASSERT(col_after_1 == start_col - 1, "first backspace moves exactly 1 column");

    // Backspace again - should move one column (not skip the fullwidth char continuation)
    h.feed(L"\b");
    int col_after_2 = h.buf.cursorColumn();
    ASSERT(col_after_2 == col_after_1 - 1, "second backspace also moves exactly 1 column (not 2)");
}

int main() {
    printf("Terminal Backspace Fullwidth Character Regression Tests\n");
    printf("=========================================================\n\n");

    TEST("Buffer-level backspace cursor movement");
    test_backspace_fullwidth_buffer();
    PASS();

    TEST("Emulator-level fullwidth character backspace");
    test_backspace_fullwidth_emulator();
    PASS();

    TEST("ASCII guard - regular ASCII backspace");
    test_backspace_ascii_guard();
    PASS();

    TEST("Backspace at column 0");
    test_backspace_at_column_zero();
    PASS();

    TEST("Mixed ASCII and fullwidth backspace");
    test_backspace_mixed_width();
    PASS();

    printf("\n");
    if (g_failures > 0) {
        printf("FAILURES: %d\n", g_failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
