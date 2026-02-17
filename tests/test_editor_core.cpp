#include "../include/Buffer.h"
#include <cassert>
#include <iostream>
#include <string>

#include <functional>

// Mock for SafeSave as it used in Buffer.cpp
bool SafeSave(const std::wstring &targetPath, const std::string &content) {
  return true;
}

bool SafeSaveStreaming(
    const std::wstring &targetPath,
    const std::function<void(std::function<void(const char *, size_t)>)>
        &source) {
  // Just consume the data to avoid "unused" warnings or logic issues
  source([](const char *, size_t) {});
  return true;
}

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

void TestBufferFolding() {
  Buffer buf;
  std::string text = "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\n";
  buf.Insert(0, text);

  VERIFY(buf.GetTotalLines() == 6,
         "Initial lines mismatch"); // 5 lines + 1 empty after last \n
  VERIFY(buf.GetVisibleLineCount() == 6, "Initial visible lines mismatch");

  // Fold line 1 (0-indexed)
  buf.FoldLine(1);
  VERIFY(buf.GetVisibleLineCount() == 5, "Visible lines after fold mismatch");

  std::string visible = buf.GetVisibleText();
  VERIFY(visible.find("Line 2") == std::string::npos,
         "Folded line content still visible");
  VERIFY(visible.find("Line 1") != std::string::npos,
         "Line 1 should be visible");
  VERIFY(visible.find("Line 3") != std::string::npos,
         "Line 3 should be visible");

  // Test Offset Mapping
  // "Line 1\n" is 7 chars.
  // "Line 2\n" is 7 chars.
  // logical offset 0 (L) -> visual offset 0
  VERIFY(buf.LogicalToVisualOffset(0) == 0, "Mapping L0 failed");
  // logical offset 7 (\n after Line 1) -> visual offset 7
  VERIFY(buf.LogicalToVisualOffset(7) == 7, "Mapping L7 failed");
  // logical offset 8 (L in Line 2) is folded -> should return visual offset 7
  // (collapse to boundary)
  VERIFY(buf.LogicalToVisualOffset(8) == 7, "Mapping L8 failed");
  // logical offset 14 (\n after Line 2) is folded -> visual offset 7
  // This assert is redundant as L8 covers the start of the folded line.
  // The provided instruction removes it, which is fine.
  // logical 14 (Line 3 start) -> visual 7
  VERIFY(buf.LogicalToVisualOffset(14) == 7, "Mapping L14 failed");
  // Line 1: 0-6 (7 chars including \n)
  // Line 2: 7-13 (7 chars including \n)
  // Line 3: 14-20

  // logical 14 (Line 3 start) -> visual 7
  // This assert is redundant.
  // VERIFY(buf.LogicalToVisualOffset(14) == 7, "Mapping L14 failed");

  // Test point in middle of Line 3
  // logical 15 ('i' in Line 3) -> visual 8
  VERIFY(buf.LogicalToVisualOffset(15) == 8, "Mapping L15 failed");

  // Test VisualToLogical
  VERIFY(buf.VisualToLogicalOffset(0) == 0, "V0 -> L0 failed");
  VERIFY(buf.VisualToLogicalOffset(7) == 14,
         "V7 -> L14 failed"); // start of Line 3
  VERIFY(buf.VisualToLogicalOffset(8) == 15, "V8 -> L15 failed");

  std::cout << "Test Passed: Buffer Folding & Mapping" << std::endl;
}

void TestBufferSearchReplace() {
  Buffer buf;
  buf.Insert(0, "The quick brown fox jumps over the lazy dog dog");

  // Find
  size_t pos = buf.Find("fox", 0);
  VERIFY(pos == 16, "Find 'fox' failed");

  pos = buf.Find("dog", 0);
  VERIFY(pos == 40, "Find 'dog' failed");

  // Find backward
  pos = buf.Find("dog", 45, false);
  VERIFY(pos == 44, "Find 'dog' backward failed");
  // 012345678901234567890123456789012345678901234567
  // The quick brown fox jumps over the lazy dog dog
  //                                         ^   ^
  //                                         40  44

  pos = buf.Find("dog", 48, true);
  VERIFY(pos == std::string::npos, "Find 'dog' out of bounds failed");

  // Replace
  buf.Replace(16, 19, "cat"); // fox -> cat (end = 16 + 3 = 19)
  VERIFY(buf.GetText(0, buf.GetTotalLength()).find("cat") == 16,
         "Replace content mismatch");
  VERIFY(buf.GetText(0, buf.GetTotalLength()).find("fox") == std::string::npos,
         "Fox should be gone");

  std::cout << "Test Passed: Buffer Search & Replace" << std::endl;
}


void TestBufferShellHistory() {
  Buffer buf;
  buf.SetShell(true);
  buf.SetInputStart(0);

  // Initial state logic check
  // Simulate prompt "C:\> "
  buf.Insert(0, "C:\\> ");
  buf.SetInputStart(5);
  buf.SetCaretPos(5);

  // 1. Add history
  buf.AddShellHistory("ls");
  buf.AddShellHistory("cd ..");
  buf.AddShellHistory("dir");

  // 2. Test Up (traverse back)
  // Stack: [ls, cd .., dir] (newest at back)
  // Index starts at -1.

  // First UP -> "dir" (index 2)
  buf.ShellHistoryUp();
  // Expect "C:\> dir"
  std::string text = buf.GetText(0, buf.GetTotalLength());
  VERIFY(text == "C:\\> dir", "History Up 1 failed: expected 'dir'");

  // Second UP -> "cd .." (index 1)
  buf.ShellHistoryUp();
  text = buf.GetText(0, buf.GetTotalLength());
  VERIFY(text == "C:\\> cd ..", "History Up 2 failed: expected 'cd ..'");

  // Third UP -> "ls" (index 0)
  buf.ShellHistoryUp();
  text = buf.GetText(0, buf.GetTotalLength());
  VERIFY(text == "C:\\> ls", "History Up 3 failed: expected 'ls'");

  // Fourth UP -> "ls" (clamped at index 0)
  buf.ShellHistoryUp();
  text = buf.GetText(0, buf.GetTotalLength());
  VERIFY(text == "C:\\> ls", "History Up 4 failed: expected 'ls'");

  // 3. Test Down (traverse forward)
  
  // First DOWN -> "cd .." (index 1)
  buf.ShellHistoryDown();
  text = buf.GetText(0, buf.GetTotalLength());
  VERIFY(text == "C:\\> cd ..", "History Down 1 failed: expected 'cd ..'");

  // Second DOWN -> "dir" (index 2)
  buf.ShellHistoryDown();
  text = buf.GetText(0, buf.GetTotalLength());
  VERIFY(text == "C:\\> dir", "History Down 2 failed: expected 'dir'");

  // Third DOWN -> empty (index -1, reset)
  buf.ShellHistoryDown();
  text = buf.GetText(0, buf.GetTotalLength());
  // Assuming ShellHistoryDown clears input when going past new
  VERIFY(text == "C:\\> ", "History Down 3 failed: expected empty prompt");

  // Fourth DOWN -> empty
  buf.ShellHistoryDown();
  text = buf.GetText(0, buf.GetTotalLength());
  VERIFY(text == "C:\\> ", "History Down 4 failed: expected empty prompt");

  // 4. Test Add Duplicate and Reset
  buf.AddShellHistory("dir"); // Duplicate of last command, effectively
  // But our implementation allows duplicates if they are not consecutive *adds*.
  // Wait, my implementation:
  // if (!m_shellHistory.empty() && m_shellHistory.back() == cmd) return;
  // So adding "dir" again when "dir" is at back will do nothing to the vector, just reset index.
  
  buf.ShellHistoryUp();
  text = buf.GetText(0, buf.GetTotalLength());
  VERIFY(text == "C:\\> dir", "History Post-Add Up failed");

  std::cout << "Test Passed: Buffer Shell History" << std::endl;
}

int main() {
  try {
    TestBufferFolding();
    TestBufferSearchReplace();
    TestBufferShellHistory();
    std::cout << "=== ALL CORE TESTS PASSED ===" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Test suite failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
