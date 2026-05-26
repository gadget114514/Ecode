#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <windows.h>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

extern bool SafeSave(const std::wstring &targetPath, const std::string &content);
extern bool SafeSaveStreaming(
    const std::wstring &targetPath,
    const std::function<void(std::function<void(const char *, size_t)>)>
        &source);

static bool FileContentsEqual(const std::wstring &path,
                               const std::string &expected) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  DWORD size = GetFileSize(h, NULL);
  std::string buf(size, '\0');
  DWORD read;
  bool ok = ReadFile(h, &buf[0], size, &read, NULL) && read == size;
  CloseHandle(h);
  return ok && buf == expected;
}

void TestSafeSaveNewFile() {
  std::wstring path = L"test_fu_new.tmp";
  DeleteFileW(path.c_str());

  std::string content = "Hello from SafeSave!";
  VERIFY(SafeSave(path, content), "SafeSave new file should succeed");
  VERIFY(FileContentsEqual(path, content), "Content mismatch");

  DeleteFileW(path.c_str());
  std::cout << "Test Passed: SafeSave new file" << std::endl;
}

void TestSafeSaveOverwrite() {
  std::wstring path = L"test_fu_over.tmp";
  {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written;
    WriteFile(h, "old content", 11, &written, NULL);
    CloseHandle(h);
  }

  std::string newContent = "new content here";
  VERIFY(SafeSave(path, newContent), "SafeSave overwrite should succeed");
  VERIFY(FileContentsEqual(path, newContent), "Overwritten content mismatch");

  DeleteFileW(path.c_str());
  std::cout << "Test Passed: SafeSave overwrite" << std::endl;
}

void TestSafeSaveEmpty() {
  std::wstring path = L"test_fu_empty.tmp";
  DeleteFileW(path.c_str());

  VERIFY(SafeSave(path, ""), "SafeSave empty content should succeed");
  VERIFY(FileContentsEqual(path, ""), "Empty file content mismatch");

  DeleteFileW(path.c_str());
  std::cout << "Test Passed: SafeSave empty content" << std::endl;
}

void TestSafeSaveLarge() {
  std::wstring path = L"test_fu_large.tmp";
  DeleteFileW(path.c_str());

  std::string content(100000, 'X');
  content += "ENDMARKER";
  VERIFY(SafeSave(path, content), "SafeSave large file should succeed");
  VERIFY(FileContentsEqual(path, content), "Large file content mismatch");

  DeleteFileW(path.c_str());
  std::cout << "Test Passed: SafeSave large content (100KB)" << std::endl;
}

void TestSafeSaveMultipleVersions() {
  std::wstring path = L"test_fu_multi.tmp";
  DeleteFileW(path.c_str());

  for (int i = 1; i <= 5; ++i) {
    std::string content = "Version " + std::to_string(i);
    VERIFY(SafeSave(path, content), "SafeSave version " +
                                        std::to_string(i) + " should succeed");
    VERIFY(FileContentsEqual(path, content),
           "Version " + std::to_string(i) + " content mismatch");
  }

  DeleteFileW(path.c_str());
  std::cout << "Test Passed: SafeSave multiple versions" << std::endl;
}

void TestSafeSaveStreamingBasic() {
  std::wstring path = L"test_fu_stream.tmp";
  DeleteFileW(path.c_str());

  std::string part1 = "Hello ";
  std::string part2 = "Streaming ";
  std::string part3 = "World!";
  std::string full = part1 + part2 + part3;

  VERIFY(SafeSaveStreaming(path,
                           [&](auto writer) {
                             writer(part1.c_str(), part1.size());
                             writer(part2.c_str(), part2.size());
                             writer(part3.c_str(), part3.size());
                           }),
         "SafeSaveStreaming should succeed");
  VERIFY(FileContentsEqual(path, full), "Streaming content mismatch");

  DeleteFileW(path.c_str());
  std::cout << "Test Passed: SafeSaveStreaming basic" << std::endl;
}

void TestSafeSaveStreamingLarge() {
  std::wstring path = L"test_fu_stream_large.tmp";
  DeleteFileW(path.c_str());

  std::string chunk(65536, 'A');
  std::string full;
  full.reserve(chunk.size() * 4);

  VERIFY(SafeSaveStreaming(path,
                           [&](auto writer) {
                             for (int i = 0; i < 4; ++i) {
                               writer(chunk.c_str(), chunk.size());
                               full += chunk;
                             }
                           }),
         "SafeSaveStreaming large should succeed");
  VERIFY(FileContentsEqual(path, full), "Streaming large content mismatch");

  DeleteFileW(path.c_str());
  std::cout << "Test Passed: SafeSaveStreaming large (256KB)" << std::endl;
}

int main() {
  try {
    TestSafeSaveNewFile();
    TestSafeSaveOverwrite();
    TestSafeSaveEmpty();
    TestSafeSaveLarge();
    TestSafeSaveMultipleVersions();
    TestSafeSaveStreamingBasic();
    TestSafeSaveStreamingLarge();
    std::cout << "\n=== ALL FILE UTILS TESTS PASSED ===" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test suite failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
