// =============================================================================
// Globals.inl
// Common headers, command IDs, and global variable declarations
// Included by main.cpp
// =============================================================================

#include <algorithm>
#include <commctrl.h>
#include <fstream>
#include <imm.h>
#include <memory>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>

#include "../include/Dialogs.h"
#include "../include/Editor.h"
#include "../include/EditorBufferRenderer.h"
#include "../include/Localization.h"
#include "../include/ScriptEngine.h"
#include "../include/SettingsManager.h"

// Forward declarations
class Buffer;
void UpdateMenu(HWND hwnd);
void UpdateScrollbars(HWND hwnd);
void UpdateTabs(HWND hwnd);
void EnsureCaretVisible(HWND hwnd);
bool PromptSaveBuffer(HWND hwnd, Buffer *buf);
void HideMinibuffer();

enum LogLevel { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };
extern int g_currentLogLevel;
void DebugLog(const std::string &msg, LogLevel level = LOG_INFO);

// Window handlers
LRESULT HandleCreate(HWND hwnd);
LRESULT HandleSize(HWND hwnd, LPARAM lParam);
LRESULT HandlePaint(HWND hwnd);
LRESULT HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam);
LRESULT HandleKeyDown(HWND hwnd, WPARAM wParam, LPARAM lParam);
LRESULT HandleChar(HWND hwnd, WPARAM wParam);
LRESULT HandleMouseDown(HWND hwnd, LPARAM lParam);
LRESULT HandleMouseMove(HWND hwnd, LPARAM lParam);
LRESULT HandleVScroll(HWND hwnd, WPARAM wParam);
LRESULT HandleHScroll(HWND hwnd, WPARAM wParam);
LRESULT HandleFindReplace(HWND hwnd, LPARAM lParam);
LRESULT HandleClose(HWND hwnd);
void HandleDestroy(HWND hwnd);

// Command IDs
#define IDM_FILE_NEW 101
#define IDM_FILE_OPEN 102
#define IDM_FILE_SAVE 103
#define IDM_FILE_SAVE_AS 104
#define IDM_FILE_CLOSE 105
#define IDM_FILE_SCRATCH 106
#define IDM_FILE_EXIT 107

#define IDM_EDIT_UNDO 201
#define IDM_EDIT_REDO 202
#define IDM_EDIT_CUT 203
#define IDM_EDIT_COPY 204
#define IDM_EDIT_PASTE 205
#define IDM_EDIT_SELECT_ALL 206
#define IDM_EDIT_FIND 207
#define IDM_EDIT_FIND_NEXT 208
#define IDM_EDIT_FIND_PREV 209
#define IDM_EDIT_REPLACE 210
#define IDM_EDIT_REPLACE_ALL 211
#define IDM_EDIT_GOTO 212

#define IDM_VIEW_TOGGLE_UI 301
#define IDM_VIEW_ZOOM_IN 302
#define IDM_VIEW_ZOOM_OUT 303
#define IDM_VIEW_ZOOM_RESET 304

#define IDM_CONFIG_SETTINGS 401
#define IDM_CONFIG_THEME 402
#define IDM_CONFIG_EDIT_INIT 403

#define IDM_TOOLS_RUN_MACRO 501
#define IDM_TOOLS_CONSOLE 502
#define IDM_TOOLS_MACRO_GALLERY 503

#define IDM_LANG_EN 601
#define IDM_LANG_JP 602
#define IDM_LANG_ES 603
#define IDM_LANG_FR 604
#define IDM_LANG_DE 605

#define IDM_BUFFERS_LIST 701
#define IDM_RECENT_START 2000
#define IDM_BUFFERS_START 1000

#define IDM_HELP_DOC 801
#define IDM_HELP_ABOUT 802
#define IDM_HELP_MESSAGES 803

#define WM_SHELL_OUTPUT (WM_USER + 101)

struct ShellOutput {
  Buffer *buffer;
  std::string text;
};

// Global objects (externs)
extern HWND g_mainHwnd;
extern HWND g_statusHwnd;
extern HWND g_progressHwnd;
extern HWND g_tabHwnd;
extern HWND g_minibufferHwnd;
extern HWND g_minibufferPromptHwnd;
extern bool g_minibufferVisible;
extern std::string g_minibufferPrompt;

enum MinibufferMode { MB_EVAL = 0, MB_MX_COMMAND = 1, MB_CALLBACK = 2 };
extern int g_minibufferMode;
extern std::string g_minibufferJsCallback;
extern std::unique_ptr<Editor> g_editor;
extern std::unique_ptr<EditorBufferRenderer> g_renderer;
extern std::unique_ptr<ScriptEngine> g_scriptEngine;
extern bool g_isDragging;
extern UINT g_uFindMsgString;
extern FINDREPLACEW g_fr;
extern WCHAR g_szFindWhat[256];
extern WCHAR g_szReplaceWith[256];
extern HWND g_hDlgFind;

extern std::vector<std::string> g_minibufferHistory;
extern int g_historyIndex;
extern WNDPROC g_oldMinibufferProc;

// Utility functions
static std::wstring StringToWString(const std::string &s) {
  int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
  if (len <= 0)
    return L"";
  std::wstring ws(len - 1, '\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
  return ws;
}
