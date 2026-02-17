#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <memory>
#include <functional>
#include <commctrl.h>
#include <commdlg.h>
#include <imm.h>

#include "../include/Editor.h"
#include "../include/Buffer.h"
#include "../include/EditorBufferRenderer.h"
#include "../include/SettingsManager.h"
#include "../include/resource.h"

// Mock ScriptEngine to avoid linking issues with duktape
class ScriptEngine {
public:
    ScriptEngine() : m_captureKeyboard(false), m_bypassCache(false) {}
    ~ScriptEngine() {}
    bool Initialize() { return true; }
    std::string Evaluate(const std::string&) { return ""; }
    bool RunFile(const std::wstring&) { return true; }
    void RegisterBinding(const std::string&, const std::string&) {}
    bool HandleBinding(const std::string&) { return false; }
    void SetCaptureKeyboard(bool capture) { m_captureKeyboard = capture; }
    bool IsKeyboardCaptured() const { return m_captureKeyboard; }
    void SetKeyHandler(const std::string&) {}
    bool HandleKeyEvent(const std::string&, bool) { return false; }
    void SetBypassCache(bool) {}
    void CompileAllScripts() {}
private:
    bool m_captureKeyboard;
    std::string m_keyHandler;
    bool m_bypassCache;
};

// Mock Globals required by inl files
std::unique_ptr<Editor> g_editor;
std::unique_ptr<ScriptEngine> g_scriptEngine;
std::unique_ptr<EditorBufferRenderer> g_renderer;
HWND g_mainHwnd = (HWND)0x1;
HWND g_statusHwnd = (HWND)0x2;
HWND g_progressHwnd = (HWND)0x3;
HWND g_tabHwnd = (HWND)0x4;
HWND g_minibufferHwnd = (HWND)0x5;
HWND g_minibufferPromptHwnd = (HWND)0x6;
bool g_minibufferVisible = false;
HWND g_hDlgFind = NULL;
bool g_isDragging = false;
UINT g_uFindMsgString = 0;
FINDREPLACEW g_fr;
WCHAR g_szFindWhat[256];
WCHAR g_szReplaceWith[256];
std::vector<std::string> g_minibufferHistory;
int g_historyIndex = -1;
WNDPROC g_oldMinibufferProc = NULL;
std::string g_minibufferPrompt;
int g_minibufferMode = 0;
std::string g_minibufferJsCallback;
// HWND g_minibufferPromptHwnd = (HWND)0x6; // Removed duplicate

// Mock functions used in inl files
void EnsureCaretVisible(HWND) {}
void UpdateScrollbars(HWND) {}
void HideMinibuffer() { g_minibufferVisible = false; }

// Dummy for Shell stuff
struct ShellOutput {
  Buffer *buffer;
  std::string text;
};

// Command IDs (normally in Globals.inl - copied here for standalone test)
#define IDM_FILE_NEW 101
#define IDM_FILE_OPEN 102
#define IDM_FILE_SAVE 103
#define IDM_FILE_SAVE_AS 104
#define IDM_FILE_CLOSE 105
#define IDM_FILE_EXIT 107
#define IDM_EDIT_UNDO 201
#define IDM_EDIT_CUT 203
#define IDM_EDIT_COPY 204
#define IDM_EDIT_PASTE 205
#define IDM_EDIT_FIND 207
#define IDM_EDIT_FIND_IN_FILES 506
#define IDM_VIEW_ZOOM_IN 302
#define IDM_VIEW_ZOOM_OUT 303
#define IDM_VIEW_TOGGLE_UI 301

// Helper for String conversion
static std::wstring StringToWString(const std::string &s) {
  int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
  if (len <= 0) return L"";
  std::wstring ws(len - 1, '\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
  return ws;
}

#include "../src/WindowHandlers_Input.inl"

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

int main() {
    g_editor = std::make_unique<Editor>();
    g_scriptEngine = std::make_unique<ScriptEngine>();
    g_renderer = std::make_unique<EditorBufferRenderer>();
    
    g_editor->NewFile();
    Buffer* buf = g_editor->GetActiveBuffer();
    buf->Insert(0, "Line 1\nLine 2\nLine 3\n");
    buf->SetCaretPos(0);
    
    // Test C-f (Forward char)
    g_getKeyState = [](int key) -> SHORT {
        if (key == VK_CONTROL) return 0x8000;
        return 0;
    };
    
    HandleKeyDown(g_mainHwnd, 'F', 0);
    VERIFY(buf->GetCaretPos() == 1, "C-f failed to move caret forward");
    
    // Test C-b (Backward char)
    HandleKeyDown(g_mainHwnd, 'B', 0);
    VERIFY(buf->GetCaretPos() == 0, "C-b failed to move caret backward");
    
    // Test C-n (Next line)
    HandleKeyDown(g_mainHwnd, 'N', 0);
    VERIFY(buf->GetLineAtOffset(buf->GetCaretPos()) == 1, "C-n failed to move to next line");

    // Test C-p (Previous line)
    HandleKeyDown(g_mainHwnd, 'P', 0);
    VERIFY(buf->GetLineAtOffset(buf->GetCaretPos()) == 0, "C-p failed to move to previous line");

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
    VERIFY(buf->GetLineAtOffset(buf->GetCaretPos()) == 0, "C-k should stay on same line");
    // Line 1 was "ine 1", after C-k it should be empty but the \n remains
    VERIFY(buf->GetText(0, 1) == "\n", "C-k failed to kill line content");

    std::cout << "test_shortcuts passed!" << std::endl;
    return 0;
}
