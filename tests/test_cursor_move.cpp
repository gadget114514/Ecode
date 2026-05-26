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
  std::cout << "\nAll tests passed!" << std::endl;
  return 0;
}
