#include "../include/Buffer.h"
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

// Mock SafeSave/SafeSaveStreaming is NOT used here.
// We link with FileUtils.cpp and PieceTable.cpp for real logic.

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FILE IO FAILURE at line " << __LINE__ << ": " << msg         \
              << std::endl;                                                    \
    exit(1);                                                                   \
  }

void TestFileIO() {
  std::wstring testFile = L"test_io_temp.txt";
  std::string testContent =
      "Hello World\nThis is a test of file IO.\nEnjoy Ecode!";

  std::cout << "Starting File IO Tests..." << std::endl;

  // 1. Save Test
  {
    Buffer buf;
    buf.Insert(0, testContent);
    VERIFY(buf.SaveFile(testFile), "Failed to save file");
  }

  // 2. Load Test
  {
    Buffer buf;
    VERIFY(buf.OpenFile(testFile), "Failed to open file");
    VERIFY(buf.GetText(0, buf.GetTotalLength()) == testContent,
           "Loaded content mismatch");
    VERIFY(buf.GetTotalLines() == 3, "Line count mismatch");
  }

  // 3. Large File Test (Streaming)
  {
    std::cout << "Testing Large File Streaming IO..." << std::endl;
    std::string largeContent;
    // Create ~1MB of text
    for (int i = 0; i < 50000; ++i) {
      largeContent += "Line " + std::to_string(i) + " text content\n";
    }

    Buffer buf;
    bool progressCalled = false;
    buf.SetProgressCallback([&](float p) {
      if (p > 0)
        progressCalled = true;
    });

    buf.Insert(0, largeContent);
    VERIFY(buf.SaveFile(testFile), "Failed to save large file");
    VERIFY(progressCalled, "Progress callback was never called for large save");

    Buffer buf2;
    VERIFY(buf2.OpenFile(testFile), "Failed to open large file");
    VERIFY(buf2.GetTotalLength() == largeContent.length(),
           "Large file length mismatch");

    // Verify a sample in the middle
    std::string search = "Line 25000 text content\n";
    size_t mid = largeContent.find(search);
    VERIFY(buf2.GetText(mid, search.length()) == search,
           "Large file content mismatch at midpoint");
  }

  DeleteFileW(testFile.c_str());
  std::cout << "File IO Tests Passed!" << std::endl;
}

int main() {
  try {
    TestFileIO();
  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
