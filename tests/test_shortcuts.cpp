#include "../src/Globals.inl"
#include <functional>
#include <imm.h>
#include <iostream>
#include <memory>

// Note: TestGlobals.cpp is already linked in via CMake

// Mock functions used in inl files
void EnsureCaretVisible(HWND) {}
void UpdateScrollbars(HWND) {}
void HideMinibuffer() { g_minibufferVisible = false; }

// ShellOutput is now in Globals.inl

// ScriptEngine stubs for test_shortcuts
std::string ScriptEngine::Evaluate(const std::string &) { return ""; }
bool ScriptEngine::HandleBinding(const std::string &) { return false; }
bool ScriptEngine::HandleKeyEvent(const std::string &, bool) { return false; }
void ScriptEngine::RegisterBinding(const std::string &, const std::string &) {}
void ScriptEngine::CompileAllScripts() {}
void ScriptEngine::CallGlobalFunction(const std::string &,
                                      const std::string &) {}
bool ScriptEngine::RunFile(const std::wstring &) { return true; }

#include "../src/WindowHandlers_Input.inl"

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

int main() {
  g_editor = new Editor();
  g_scriptEngine = (ScriptEngine *)new char[sizeof(
      ScriptEngine)]; // Minimal hack since we don't link full ScriptEngine
  g_renderer = new EditorBufferRenderer();

  g_editor->NewFile();
  Buffer *buf = g_editor->GetActiveBuffer();
  buf->Insert(0, "Line 1\nLine 2\nLine 3\n");
  buf->SetCaretPos(0);

  // Test C-f (Forward char)
  g_getKeyState = [](int key) -> SHORT {
    if (key == VK_CONTROL)
      return 0x8000;
    return 0;
  };

  HandleKeyDown(g_mainHwnd, 'F', 0);
  VERIFY(buf->GetCaretPos() == 1, "C-f failed to move caret forward");

  // Test C-b (Backward char)
  HandleKeyDown(g_mainHwnd, 'B', 0);
  VERIFY(buf->GetCaretPos() == 0, "C-b failed to move caret backward");

  // Test C-n (Next line)
  HandleKeyDown(g_mainHwnd, 'N', 0);
  VERIFY(buf->GetLineAtOffset(buf->GetCaretPos()) == 1,
         "C-n failed to move to next line");

  // Test C-p (Previous line)
  HandleKeyDown(g_mainHwnd, 'P', 0);
  VERIFY(buf->GetLineAtOffset(buf->GetCaretPos()) == 0,
         "C-p failed to move to previous line");

  // Test C-a (Start of line)
  buf->SetCaretPos(5);
  HandleKeyDown(g_mainHwnd, 'A', 0);
  VERIFY(buf->GetCaretPos() == 0, "C-a failed to move to start of line");

  // Test C-e (End of line)
  HandleKeyDown(g_mainHwnd, 'E', 0);
  // "Line 1\n" -> End is 6 (before \n)
  VERIFY(buf->GetCaretPos() == 6, "C-e failed to move to end of line");

  // Test C-d (Delete char)
  buf->SetCaretPos(0);
  HandleKeyDown(g_mainHwnd, 'D', 0);
  VERIFY(buf->GetText(0, 4) == "ine ", "C-d failed to delete character 'L'");

  // Test Kill Line (C-k)
  buf->SetCaretPos(0);
  HandleKeyDown(g_mainHwnd, 'K', 0);
  VERIFY(buf->GetLineAtOffset(buf->GetCaretPos()) == 0,
         "C-k should stay on same line");
  // Line 1 was "ine 1", after C-k it should be empty but the \n remains
  VERIFY(buf->GetText(0, 1) == "\n", "C-k failed to kill line content");

  std::cout << "test_shortcuts passed!" << std::endl;

  delete g_editor;
  delete g_renderer;
  // delete g_scriptEngine; // Be careful with the hack above

  return 0;
}
