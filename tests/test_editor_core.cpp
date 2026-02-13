#include "../include/Buffer.h"
#include <cassert>
#include <iostream>
#include <string>

// Mock for SafeSave as it used in Buffer.cpp
bool SafeSave(const std::wstring &targetPath, const std::string &content) {
  return true;
}

void TestBufferFolding() {
  Buffer buf;
  std::string text = "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\n";
  buf.Insert(0, text);

  assert(buf.GetTotalLines() == 6); // 5 lines + 1 empty after last \n
  assert(buf.GetVisibleLineCount() == 6);

  // Fold line 1 (0-indexed)
  buf.FoldLine(1);
  assert(buf.GetVisibleLineCount() == 5);

  std::string visible = buf.GetVisibleText();
  assert(visible.find("Line 2") == std::string::npos);
  assert(visible.find("Line 1") != std::string::npos);
  assert(visible.find("Line 3") != std::string::npos);

  // Test Offset Mapping
  // "Line 1\n" is 7 chars.
  // "Line 2\n" is 7 chars.
  // logical offset 0 (L) -> visual offset 0
  assert(buf.LogicalToVisualOffset(0) == 0);
  // logical offset 7 (\n after Line 1) -> visual offset 7
  assert(buf.LogicalToVisualOffset(7) == 7);
  // logical offset 8 (L in Line 2) is folded -> should return visual offset 7
  // (collapse to boundary)
  assert(buf.LogicalToVisualOffset(8) == 7);
  // logical offset 14 (\n after Line 2) is folded -> visual offset 7
  assert(buf.LogicalToVisualOffset(14) == 7);
  // logical 14 (Line 3 start) -> visual 7
  assert(buf.LogicalToVisualOffset(14) == 7);
  // Line 1: 0-6 (7 chars including \n)
  // Line 2: 7-13 (7 chars including \n)
  // Line 3: 14-20

  // logical 14 (Line 3 start) -> visual 7
  assert(buf.LogicalToVisualOffset(14) == 7);

  // Test point in middle of Line 3
  // logical 15 ('i' in Line 3) -> visual 8
  assert(buf.LogicalToVisualOffset(15) == 8);

  // Test VisualToLogical
  assert(buf.VisualToLogicalOffset(0) == 0);
  assert(buf.VisualToLogicalOffset(7) == 14); // start of Line 3
  assert(buf.VisualToLogicalOffset(8) == 15);

  std::cout << "Test Passed: Buffer Folding & Mapping" << std::endl;
}

void TestBufferSearchReplace() {
  Buffer buf;
  buf.Insert(0, "The quick brown fox jumps over the lazy dog dog");

  // Find
  size_t pos = buf.Find("fox", 0);
  assert(pos == 16);

  pos = buf.Find("dog", 0);
  assert(pos == 40);

  // Find backward
  pos = buf.Find("dog", 45, false);
  assert(pos == 44);
  // 012345678901234567890123456789012345678901234567
  // The quick brown fox jumps over the lazy dog dog
  //                                         ^   ^
  //                                         40  44

  pos = buf.Find("dog", 48, true);
  assert(pos == std::string::npos);

  // Replace
  buf.Replace(16, 19, "cat"); // fox -> cat
  assert(buf.GetText(0, buf.GetTotalLength()).find("cat") == 16);
  assert(buf.GetText(0, buf.GetTotalLength()).find("fox") == std::string::npos);

  std::cout << "Test Passed: Buffer Search & Replace" << std::endl;
}

int main() {
  try {
    TestBufferFolding();
    TestBufferSearchReplace();
    std::cout << "=== ALL CORE TESTS PASSED ===" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test suite failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
