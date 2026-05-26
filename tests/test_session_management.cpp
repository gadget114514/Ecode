#include "../include/SettingsManager.h"
#include "../include/Editor.h"
#include "../include/Buffer.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

// Forward declarations for globals defined in TestGlobals.cpp
extern HWND g_mainHwnd;
extern Editor *g_editor;

// -----------------------------------------------------------------------
// Test: SessionBufferState data structure
// -----------------------------------------------------------------------
void TestSessionBufferState() {
  SessionBufferState state;

  VERIFY(state.path.empty(), "Default path should be empty");
  VERIFY(state.caretPos == 0, "Default caretPos should be 0");
  VERIFY(state.selectionAnchor == 0, "Default selectionAnchor should be 0");
  VERIFY(state.scrollLine == 0, "Default scrollLine should be 0");
  VERIFY(state.scrollX == 0, "Default scrollX should be 0");
  VERIFY(state.desiredColumn == 0, "Default desiredColumn should be 0");
  VERIFY(state.encoding == 0, "Default encoding should be 0 (UTF-8)");
  VERIFY(state.isDirty == false, "Default isDirty should be false");
  VERIFY(state.isScratch == false, "Default isScratch should be false");
  VERIFY(state.isShell == false, "Default isShell should be false");
  VERIFY(state.foldedLines.empty(), "Default foldedLines should be empty");
  VERIFY(state.contentFile.empty(), "Default contentFile should be empty");

  state.path = L"C:\\test\\file.txt";
  state.caretPos = 42;
  state.selectionAnchor = 10;
  state.scrollLine = 5;
  state.scrollX = 3;
  state.desiredColumn = 20;
  state.encoding = 1;
  state.isDirty = true;
  state.isScratch = true;
  state.foldedLines.push_back(2);
  state.foldedLines.push_back(7);
  state.contentFile = L"buffer_0.txt";

  VERIFY(state.path == L"C:\\test\\file.txt", "path mismatch");
  VERIFY(state.caretPos == 42, "caretPos mismatch");
  VERIFY(state.selectionAnchor == 10, "selectionAnchor mismatch");
  VERIFY(state.scrollLine == 5, "scrollLine mismatch");
  VERIFY(state.scrollX == 3, "scrollX mismatch");
  VERIFY(state.desiredColumn == 20, "desiredColumn mismatch");
  VERIFY(state.encoding == 1, "encoding mismatch");
  VERIFY(state.isDirty == true, "isDirty mismatch");
  VERIFY(state.isScratch == true, "isScratch mismatch");
  VERIFY(state.foldedLines.size() == 2, "foldedLines size mismatch");
  VERIFY(state.foldedLines[0] == 2, "foldedLines[0] mismatch");
  VERIFY(state.foldedLines[1] == 7, "foldedLines[1] mismatch");
  VERIFY(state.contentFile == L"buffer_0.txt", "contentFile mismatch");

  std::cout << "Test Passed: SessionBufferState data structure" << std::endl;
}

// -----------------------------------------------------------------------
// Test: SessionInfo data structure
// -----------------------------------------------------------------------
void TestSessionInfo() {
  SessionInfo info;

  VERIFY(info.index == 0, "Default index should be 0");
  VERIFY(info.name.empty(), "Default name should be empty");
  VERIFY(info.time.empty(), "Default time should be empty");
  VERIFY(info.bufferCount == 0, "Default bufferCount should be 0");

  info.index = 3;
  info.name = L"My Session";
  info.time = L"2026-05-25 12:00:00";
  info.bufferCount = 5;

  VERIFY(info.index == 3, "index mismatch");
  VERIFY(info.name == L"My Session", "name mismatch");
  VERIFY(info.time == L"2026-05-25 12:00:00", "time mismatch");
  VERIFY(info.bufferCount == 5, "bufferCount mismatch");

  std::cout << "Test Passed: SessionInfo data structure" << std::endl;
}

// -----------------------------------------------------------------------
// Test: SettingsManager session directory methods
// -----------------------------------------------------------------------
void TestSessionDirectories() {
  auto &settings = SettingsManager::Instance();

  std::wstring sessDir = settings.GetSessionsDirectory();
  VERIFY(!sessDir.empty(), "Sessions directory should not be empty");
  VERIFY(sessDir.find(L"sessions") != std::wstring::npos,
         "Sessions directory should contain 'sessions'");

  // Directory should exist (created by GetSessionsDirectory)
  DWORD attr = GetFileAttributesW(sessDir.c_str());
  VERIFY(attr != INVALID_FILE_ATTRIBUTES, "Sessions directory should exist");
  VERIFY(attr & FILE_ATTRIBUTE_DIRECTORY,
         "Sessions directory should be a directory");

  std::wstring idxPath = settings.GetSessionIndexPath();
  VERIFY(!idxPath.empty(), "Index path should not be empty");
  VERIFY(idxPath.find(L"sessions.ini") != std::wstring::npos,
         "Index path should end with sessions.ini");

  std::wstring dir3 = settings.GetSessionDir(3);
  VERIFY(dir3.find(L"session_3") != std::wstring::npos,
         "Session dir should contain session_3");

  std::wstring dir0 = settings.GetSessionDir(0);
  VERIFY(dir0.find(L"session_0") != std::wstring::npos,
         "Session dir should contain session_0");
  VERIFY(dir0 != dir3, "Different session indices should give different dirs");

  std::cout << "Test Passed: Session directories" << std::endl;
}

// -----------------------------------------------------------------------
// Test: Session save and load cycle
// -----------------------------------------------------------------------
void TestSaveLoadSession() {
  auto &settings = SettingsManager::Instance();

  // Create an Editor with some buffers for testing session operations
  Editor editor;
  g_editor = &editor;

  // Create a file-backed buffer
  std::wstring tmpFile = L"test_session_file.txt";
  {
    FILE *f = _wfopen(tmpFile.c_str(), L"wb");
    VERIFY(f != nullptr, "Failed to create temp file");
    fwrite("Hello World\nSecond Line\nThird Line\n", 1, 35, f);
    fclose(f);
  }

  size_t fileBufIdx = editor.OpenFile(tmpFile);
  VERIFY(fileBufIdx != (size_t)-1, "Failed to open temp file");
  VERIFY(editor.GetBuffers().size() == 1, "Should have 1 buffer");

  // Modify buffer state
  auto *buf = editor.GetBuffers()[fileBufIdx].get();
  buf->SetCaretPos(6);
  buf->SetSelectionAnchor(0);
  buf->SetScrollLine(1);
  buf->FoldLine(2);

  // Create a scratch buffer
  editor.NewFile("*scratch*");
  size_t scratchIdx = editor.GetBuffers().size() - 1;
  auto *scratch = editor.GetBuffers()[scratchIdx].get();
  scratch->SetScratch(true);
  scratch->Insert(0, "Scratch content here");
  scratch->SetCaretPos(10);

  VERIFY(editor.GetBuffers().size() == 2, "Should have 2 buffers");

  // Save session
  int sessionIdx = settings.SaveSession(L"Test Session 1");
  VERIFY(sessionIdx >= 0, "SaveSession should return non-negative index");

  // Verify session directory and files exist
  std::wstring sessionDir = settings.GetSessionDir(sessionIdx);
  VERIFY(GetFileAttributesW(sessionDir.c_str()) != INVALID_FILE_ATTRIBUTES,
         "Session directory should exist after save");

  std::wstring iniPath = sessionDir + L"\\session.ini";
  VERIFY(GetFileAttributesW(iniPath.c_str()) != INVALID_FILE_ATTRIBUTES,
         "session.ini should exist after save");

  // Verify session is in the list
  auto sessions = settings.GetSessionList();
  bool found = false;
  for (const auto &s : sessions) {
    if (s.index == sessionIdx && s.name == L"Test Session 1") {
      found = true;
      VERIFY(s.bufferCount == 2, "Session should have 2 buffers");
      break;
    }
  }
  VERIFY(found, "Session should be in GetSessionList");

  // Load session (creates new editor buffers from saved state)
  // First close existing buffers to simulate fresh load
  while (editor.GetBuffers().size() > 0)
    editor.CloseBuffer(0);

  bool loaded = settings.LoadSession(sessionIdx);
  VERIFY(loaded, "LoadSession should return true");

  // Check that buffers were restored
  VERIFY(editor.GetBuffers().size() == 2,
         "Should have 2 buffers after load");

  // Check first buffer (file-backed) was restored
  auto *restoredFile = editor.GetBuffers()[0].get();
  VERIFY(restoredFile->GetCaretPos() == 6, "Restored file caretPos mismatch");
  auto folded = restoredFile->GetFoldedLines();
  VERIFY(folded.find(2) != folded.end(), "Folded line 2 should be restored");

  // Clean up
  settings.DeleteSession(sessionIdx);
  DeleteFileW(tmpFile.c_str());
  g_editor = nullptr;

  std::cout << "Test Passed: Session save/load cycle" << std::endl;
}

// -----------------------------------------------------------------------
// Test: Multiple session management
// -----------------------------------------------------------------------
void TestMultipleSessions() {
  auto &settings = SettingsManager::Instance();

  Editor editor;
  g_editor = &editor;

  editor.NewFile("File A");
  int idx1 = settings.SaveSession(L"Session Alpha");
  VERIFY(idx1 >= 0, "First session save failed");

  editor.NewFile("File B");
  int idx2 = settings.SaveSession(L"Session Beta");
  VERIFY(idx2 >= 0, "Second session save failed");
  VERIFY(idx2 != idx1, "Different sessions should have different indices");

  editor.NewFile("File C");
  int idx3 = settings.SaveSession(L"Session Gamma");
  VERIFY(idx3 >= 0, "Third session save failed");

  // List sessions
  auto sessions = settings.GetSessionList();
  VERIFY(sessions.size() >= 3, "Should have at least 3 sessions");

  int alphaCount = 0, betaCount = 0, gammaCount = 0;
  for (const auto &s : sessions) {
    if (s.name == L"Session Alpha") { alphaCount++; VERIFY(s.bufferCount == 1, "Alpha should have 1 buffer"); }
    if (s.name == L"Session Beta")  { betaCount++;  VERIFY(s.bufferCount == 2, "Beta should have 2 buffers"); }
    if (s.name == L"Session Gamma") { gammaCount++; VERIFY(s.bufferCount == 3, "Gamma should have 3 buffers"); }
  }
  VERIFY(alphaCount == 1, "Session Alpha not found or duplicate");
  VERIFY(betaCount == 1, "Session Beta not found or duplicate");
  VERIFY(gammaCount == 1, "Session Gamma not found or duplicate");

  // Delete middle session
  bool deleted = settings.DeleteSession(idx2);
  VERIFY(deleted, "DeleteSession should return true");

  // Verify it's gone from list
  sessions = settings.GetSessionList();
  for (const auto &s : sessions) {
    VERIFY(s.name != L"Session Beta", "Deleted session should not appear in list");
  }

  // Clean up remaining sessions
  settings.DeleteSession(idx1);
  settings.DeleteSession(idx3);

  g_editor = nullptr;

  std::cout << "Test Passed: Multiple session management" << std::endl;
}

// -----------------------------------------------------------------------
// Test: Editor buffer state serialization
// -----------------------------------------------------------------------
void TestBufferStateSerialization() {
  Editor editor;
  g_editor = &editor;

  editor.NewFile("Test Buffer");
  VERIFY(editor.GetBuffers().size() >= 1, "Editor should have buffers");

  auto *buf = editor.GetBuffers()[0].get();
  buf->Insert(0, "Line 1\nLine 2\nLine 3\nLine 4\n");
  buf->SetCaretPos(10);
  buf->SetSelectionAnchor(0);
  buf->SetScrollLine(2);
  buf->FoldLine(1);

  SessionBufferState state = editor.GetBufferState(0);
  VERIFY(state.caretPos == 10, "GetBufferState caretPos mismatch");
  VERIFY(state.selectionAnchor == 0, "GetBufferState selectionAnchor mismatch");
  VERIFY(state.scrollLine == 2, "GetBufferState scrollLine mismatch");
  VERIFY(state.foldedLines.size() == 1, "GetBufferState foldedLines size mismatch");
  VERIFY(state.foldedLines[0] == 1, "GetBufferState foldedLines[0] mismatch");

  // Modify buffer then restore
  buf->SetCaretPos(0);
  buf->SetScrollLine(0);
  buf->UnfoldLine(1);

  editor.RestoreBufferState(0, state);
  VERIFY(buf->GetCaretPos() == 10, "RestoreBufferState caretPos mismatch");
  VERIFY(buf->GetScrollLine() == 2, "RestoreBufferState scrollLine mismatch");
  VERIFY(buf->IsLineFolded(1), "RestoreBufferState fold should be restored");

  g_editor = nullptr;

  std::cout << "Test Passed: Buffer state serialization" << std::endl;
}

// -----------------------------------------------------------------------
// Test: Auto-save / restore cycle
// -----------------------------------------------------------------------
void TestAutoSaveCycle() {
  auto &settings = SettingsManager::Instance();

  // Clear any existing auto-save
  settings.ClearAutoSaveSession();

  VERIFY(settings.GetAutoSaveSessionIndex() == -1,
         "Auto-save index should be -1 after clear");

  Editor editor;
  g_editor = &editor;
  editor.NewFile("Auto Save Test");
  editor.GetBuffers()[0]->Insert(0, "Auto-saved content");

  // Simulate auto-save
  int idx = settings.SaveSession(L"*AutoSave*");
  VERIFY(idx >= 0, "Auto-save should succeed");
  settings.SetAutoSaveSessionIndex(idx);

  VERIFY(settings.GetAutoSaveSessionIndex() == idx,
         "Auto-save index should match saved index");

  // Verify auto-save is in the list
  auto sessions = settings.GetSessionList();
  bool found = false;
  for (const auto &s : sessions) {
    if (s.name == L"*AutoSave*") {
      found = true;
      break;
    }
  }
  VERIFY(found, "Auto-save session should be in the session list");

  // Clear auto-save
  settings.ClearAutoSaveSession();
  VERIFY(settings.GetAutoSaveSessionIndex() == -1,
         "Auto-save index should be -1 after clear");

  // Verify auto-save is gone from list
  sessions = settings.GetSessionList();
  for (const auto &s : sessions) {
    VERIFY(s.name != L"*AutoSave*", "Auto-save should be deleted after clear");
  }

  g_editor = nullptr;

  std::cout << "Test Passed: Auto-save/restore cycle" << std::endl;
}

// -----------------------------------------------------------------------
// Test: Edge cases
// -----------------------------------------------------------------------
void TestEdgeCases() {
  auto &settings = SettingsManager::Instance();

  Editor editor;
  g_editor = &editor;

  // 1. Load non-existent session
  bool loaded = settings.LoadSession(9999);
  VERIFY(!loaded, "Loading non-existent session should return false");

  // 2. Delete non-existent session
  bool deleted = settings.DeleteSession(9999);
  VERIFY(!deleted, "Deleting non-existent session should return false");

  // 3. Save empty session (no buffers)
  // Clear buffers so we save with 0 buffers
  while (editor.GetBuffers().size() > 0)
    editor.CloseBuffer(0);
  int idx = settings.SaveSession(L"Empty Session");
  VERIFY(idx >= 0, "Saving empty session should succeed");
  settings.DeleteSession(idx);

  // 4. Session with scratch buffer content
  editor.NewFile("scratch_test");
  auto *buf = editor.GetBuffers()[0].get();
  buf->SetScratch(true);
  buf->Insert(0, "Persistent scratch data");
  int scratchIdx = settings.SaveSession(L"Scratch Session");
  VERIFY(scratchIdx >= 0, "Scratch session save should succeed");

  // Verify content file was created for scratch buffer
  std::wstring contentPath =
      settings.GetSessionDir(scratchIdx) + L"\\buffer_0.txt";
  VERIFY(GetFileAttributesW(contentPath.c_str()) != INVALID_FILE_ATTRIBUTES,
         "Content file should exist for scratch buffer");

  settings.DeleteSession(scratchIdx);
  g_editor = nullptr;

  std::cout << "Test Passed: Edge cases" << std::endl;
}

// -----------------------------------------------------------------------
// Test: Session management setting toggle
// -----------------------------------------------------------------------
void TestSessionToggle() {
  auto &settings = SettingsManager::Instance();

  // Default should be disabled
  VERIFY(settings.IsSessionManagementEnabled() == false,
         "Session management should be disabled by default");

  // Toggle on
  settings.SetSessionManagementEnabled(true);
  VERIFY(settings.IsSessionManagementEnabled() == true,
         "Session management should be enabled after Set(true)");

  // Toggle off
  settings.SetSessionManagementEnabled(false);
  VERIFY(settings.IsSessionManagementEnabled() == false,
         "Session management should be disabled after Set(false)");

  std::cout << "Test Passed: Session management toggle" << std::endl;
}

// -----------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------
int main() {
  try {
    TestSessionBufferState();
    TestSessionInfo();
    TestSessionDirectories();
    TestBufferStateSerialization();
    TestSaveLoadSession();
    TestMultipleSessions();
    TestAutoSaveCycle();
    TestEdgeCases();
    TestSessionToggle();

    std::cout << "\n=== ALL SESSION MANAGEMENT TESTS PASSED ===" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test suite failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
