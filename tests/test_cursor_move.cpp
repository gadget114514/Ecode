#include "../include/Buffer.h"
#include "../include/PieceTable.h"
#include <cassert>
#include <iostream>
#include <string>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

// MoveCaretUp from end of single newline
void TestMoveUpOneEnter() {
  Buffer buf;
  buf.Insert(0, "a");
  buf.Insert(1, "\n");

  VERIFY(buf.GetTotalLength() == 2, "Length fail");
  VERIFY(buf.GetTotalLines() == 2, "Lines fail");

  buf.SetCaretPos(2);
  buf.MoveCaretUp();
  VERIFY(buf.GetCaretPos() == 0, "Go to line 0");
  std::cout << "PASS: MoveUpOneEnter\n";
}

// MoveCaretUp with two trailing newlines (regression test for line cache bug)
void TestMoveUpTwoEnters() {
  Buffer buf;
  buf.Insert(0, "a");
  buf.Insert(1, "\n");
  buf.Insert(2, "\n");

  buf.SetCaretPos(3);
  buf.MoveCaretUp();
  VERIFY(buf.GetCaretPos() == 2, "First up to line 1");
  buf.MoveCaretUp();
  VERIFY(buf.GetCaretPos() == 0, "Second up to line 0");
  std::cout << "PASS: MoveUpTwoEnters\n";
}

// MoveCaretUp with three trailing newlines
void TestMoveUpThreeEnters() {
  Buffer buf;
  buf.Insert(0, "a");
  buf.Insert(1, "\n"); buf.Insert(2, "\n"); buf.Insert(3, "\n");

  buf.SetCaretPos(4);
  buf.MoveCaretUp(); VERIFY(buf.GetCaretPos() == 3, "Step 1");
  buf.MoveCaretUp(); VERIFY(buf.GetCaretPos() == 2, "Step 2");
  buf.MoveCaretUp(); VERIFY(buf.GetCaretPos() == 0, "Step 3");
  std::cout << "PASS: MoveUpThreeEnters\n";
}

// MoveCaretDown with multiple lines
void TestMoveDownTwoEnters() {
  Buffer buf;
  buf.Insert(0, "a\n\n");

  buf.SetCaretPos(0);
  VERIFY(buf.GetTotalLines() == 3, "3 lines");

  buf.MoveCaretDown(); VERIFY(buf.GetCaretPos() == 2, "Down to line 1");
  buf.MoveCaretDown(); VERIFY(buf.GetCaretPos() == 3, "Down to line 2");
  std::cout << "PASS: MoveDownTwoEnters\n";
}

// MoveCaretUp at first line: should clamp to 0
void TestMoveUpAtFirstLine() {
  Buffer buf;
  buf.Insert(0, "Hello");
  buf.SetCaretPos(0);
  buf.MoveCaretUp();
  VERIFY(buf.GetCaretPos() == 0, "Already at first line");
  std::cout << "PASS: MoveUpAtFirstLine\n";
}

// MoveCaretDown at last line: should stay
void TestMoveDownAtLastLine() {
  Buffer buf;
  buf.Insert(0, "Hello");
  buf.SetCaretPos(5);
  buf.MoveCaretDown();
  VERIFY(buf.GetCaretPos() == 5, "Already at last line");
  std::cout << "PASS: MoveDownAtLastLine\n";
}

// Move up/down with desired column tracking
void TestDesiredColumn() {
  Buffer buf;
  buf.Insert(0, "aaa\nb\nccccc\n");

  // Start at end of "ccccc" (line 2, column 5)
  buf.SetCaretPos(12);
  buf.UpdateDesiredColumn();

  // Move up to "ccccc" (line 2): desiredColumn=0 → start of line
  buf.MoveCaretUp();
  size_t pos = buf.GetCaretPos();
  size_t line = buf.GetLineAtOffset(pos);
  VERIFY(line == 2, "Up to line 2, got line " + std::to_string(line));

  // Move up to "b" (line 1):
  buf.MoveCaretUp();
  pos = buf.GetCaretPos();
  line = buf.GetLineAtOffset(pos);
  VERIFY(line == 1, "Up to line 1, got line " + std::to_string(line) + " offset " + std::to_string(pos));

  // Move up to "aaa" (line 0):
  buf.MoveCaretUp();
  pos = buf.GetCaretPos();
  line = buf.GetLineAtOffset(pos);
  VERIFY(line == 0, "Up to line 0, got line " + std::to_string(line));

  std::cout << "PASS: DesiredColumn\n";
}

// CRLF line ending handling
void TestCrlfMovement() {
  Buffer buf;
  buf.Insert(0, "abc\r\ndef\r\nghi");

  VERIFY(buf.GetTotalLines() == 3, "3 lines with CRLF");

  // Start at end, move up
  buf.SetCaretPos(13);
  buf.MoveCaretUp();
  // Should be on "def" line (start at offset 5)
  VERIFY(buf.GetCaretPos() == 5, "Up from line 2 with CRLF, got " + std::to_string(buf.GetCaretPos()));

  buf.MoveCaretUp();
  VERIFY(buf.GetCaretPos() == 0, "Up to line 0 with CRLF");
  std::cout << "PASS: CrlfMovement\n";
}

// Multiple sequential appends (the original line cache bug)
void TestAppendLineCacheInvalidation() {
  Buffer buf;
  buf.Insert(0, "start");
  for (int i = 0; i < 10; ++i)
    buf.Insert(buf.GetTotalLength(), "\nline" + std::to_string(i));

  VERIFY(buf.GetTotalLines() == 11, "11 lines after 10 appends");
  VERIFY(buf.GetTotalLength() == 5 + 10 * 6, "Length correct");

  // Move from end up repeatedly
  buf.SetCaretPos(buf.GetTotalLength());
  for (int i = 10; i >= 0; --i) {
    size_t expectedLine = (size_t)i;
    size_t actualLine = buf.GetLineAtOffset(buf.GetCaretPos());
    VERIFY(actualLine == expectedLine,
           "At step " + std::to_string(i) + " expected line " +
           std::to_string(expectedLine) + " got " + std::to_string(actualLine));
    if (i > 0) buf.MoveCaretUp();
  }
  std::cout << "PASS: AppendLineCacheInvalidation\n";
}

// MoveCaretByChar across line boundaries
void TestMoveByCharAcrossLines() {
  Buffer buf;
  buf.Insert(0, "ab\ncd");

  buf.SetCaretPos(0);
  for (int i = 0; i < 5; ++i) buf.MoveCaretByChar(1);
  VERIFY(buf.GetCaretPos() == 5, "End after forwarding through lines");

  for (int i = 0; i < 5; ++i) buf.MoveCaretByChar(-1);
  VERIFY(buf.GetCaretPos() == 0, "Start after backward through lines");
  std::cout << "PASS: MoveByCharAcrossLines\n";
}

// Folded lines: MoveCaretUp/Down currently do NOT skip folded lines
// (known limitation; test documents the current behavior)
void TestMoveFoldedLines() {
  Buffer buf;
  buf.Insert(0, "L0\nL1\nL2\nL3\nL4\n");
  buf.FoldLine(1);
  buf.FoldLine(2);

  // From line 3 (offset 9: "L3"), MoveCaretUp goes to line 2 (folded)
  buf.SetCaretPos(9);
  buf.MoveCaretUp();
  size_t caret = buf.GetCaretPos();
  size_t line = buf.GetLineAtOffset(caret);
  VERIFY(line == 2, "Known: MoveCaretUp does not skip folded lines, got line " + std::to_string(line));
  std::cout << "PASS: MoveFoldedLines (known limitation: no folded line skip)\n";
}

// Empty buffer
void TestEmptyBuffer() {
  Buffer buf;
  VERIFY(buf.GetTotalLength() == 0, "Empty length");
  VERIFY(buf.GetTotalLines() == 1, "Empty has 1 line");

  buf.MoveCaretUp();
  VERIFY(buf.GetCaretPos() == 0, "Up on empty buffer");

  buf.MoveCaretDown();
  VERIFY(buf.GetCaretPos() == 0, "Down on empty buffer");
  std::cout << "PASS: EmptyBuffer\n";
}

// Single line: no vertical movement possible
void TestSingleLine() {
  Buffer buf;
  buf.Insert(0, "Hello World");

  buf.SetCaretPos(5);
  buf.MoveCaretUp();
  VERIFY(buf.GetCaretPos() == 0, "Up on single line to start");

  buf.SetCaretPos(5);
  buf.MoveCaretDown();
  // Single line only: MoveCaretDown does nothing
  VERIFY(buf.GetCaretPos() == 5, "Down on single line stays, got " + std::to_string(buf.GetCaretPos()));
  std::cout << "PASS: SingleLine\n";
}

// PieceTable append does not invalidate line cache (regression)
void TestPieceTableAppendInvalidate() {
  PieceTable pt;
  pt.LoadOriginal("base", 4);

  // Multiple appends without explicit InvalidateLineCache
  pt.Insert(4, "\nappended1");
  pt.Insert(pt.GetTotalLength(), "\nappended2");
  pt.Insert(pt.GetTotalLength(), "\nappended3");

  VERIFY(pt.GetTotalLines() == 4, "4 total lines");
  VERIFY(pt.GetLineOffset(0) == 0, "Line 0 at 0");
  VERIFY(pt.GetLineOffset(1) == 5, "Line 1 at 5, got " + std::to_string(pt.GetLineOffset(1)));
  VERIFY(pt.GetLineOffset(2) == 15, "Line 2 at 15, got " + std::to_string(pt.GetLineOffset(2)));
  VERIFY(pt.GetLineOffset(3) == 25, "Line 3 at 25, got " + std::to_string(pt.GetLineOffset(3)));
  std::cout << "PASS: PieceTableAppendInvalidate\n";
}

// SetCaretPos alone should NOT update desired column;
// UpdateDesiredColumn must be called explicitly (as done in mouse click handler)
void TestSetCaretPosDoesNotUpdateDesiredColumn() {
  Buffer buf;
  buf.Insert(0, "aaaaa\nbb\nccccc\n");

  // Simulate mouse click at column 4 of line 2 ("ccccc")
  buf.SetCaretPos(11); // position of 5th 'c' (0-indexed)
  // Note: UpdateDesiredColumn is NOT called here (as was the original bug)

  // Move up to line 1 ("bb") — desiredColumn is still 0 (never updated)
  buf.MoveCaretUp();
  size_t pos = buf.GetCaretPos();
  size_t line = buf.GetLineAtOffset(pos);
  // Should go to start of line 1 (column 0), not column 4
  VERIFY(line == 1, "Up to line 1 after click without UpdateDesiredColumn, got line " + std::to_string(line));
  size_t col = pos - buf.GetLineOffset(line);
  VERIFY(col == 0, "Column should be 0 (desiredColumn never updated), got " + std::to_string(col));

  std::cout << "PASS: SetCaretPosDoesNotUpdateDesiredColumn\n";
}

// After calling UpdateDesiredColumn following SetCaretPos,
// vertical movement should maintain the correct column
void TestUpdateDesiredColumnAfterSetCaretPos() {
  Buffer buf;
  buf.Insert(0, "aaaaa\nbb\nccccc\n");

  // Simulate mouse click + UpdateDesiredColumn (the fix)
  // "aaaaa\nbb\nccccc\n" -> line 2 "ccccc" starts at offset 9
  buf.SetCaretPos(13); // last 'c' on line 2 (column 4)
  buf.UpdateDesiredColumn();

  // Move up to line 1 ("bb", length 2)
  buf.MoveCaretUp();
  size_t pos = buf.GetCaretPos();
  size_t line = buf.GetLineAtOffset(pos);
  VERIFY(line == 1, "Up to line 1 after UpdateDesiredColumn, got line " + std::to_string(line));
  size_t col = pos - buf.GetLineOffset(line);
  // Line 1 is shorter ("bb" = 2 chars, column 0,1), clamps to end (col 2 = past last char, at \n)
  VERIFY(col == 2, "Column should clamp to 2 (end of line 1), got " + std::to_string(col));

  // Move up to line 0 ("aaaaa", length 5)
  buf.MoveCaretUp();
  pos = buf.GetCaretPos();
  line = buf.GetLineAtOffset(pos);
  VERIFY(line == 0, "Up to line 0, got line " + std::to_string(line));
  col = pos - buf.GetLineOffset(line);
  VERIFY(col == 4, "Column should be 4 (desiredColumn=4 fits), got " + std::to_string(col));

  std::cout << "PASS: UpdateDesiredColumnAfterSetCaretPos\n";
}

// ── Batch test: ~100 cursor-movement & desired-column cases ──────────────

struct CursorTestCase {
  const char *text;
  size_t startLine;
  size_t startCol;   // character offset (not byte) from line start
  bool   updateDesired;
  const char *ops;   // 'U' up, 'D' down, 'L' left, 'R' right
  size_t expectLine;
  size_t expectCol;  // expected character offset after ops
  const char *desc;
};

// Compute visible-byte length of a line (excludes trailing \n / \r\n)
static size_t VisibleByteLen(const std::string &text, size_t lineStart,
                              size_t lineEnd) {
  size_t len = lineEnd - lineStart;
  if (len == 0) return 0;
  if (text[lineEnd - 1] == '\n') {
    len--;
    if (len > 0 && text[lineEnd - 2] == '\r') len--;
  }
  return len;
}

// Map character offset → byte offset within a line view (only visible part)
static size_t CharToByteInLine(const std::string &text, size_t lineStart,
                                size_t lineEnd, size_t charIdx) {
  size_t visLen = VisibleByteLen(text, lineStart, lineEnd);
  size_t bo = 0, ci = 0;
  while (ci < charIdx && bo < visLen) {
    unsigned char c = (unsigned char)text[lineStart + bo];
    size_t step = 1;
    if (c < 0x80) step = 1;
    else if ((c&0xE0)==0xC0) step = 2;
    else if ((c&0xF0)==0xE0) step = 3;
    else if ((c&0xF8)==0xF0) step = 4;
    if (bo + step > visLen) break;
    bo += step;
    ci++;
  }
  return bo;
}

// Count character offset from line-start byte offset
static size_t ByteToCharInLine(const std::string &text, size_t lineStart,
                                size_t lineEnd, size_t byteOffset) {
  size_t visLen = VisibleByteLen(text, lineStart, lineEnd);
  size_t bo = 0, ci = 0;
  while (bo < byteOffset && bo < visLen) {
    unsigned char c = (unsigned char)text[lineStart + bo];
    size_t step = 1;
    if (c < 0x80) step = 1;
    else if ((c&0xE0)==0xC0) step = 2;
    else if ((c&0xF0)==0xE0) step = 3;
    else if ((c&0xF8)==0xF0) step = 4;
    bo += step;
    ci++;
  }
  return ci;
}

// Helper: get line text for a buffer line
static std::string BufferLineText(Buffer &buf, size_t line) {
  size_t tot = buf.GetTotalLength();
  size_t lo = buf.GetLineOffset(line);
  size_t hi = (line + 1 < buf.GetTotalLines()) ? buf.GetLineOffset(line + 1) : tot;
  return buf.GetText(lo, hi - lo);
}

static void RunCursorTestCase(const CursorTestCase &tc, int idx) {
  Buffer buf;
  buf.Insert(0, tc.text);
  size_t total = buf.GetTotalLength();
  (void)total;

  // Starting byte position
  size_t slo = buf.GetLineOffset(tc.startLine);
  size_t shi = (tc.startLine + 1 < buf.GetTotalLines())
                   ? buf.GetLineOffset(tc.startLine + 1) : total;
  size_t startByte = slo + CharToByteInLine(
      BufferLineText(buf, tc.startLine), slo, shi, tc.startCol);
  buf.SetCaretPos(startByte);
  if (tc.updateDesired) buf.UpdateDesiredColumn();

  // Execute ops
  for (const char *p = tc.ops; *p; ++p) {
    switch (*p) {
    case 'U': buf.MoveCaretUp();    break;
    case 'D': buf.MoveCaretDown();  break;
    case 'L': buf.MoveCaretByChar(-1); break;
    case 'R': buf.MoveCaretByChar(1);  break;
    }
  }

  // Expected byte position
  size_t elo = buf.GetLineOffset(tc.expectLine);
  size_t ehi = (tc.expectLine + 1 < buf.GetTotalLines())
                   ? buf.GetLineOffset(tc.expectLine + 1) : total;
  size_t expectedByte = elo + CharToByteInLine(
      BufferLineText(buf, tc.expectLine), elo, ehi, tc.expectCol);

  // Actual results
  size_t actualLine = buf.GetLineAtOffset(buf.GetCaretPos());
  size_t alo = buf.GetLineOffset(actualLine);
  size_t ahi = (actualLine + 1 < buf.GetTotalLines())
                   ? buf.GetLineOffset(actualLine + 1) : total;
  size_t actualCol = ByteToCharInLine(BufferLineText(buf, actualLine), alo, ahi,
                                       buf.GetCaretPos() - alo);

  if (buf.GetCaretPos() != expectedByte || actualLine != tc.expectLine) {
    std::cerr << "FAIL [" << idx << "] " << tc.desc
              << ":\n  expected line=" << tc.expectLine << " col=" << tc.expectCol
              << " (byte=" << expectedByte << ")"
              << "\n  got      line=" << actualLine << " col=" << actualCol
              << " (byte=" << buf.GetCaretPos() << ")" << std::endl;
    exit(1);
  }
  std::cout << "PASS [" << idx << "] " << tc.desc << "\n";
}

void Test100CursorCases() {
  //                   1111111111222222
  //         0123456789012345678901234567
  // T_A = "aaaaa\nbbbbb\nccccc\n"
  //        line 0: "aaaaa" [0-4],  \n@5
  //        line 1: "bbbbb" [6-10], \n@11
  //        line 2: "ccccc" [12-16],\n@17
  //        line 3: ""      @18
  // visible: 5 chars each

  // T_B = "aaaaa\nbb\ncccccccccc\n"
  //        line 0: "aaaaa"     [0-4],   \n@5
  //        line 1: "bb"        [6-7],   \n@8
  //        line 2: "cccccccccc"[9-18],  \n@19
  //        line 3: ""          @20
  // visible: 5, 2, 10

  // T_C = "aaa\n\nccc\n"
  //        line 0: "aaa" [0-2], \n@3
  //        line 1: ""    @4,
  //        line 2: "ccc" [5-7], \n@8
  //        line 3: ""    @9

  // T_D = "a\r\nbb\r\nccc\r\n"
  //        line 0: "a"   [0],   \r\n@1-2
  //        line 1: "bb"  [3-4], \r\n@5-6
  //        line 2: "ccc" [7-9], \r\n@10-11
  //        line 3: ""    @12

  // T_E = "Hello" (single line, no newline)
  //        line 0: "Hello" [0-4]

  // T_F = "x\ny\nz\n"
  //        line 0: "x" [0],   \n@1
  //        line 1: "y" [2],   \n@3
  //        line 2: "z" [4],   \n@5
  //        line 3: ""  @6

  // T_G = "hello\nworld"
  //        line 0: "hello" [0-4], \n@5
  //        line 1: "world" [6-10]
  //        (no trailing newline, 2 lines)

  const char *TA = "aaaaa\nbbbbb\nccccc\n";
  const char *TB = "aaaaa\nbb\ncccccccccc\n";
  const char *TC = "aaa\n\nccc\n";
  const char *TD = "a\r\nbb\r\nccc\r\n";
  const char *TE = "Hello";
  const char *TF = "x\ny\nz\n";
  const char *TG = "hello\nworld";

  int n = 0;

  // ─── Group 1: Same-length, UpdateDesired, MoveUp ─────────────────────
  // line-2 col C → up → line-1 col C  (5 × 1)
  for (size_t c = 0; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G1.%zu: L2C%zu U -> L1C%zu (same-length)", c, c, c);
    RunCursorTestCase({TA, 2, c, true, "U", 1, c, d}, n++);
  }

  // line-2 col C → U,U → line-0 col C  (5 × 1)
  for (size_t c = 0; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G1.%zu: L2C%zu UU -> L0C%zu", 5+c, c, c);
    RunCursorTestCase({TA, 2, c, true, "UU", 0, c, d}, n++);
  }

  // line-2 col C → U,D → line-2 col C (roundtrip)  (5 × 1)
  for (size_t c = 0; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G1.%zu: L2C%zu UD -> L2C%zu (roundtrip)", 10+c, c, c);
    RunCursorTestCase({TA, 2, c, true, "UD", 2, c, d}, n++);
  }
  // subtotal: 15

  // ─── Group 2: Clamping (line above/below is shorter) ────────────────
  // T_B: L2 (10 chars) → up → L1 (2 chars), clamped  (10 × 1)
  for (size_t c = 0; c <= 9; ++c) {
    size_t ex = (c <= 2) ? c : 2;
    char d[80]; snprintf(d, sizeof d, "G2.%zu: L2C%zu U -> L1 (short) C%zu (clamp)", c, c, ex);
    RunCursorTestCase({TB, 2, c, true, "U", 1, ex, d}, n++);
  }

  // T_B: L1 col C → up → L0 (longer) same col  (3 × 1)
  for (size_t c = 0; c <= 2; ++c) {
    char d[80]; snprintf(d, sizeof d, "G2.%zu: L1C%zu U -> L0 (longer) C%zu", 10+c, c, c);
    RunCursorTestCase({TB, 1, c, true, "U", 0, c, d}, n++);
  }

  // T_B: L0 col C → down → L1 (shorter), clamped  (5 × 1)
  for (size_t c = 0; c <= 4; ++c) {
    size_t ex = (c <= 2) ? c : 2;
    char d[80]; snprintf(d, sizeof d, "G2.%zu: L0C%zu D -> L1 (short) C%zu (clamp)", 13+c, c, ex);
    RunCursorTestCase({TB, 0, c, true, "D", 1, ex, d}, n++);
  }
  // subtotal: 15+10+3+5=33

  // ─── Group 3: Empty line in the middle ──────────────────────────────
  // T_C: L2 col C → U → L1 (empty) col 0  (4 × 1)
  for (size_t c = 0; c <= 3; ++c) {
    char d[80]; snprintf(d, sizeof d, "G3.%zu: L2C%zu U -> L1 (empty) C0", c, c);
    RunCursorTestCase({TC, 2, c, true, "U", 1, 0, d}, n++);
  }

  // T_C: L1 (empty, col 0) → U → L0 col 0
  RunCursorTestCase({TC, 1, 0, true, "U", 0, 0, "G3.4: L1(empty) U -> L0C0"}, n++);

  // T_C: L0 col C → D → L1 (empty) col 0  (4 × 1)
  for (size_t c = 0; c <= 3; ++c) {
    char d[80]; snprintf(d, sizeof d, "G3.%zu: L0C%zu D -> L1 (empty) C0", 5+c, c);
    RunCursorTestCase({TC, 0, c, true, "D", 1, 0, d}, n++);
  }

  // T_C: L1 (empty) → D → L2 col 0
  RunCursorTestCase({TC, 1, 0, true, "D", 2, 0, "G3.9: L1(empty) D -> L2C0"}, n++);
  // subtotal: 33+4+1+4+1=43

  // ─── Group 4: CRLF line endings ─────────────────────────────────────
  RunCursorTestCase({TD, 2, 0, true, "U", 1, 0, "G4.0: L2C0 U->L1C0 (CRLF)"}, n++);
  RunCursorTestCase({TD, 2, 1, true, "U", 1, 1, "G4.1: L2C1 U->L1C1 (CRLF)"}, n++);
  RunCursorTestCase({TD, 2, 2, true, "U", 1, 2, "G4.2: L2C2 U->L1C2 (CRLF clamp)"}, n++);
  RunCursorTestCase({TD, 2, 3, true, "U", 1, 2, "G4.3: L2C3 U->L1C2 (CRLF clamp)"}, n++);
  RunCursorTestCase({TD, 1, 0, true, "U", 0, 0, "G4.4: L1C0 U->L0C0 (CRLF)"}, n++);
  RunCursorTestCase({TD, 1, 1, true, "U", 0, 1, "G4.5: L1C1 U->L0C1 (CRLF)"}, n++);
  RunCursorTestCase({TD, 1, 2, true, "U", 0, 1, "G4.6: L1C2 U->L0C1 (CRLF clamp)"}, n++);
  RunCursorTestCase({TD, 0, 0, true, "D", 1, 0, "G4.7: L0C0 D->L1C0 (CRLF)"}, n++);
  RunCursorTestCase({TD, 0, 1, true, "D", 1, 1, "G4.8: L0C1 D->L1C1 (CRLF)"}, n++);
  RunCursorTestCase({TD, 0, 0, true, "DD", 2, 0, "G4.9: L0C0 DD->L2C0 (CRLF)"}, n++);
  // subtotal: 43+10=53

  // ─── Group 5: Without UpdateDesiredColumn (desiredColumn stays 0) ───
  // T_A: L2 col C (C≥1) no-update → U → L1 col 0  (4 × 1)
  for (size_t c = 1; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G5.%zu: L2C%zu NO-upd U -> L1C0", c, c);
    RunCursorTestCase({TA, 2, c, false, "U", 1, 0, d}, n++);
  }

  // T_A: L2 col C no-update → U,U → L0 col 0  (4 × 1)
  for (size_t c = 1; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G5.%zu: L2C%zu NO-upd UU -> L0C0", 4+c, c);
    RunCursorTestCase({TA, 2, c, false, "UU", 0, 0, d}, n++);
  }

  // T_B: different lengths, no-update → still col 0  (3 × 1)
  for (size_t c = 1; c <= 3; ++c) {
    char d[80]; snprintf(d, sizeof d, "G5.%zu: L2C%zu NO-upd U -> L1C0 (diff-len)", 8+c, c);
    RunCursorTestCase({TB, 2, c, false, "U", 1, 0, d}, n++);
  }

  // T_C: empty mid line, no-update → still col 0  (3 × 1)
  for (size_t c = 1; c <= 3; ++c) {
    char d[80]; snprintf(d, sizeof d, "G5.%zu: L2C%zu NO-upd U -> L1C0 (empty mid)", 11+c, c);
    RunCursorTestCase({TC, 2, c, false, "U", 1, 0, d}, n++);
  }

  // T_B: also test down without update  (2 × 1)
  RunCursorTestCase({TB, 0, 3, false, "D", 1, 0, "G5.15: L0C3 NO-upd D -> L1C0"}, n++);
  RunCursorTestCase({TB, 0, 4, false, "D", 1, 0, "G5.16: L0C4 NO-upd D -> L1C0"}, n++);
  // subtotal: 53+4+4+3+3+2=69

  // ─── Group 6: Multi-line traversal ──────────────────────────────────
  // T_A: L2 col C → UUU → L0 col 0 (final up from line 0 clamps to col 0)
  for (size_t c = 0; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G6.%zu: L2C%zu UUU -> L0C0 (first-line clamp)", c, c);
    RunCursorTestCase({TA, 2, c, true, "UUU", 0, 0, d}, n++);
  }

  // T_A: L0 col C → DDD → L3 (empty) col 0  (5 × 1)
  for (size_t c = 0; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G6.%zu: L0C%zu DDD -> L3(empty)C0", 5+c, c);
    RunCursorTestCase({TA, 0, c, true, "DDD", 3, 0, d}, n++);
  }
  // subtotal: 69+5+5=79

  // ─── Group 7: Down from L0 preserving column ────────────────────────
  // T_A: L0 col C → D → L1 col C  (5 × 1)
  for (size_t c = 0; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G7.%zu: L0C%zu D -> L1C%zu", c, c, c);
    RunCursorTestCase({TA, 0, c, true, "D", 1, c, d}, n++);
  }

  // T_A: L0 col C → DD → L2 col C  (5 × 1)
  for (size_t c = 0; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G7.%zu: L0C%zu DD -> L2C%zu", 5+c, c, c);
    RunCursorTestCase({TA, 0, c, true, "DD", 2, c, d}, n++);
  }

  // T_A: L0 col C → DU → L0 col C (roundtrip)  (5 × 1)
  for (size_t c = 0; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G7.%zu: L0C%zu DU -> L0C%zu (roundtrip)", 10+c, c, c);
    RunCursorTestCase({TA, 0, c, true, "DU", 0, c, d}, n++);
  }
  // subtotal: 79+5+5+5=94

  // ─── Group 8: Single line ────────────────────────────────────────────
  RunCursorTestCase({TE, 0, 0, true, "U", 0, 0, "G8.0: single-line C0 U -> C0"}, n++);
  RunCursorTestCase({TE, 0, 3, true, "U", 0, 0, "G8.1: single-line C3 U -> C0"}, n++);
  RunCursorTestCase({TE, 0, 0, true, "D", 0, 0, "G8.2: single-line C0 D -> C0"}, n++);
  RunCursorTestCase({TE, 0, 3, true, "D", 0, 3, "G8.3: single-line C3 D -> C3"}, n++);
  // subtotal: 94+4=98

  // ─── Group 9: First/last line boundaries ────────────────────────────
  RunCursorTestCase({TF, 0, 0, true, "U", 0, 0, "G9.0: first-line U -> stays"}, n++);
  RunCursorTestCase({TF, 0, 0, true, "UU", 0, 0, "G9.1: first-line UU -> stays"}, n++);
  RunCursorTestCase({TF, 3, 0, true, "D", 3, 0, "G9.2: last-line D -> stays"}, n++);
  RunCursorTestCase({TF, 3, 0, true, "DD", 3, 0, "G9.3: last-line DD -> stays"}, n++);
  // subtotal: 98+4=102

  // ─── Group 10: No trailing newline files ────────────────────────────
  // TG "hello\nworld" — 2 lines, no trailing \n
  for (size_t c = 0; c <= 4; ++c) {
    char d[80]; snprintf(d, sizeof d, "G10.%zu: L1C%zu U -> L0C%zu (no-trail)", c, c, c);
    RunCursorTestCase({TG, 1, c, true, "U", 0, c, d}, n++);
  }
  // subtotal: 102+5=107

  // That's >100. Print summary.
  std::cout << "\nAll " << n << " batch cursor cases passed!\n";
}

int main() {
  TestMoveUpOneEnter();
  TestMoveUpTwoEnters();
  TestMoveUpThreeEnters();
  TestMoveDownTwoEnters();
  TestMoveUpAtFirstLine();
  TestMoveDownAtLastLine();
  TestDesiredColumn();
  TestCrlfMovement();
  TestAppendLineCacheInvalidation();
  TestMoveByCharAcrossLines();
  TestMoveFoldedLines();
  TestEmptyBuffer();
  TestSingleLine();
  TestPieceTableAppendInvalidate();
  TestSetCaretPosDoesNotUpdateDesiredColumn();
  TestUpdateDesiredColumnAfterSetCaretPos();
  Test100CursorCases();
  std::cout << "\nAll tests passed!" << std::endl;
  return 0;
}
