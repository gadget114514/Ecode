#include "../include/PieceTable.h"
#include <cassert>
#include <iostream>
#include <random>
#include <string>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

void TestLineCounting() {
  PieceTable pt;
  std::string text = "Line 1\nLine 2\nLine 3\n";
  pt.LoadOriginal(text.c_str(), text.length());

  VERIFY(pt.GetTotalLength() == text.length(), "Total length mismatch");
  VERIFY(pt.GetTotalLines() == 4, "Should have 4 lines (3 newlines + 1)");

  std::cout << "Test Passed: Line counting" << std::endl;
}

void TestLineOffsets() {
  PieceTable pt;
  std::string text = "ABCD\nEFGH\nIJKL\nMNOP\n";
  pt.LoadOriginal(text.c_str(), text.length());

  VERIFY(pt.GetTotalLines() == 5, "Should have 5 lines (4 newlines + 1)");
  VERIFY(pt.GetLineOffset(0) == 0, "Line 0 offset should be 0");
  VERIFY(pt.GetLineOffset(1) == 5, "Line 1 offset should be 5");
  VERIFY(pt.GetLineOffset(2) == 10, "Line 2 offset should be 10");
  VERIFY(pt.GetLineOffset(3) == 15, "Line 3 offset should be 15");
  VERIFY(pt.GetLineOffset(4) == 20, "Line 4 offset should be at end");

  std::cout << "Test Passed: Line offsets" << std::endl;
}

void TestGetLineAtOffset() {
  PieceTable pt;
  std::string text = "ABCD\nEFGH\nIJKL\nMNOP\n";
  pt.LoadOriginal(text.c_str(), text.length());

  VERIFY(pt.GetTotalLines() == 5, "Should have 5 lines");
  VERIFY(pt.GetLineAtOffset(0) == 0, "Offset 0 -> line 0");
  VERIFY(pt.GetLineAtOffset(4) == 0, "Offset 4 -> line 0 (still on first line)");
  VERIFY(pt.GetLineAtOffset(5) == 1, "Offset 5 -> line 1 (after \\n)");
  VERIFY(pt.GetLineAtOffset(9) == 1, "Offset 9 -> line 1");
  VERIFY(pt.GetLineAtOffset(10) == 2, "Offset 10 -> line 2");
  VERIFY(pt.GetLineAtOffset(19) == 3, "Offset 19 -> line 3");
  VERIFY(pt.GetLineAtOffset(20) == 4, "Offset 20 -> line 4 (past last \\n)");

  std::cout << "Test Passed: GetLineAtOffset" << std::endl;
}

void TestLineOffsetsAfterEdit() {
  PieceTable pt;
  std::string text = "Hello\nWorld\n";
  pt.LoadOriginal(text.c_str(), text.length());

  pt.Insert(6, "Beautiful ");
  // text becomes: "Hello\nBeautiful World\n"

  VERIFY(pt.GetTotalLength() == 22, "Length after insert: 6+10+6=22");
  VERIFY(pt.GetTotalLines() == 3, "Should have 3 lines (2 newlines + 1)");

  VERIFY(pt.GetLineOffset(0) == 0, "Line 0 offset unchanged");
  VERIFY(pt.GetLineOffset(1) == 6, "Line 1 starts after original newline");
  VERIFY(pt.GetLineOffset(2) == 22, "Line 2 (end)");

  pt.Delete(0, 6);
  // text becomes: "Beautiful World\n"
  VERIFY(pt.GetTotalLines() == 2, "Should have 2 lines after deleting first line (1 newline + 1)");
  VERIFY(pt.GetLineOffset(0) == 0, "Only line offset is 0");
  VERIFY(pt.GetLineOffset(1) == 16, "End offset");

  std::cout << "Test Passed: Line offsets after edit" << std::endl;
}

void TestWriteToCallback() {
  PieceTable pt;
  std::string original = "Hello World";
  pt.LoadOriginal(original.c_str(), original.length());

  pt.Insert(5, " Beautiful");

  std::string result;
  pt.WriteTo([&](const char *data, size_t len) {
    result.append(data, len);
  });

  VERIFY(result == "Hello Beautiful World",
         "WriteTo callback should reconstruct full text");
  VERIFY(result.size() == pt.GetTotalLength(),
         "WriteTo output length should match GetTotalLength");

  std::cout << "Test Passed: WriteTo callback" << std::endl;
}

void TestEmptyOriginal() {
  PieceTable pt;
  pt.LoadOriginal(nullptr, 0);

  VERIFY(pt.GetTotalLength() == 0, "Empty original: length 0");
  VERIFY(pt.GetTotalLines() == 1, "Empty original: lines 1 (one empty line)");

  pt.Insert(0, "Content");
  VERIFY(pt.GetTotalLength() == 7, "After insert into empty: length 7");
  VERIFY(pt.GetText(0, 7) == "Content", "Content mismatch");

  pt.Delete(0, 7);
  VERIFY(pt.GetTotalLength() == 0, "After delete all: length 0");
  VERIFY(pt.GetText(0, 0) == "", "Empty text after delete all");

  std::cout << "Test Passed: Empty original" << std::endl;
}

void TestInsertDeleteInterleaved() {
  PieceTable pt;
  pt.LoadOriginal("BaseContent", 11);

  pt.Delete(0, 4);
  VERIFY(pt.GetText(0, pt.GetTotalLength()) == "Content",
         "After delete 'Base': 'Content'");

  pt.Insert(0, "Test");
  VERIFY(pt.GetText(0, pt.GetTotalLength()) == "TestContent",
         "After insert at 0: 'TestContent'");

  pt.Insert(11, "123");
  VERIFY(pt.GetText(0, pt.GetTotalLength()) == "TestContent123",
         "After append: 'TestContent123'");

  pt.Delete(4, 7);
  VERIFY(pt.GetText(0, pt.GetTotalLength()) == "Test123",
         "After delete middle: 'Test123'");

  std::cout << "Test Passed: Insert/delete interleaved" << std::endl;
}

void TestCompactAfterEdits() {
  PieceTable pt;
  pt.LoadOriginal("Base", 4);
  pt.Insert(4, ".");
  pt.Insert(5, ".");
  pt.Insert(6, ".");
  size_t before = pt.GetPieceCount();
  pt.CompactPieces();
  size_t after = pt.GetPieceCount();
  VERIFY(after < before,
         "Compaction should reduce piece count (" + std::to_string(before) +
             " -> " + std::to_string(after) + ")");
  VERIFY(pt.GetText(0, pt.GetTotalLength()) == "Base...",
         "Compaction should preserve content");

  std::cout << "Test Passed: Compact after edits" << std::endl;
}

void TestLineCacheRebuild() {
  PieceTable pt;
  std::string lines;
  for (int i = 0; i < 100; ++i) {
    lines += "Line " + std::to_string(i) + "\n";
  }
  pt.LoadOriginal(lines.c_str(), lines.length());

  VERIFY(pt.GetTotalLines() == 101, "Should have 101 lines (100 newlines + 1)");

  for (int i = 0; i < 100; ++i) {
    size_t offset = pt.GetLineOffset(i);
    size_t line = pt.GetLineAtOffset(offset);
    VERIFY(line == (size_t)i,
           "GetLineAtOffset(GetLineOffset(" + std::to_string(i) +
               ")) round-trip failed");
  }

  pt.InvalidateLineCache();
  VERIFY(pt.GetTotalLines() == 101,
         "Line count should survive cache invalidation");
  VERIFY(pt.GetLineOffset(50) > 0, "Line offset should work after cache rebuild");

  std::cout << "Test Passed: Line cache rebuild" << std::endl;
}

void TestUndoRedoBranching() {
  PieceTable pt;
  pt.LoadOriginal("Start", 5);

  pt.Insert(5, "ABC");
  pt.Insert(8, "DEF");
  VERIFY(pt.GetText(0, pt.GetTotalLength()) == "StartABCDEF",
         "Content after two inserts");

  pt.Undo();
  VERIFY(pt.GetText(0, pt.GetTotalLength()) == "StartABC",
         "After first undo");

  pt.Undo();
  VERIFY(pt.GetText(0, pt.GetTotalLength()) == "Start",
         "After second undo (back to start)");

  VERIFY(!pt.CanUndo(), "No more undos");
  VERIFY(pt.CanRedo(), "Should have redo entries");

  pt.Redo();
  VERIFY(pt.GetText(0, pt.GetTotalLength()) == "StartABC",
         "After first redo");

  pt.Redo();
  VERIFY(pt.GetText(0, pt.GetTotalLength()) == "StartABCDEF",
         "After second redo");

  VERIFY(!pt.CanRedo(), "No more redos");

  std::cout << "Test Passed: Undo/redo branching" << std::endl;
}

int main() {
  try {
    TestLineCounting();
    TestLineOffsets();
    TestGetLineAtOffset();
    TestLineOffsetsAfterEdit();
    TestWriteToCallback();
    TestEmptyOriginal();
    TestInsertDeleteInterleaved();
    TestCompactAfterEdits();
    TestLineCacheRebuild();
    TestUndoRedoBranching();
    std::cout << "\n=== ALL ADVANCED PIECE TABLE TESTS PASSED ===" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test suite failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
