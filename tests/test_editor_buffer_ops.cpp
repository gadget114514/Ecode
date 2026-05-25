#include "../include/Editor.h"
#include "../include/Buffer.h"
#include <cassert>
#include <iostream>
#include <string>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

extern HWND g_mainHwnd;
extern Editor *g_editor;

void TestNewFile() {
  Editor editor;
  g_editor = &editor;

  VERIFY(editor.GetBuffers().empty(),
         "Editor should have no buffers initially");

  editor.NewFile("Untitled");
  VERIFY(editor.GetBuffers().size() == 1, "Should have 1 buffer after NewFile");
  VERIFY(editor.GetActiveBufferIndex() == 0,
         "Active buffer index should be 0");
  VERIFY(editor.GetActiveBuffer() != nullptr, "Active buffer should not be null");

  editor.NewFile("Scratch");
  VERIFY(editor.GetBuffers().size() == 2, "Should have 2 buffers");
  VERIFY(editor.GetActiveBufferIndex() == 1,
         "Active buffer should be the new one");

  auto *buf = editor.GetActiveBuffer();
  VERIFY(buf != nullptr, "Active buffer should exist");
  VERIFY(buf->IsScratch() == false, "New file should not be scratch by default");

  g_editor = nullptr;
  std::cout << "Test Passed: NewFile creates buffers" << std::endl;
}

void TestOpenFile() {
  Editor editor;
  g_editor = &editor;

  std::wstring tmpFile = L"test_ebo_open.tmp";
  {
    FILE *f = _wfopen(tmpFile.c_str(), L"wb");
    VERIFY(f != nullptr, "Failed to create temp file");
    fwrite("Open file test content\nLine 2\n", 1, 29, f);
    fclose(f);
  }

  size_t idx = editor.OpenFile(tmpFile);
  VERIFY(idx != (size_t)-1, "OpenFile should return valid index");
  VERIFY(editor.GetBuffers().size() == 1, "Should have 1 buffer after OpenFile");
  VERIFY(editor.GetActiveBufferIndex() == idx,
         "Active buffer should be the opened file");

  auto *buf = editor.GetActiveBuffer();
  VERIFY(buf != nullptr, "Active buffer should exist");
  std::string content = buf->GetText(0, buf->GetTotalLength());
  VERIFY(content.find("Open file test") != std::string::npos,
         "File content should be loaded");

  DeleteFileW(tmpFile.c_str());
  g_editor = nullptr;
  std::cout << "Test Passed: OpenFile loads content" << std::endl;
}

void TestSwitchToBuffer() {
  Editor editor;
  g_editor = &editor;

  editor.NewFile("Buffer A");
  editor.NewFile("Buffer B");
  editor.NewFile("Buffer C");

  VERIFY(editor.GetBuffers().size() == 3, "Should have 3 buffers");
  VERIFY(editor.GetActiveBufferIndex() == 2,
         "Active should be last created (C)");

  editor.SwitchToBuffer(0);
  VERIFY(editor.GetActiveBufferIndex() == 0, "After switch to 0, index should be 0");

  editor.SwitchToBuffer(1);
  VERIFY(editor.GetActiveBufferIndex() == 1, "After switch to 1, index should be 1");

  editor.SwitchToBuffer(2);
  VERIFY(editor.GetActiveBufferIndex() == 2, "After switch to 2, index should be 2");

  g_editor = nullptr;
  std::cout << "Test Passed: SwitchToBuffer" << std::endl;
}

void TestCloseBuffer() {
  Editor editor;
  g_editor = &editor;

  editor.NewFile("File A");
  editor.NewFile("File B");
  editor.NewFile("File C");
  VERIFY(editor.GetBuffers().size() == 3, "Should start with 3 buffers");

  editor.CloseBuffer(1);
  VERIFY(editor.GetBuffers().size() == 2, "Should have 2 buffers after close");

  editor.CloseBuffer(1);
  VERIFY(editor.GetBuffers().size() == 1, "Should have 1 buffer after second close");

  editor.CloseBuffer(0);
  VERIFY(editor.GetBuffers().empty(),
         "Should have 0 buffers after closing all");

  g_editor = nullptr;
  std::cout << "Test Passed: CloseBuffer" << std::endl;
}

void TestGetBufferByName() {
  Editor editor;
  g_editor = &editor;

  editor.NewFile("Alpha");
  editor.NewFile("Beta");
  editor.NewFile("Gamma");

  auto *found = editor.GetBufferByName(L"Alpha");
  VERIFY(found != nullptr, "GetBufferByName should find Alpha");
  VERIFY(found->GetPath().find(L"Alpha") != std::wstring::npos,
         "Found buffer should have matching name");

  found = editor.GetBufferByName(L"Gamma");
  VERIFY(found != nullptr, "GetBufferByName should find Gamma");

  found = editor.GetBufferByName(L"NonExistent");
  VERIFY(found == nullptr,
         "GetBufferByName should return null for non-existent name");

  g_editor = nullptr;
  std::cout << "Test Passed: GetBufferByName" << std::endl;
}

void TestIsValidBuffer() {
  Editor editor;
  g_editor = &editor;

  editor.NewFile("Buffer X");
  auto *buf = editor.GetActiveBuffer();
  VERIFY(buf != nullptr, "Active buffer should exist");
  VERIFY(editor.IsValidBuffer(buf), "Active buffer should be valid");

  VERIFY(!editor.IsValidBuffer(nullptr),
         "Null buffer should not be valid");

  auto *stale = new Buffer();
  VERIFY(!editor.IsValidBuffer(stale),
         "Unmanaged buffer should not be valid");
  delete stale;

  g_editor = nullptr;
  std::cout << "Test Passed: IsValidBuffer" << std::endl;
}

void TestMultipleFileOpen() {
  Editor editor;
  g_editor = &editor;

  std::wstring files[] = {L"test_ebo_multi_1.tmp", L"test_ebo_multi_2.tmp",
                           L"test_ebo_multi_3.tmp"};
  for (auto &f : files) {
    FILE *fh = _wfopen(f.c_str(), L"wb");
    std::string tag = "Content of " + std::string(f.begin(), f.end());
    fwrite(tag.c_str(), 1, tag.size(), fh);
    fclose(fh);
  }

  size_t idx1 = editor.OpenFile(files[0]);
  size_t idx2 = editor.OpenFile(files[1]);
  size_t idx3 = editor.OpenFile(files[2]);

  VERIFY(editor.GetBuffers().size() == 3, "Should have 3 file buffers");
  VERIFY(idx1 == 0, "First file should be at index 0");
  VERIFY(idx2 == 1, "Second file should be at index 1");
  VERIFY(idx3 == 2, "Third file should be at index 2");

  editor.SwitchToBuffer(0);
  auto *b0 = editor.GetActiveBuffer();
  VERIFY(b0->GetPath().find(files[0]) != std::wstring::npos,
         "Buffer 0 should reference first file");

  for (auto &f : files)
    DeleteFileW(f.c_str());

  g_editor = nullptr;
  std::cout << "Test Passed: Multiple file open" << std::endl;
}

int main() {
  try {
    TestNewFile();
    TestOpenFile();
    TestSwitchToBuffer();
    TestCloseBuffer();
    TestGetBufferByName();
    TestIsValidBuffer();
    TestMultipleFileOpen();
    std::cout << "\n=== ALL EDITOR BUFFER OPERATIONS TESTS PASSED ==="
              << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test suite failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
