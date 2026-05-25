#include "../include/MemoryMappedFile.h"
#include <cassert>
#include <iostream>
#include <string>
#include <windows.h>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

void TestNonExistentFile() {
  MemoryMappedFile mmf;
  VERIFY(!mmf.Open(L"does_not_exist_xyz.txt"),
         "Opening non-existent file should return false");
  VERIFY(!mmf.IsOpen(), "IsOpen should be false after failed open");
  VERIFY(mmf.GetData() == nullptr, "GetData should be null after failed open");
  VERIFY(mmf.GetSize() == 0, "GetSize should be 0 after failed open");

  std::cout << "Test Passed: Non-existent file" << std::endl;
}

void TestEmptyFile() {
  std::wstring path = L"test_mmf_empty.tmp";
  {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    VERIFY(h != INVALID_HANDLE_VALUE, "Failed to create empty temp file");
    CloseHandle(h);
  }

  MemoryMappedFile mmf;
  VERIFY(mmf.Open(path.c_str()), "Opening empty file should succeed");
  VERIFY(mmf.IsOpen(), "IsOpen should be true after open");
  VERIFY(mmf.GetSize() == 0, "Empty file should have size 0");
  VERIFY(mmf.GetData() == nullptr, "GetData should be null for empty file");

  DeleteFileW(path.c_str());
  std::cout << "Test Passed: Empty file" << std::endl;
}

void TestReadContent() {
  std::wstring path = L"test_mmf_content.tmp";
  std::string content = "Hello MemoryMappedFile!\nSecond line.\n";
  {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    VERIFY(h != INVALID_HANDLE_VALUE, "Failed to create temp file");
    DWORD written;
    WriteFile(h, content.c_str(), (DWORD)content.size(), &written, NULL);
    VERIFY(written == content.size(), "Failed to write all bytes");
    CloseHandle(h);
  }

  MemoryMappedFile mmf;
  VERIFY(mmf.Open(path.c_str()), "Opening content file should succeed");
  VERIFY(mmf.IsOpen(), "IsOpen should be true");
  VERIFY(mmf.GetSize() == content.size(), "Size mismatch");
  VERIFY(mmf.GetData() != nullptr, "GetData should not be null");

  std::string mapped(mmf.GetData(), mmf.GetSize());
  VERIFY(mapped == content, "Content mismatch via GetData");

  DeleteFileW(path.c_str());
  std::cout << "Test Passed: Read content" << std::endl;
}

void TestDoubleOpen() {
  std::wstring path1 = L"test_mmf_d1.tmp";
  std::wstring path2 = L"test_mmf_d2.tmp";
  std::string a = "File A";
  std::string b = "File B";

  auto createFile = [](const std::wstring &p, const std::string &c) {
    HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written;
    WriteFile(h, c.c_str(), (DWORD)c.size(), &written, NULL);
    CloseHandle(h);
  };
  createFile(path1, a);
  createFile(path2, b);

  MemoryMappedFile mmf;
  VERIFY(mmf.Open(path1.c_str()), "First open should succeed");
  VERIFY(mmf.GetSize() == a.size(), "First file size mismatch");

  VERIFY(mmf.Open(path2.c_str()), "Second open should succeed (closes first)");
  VERIFY(mmf.GetSize() == b.size(), "Second file size mismatch");
  std::string mapped(mmf.GetData(), mmf.GetSize());
  VERIFY(mapped == b, "Second file content mismatch");

  DeleteFileW(path1.c_str());
  DeleteFileW(path2.c_str());
  std::cout << "Test Passed: Double open (reuse)" << std::endl;
}

void TestCloseReopen() {
  std::wstring path = L"test_mmf_close.tmp";
  std::string content = "Close and reopen test";
  {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written;
    WriteFile(h, content.c_str(), (DWORD)content.size(), &written, NULL);
    CloseHandle(h);
  }

  MemoryMappedFile mmf;
  VERIFY(mmf.Open(path.c_str()), "Open should succeed");
  VERIFY(mmf.IsOpen(), "Should be open");

  mmf.Close();
  VERIFY(!mmf.IsOpen(), "Should not be open after close");
  VERIFY(mmf.GetData() == nullptr, "GetData should be null after close");
  VERIFY(mmf.GetSize() == 0, "GetSize should be 0 after close");

  VERIFY(mmf.Open(path.c_str()), "Re-open should succeed");
  VERIFY(mmf.IsOpen(), "Should be open after re-open");
  VERIFY(mmf.GetSize() == content.size(), "Size mismatch after re-open");

  mmf.Close();
  DeleteFileW(path.c_str());
  std::cout << "Test Passed: Close and reopen" << std::endl;
}

void TestLargeFile() {
  std::wstring path = L"test_mmf_large.tmp";
  std::string content;
  content.reserve(256 * 1024);
  for (int i = 0; i < 10000; ++i) {
    content += "Line " + std::to_string(i) + ": abcdefghijklmnopqrstuvwxyz\n";
  }
  {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written;
    WriteFile(h, content.c_str(), (DWORD)content.size(), &written, NULL);
    CloseHandle(h);
  }

  MemoryMappedFile mmf;
  VERIFY(mmf.Open(path.c_str()), "Large file open should succeed");
  VERIFY(mmf.GetSize() == content.size(), "Large file size mismatch");

  std::string mapped(mmf.GetData(), mmf.GetSize());
  VERIFY(mapped == content, "Large file content mismatch");

  VERIFY(mapped.find("Line 5000:") != std::string::npos,
         "Large file should contain middle section");

  mmf.Close();
  DeleteFileW(path.c_str());
  std::cout << "Test Passed: Large file (256KB)" << std::endl;
}

int main() {
  try {
    TestNonExistentFile();
    TestEmptyFile();
    TestReadContent();
    TestDoubleOpen();
    TestCloseReopen();
    TestLargeFile();
    std::cout << "\n=== ALL MEMORY MAPPED FILE TESTS PASSED ===" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test suite failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
