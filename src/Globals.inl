// =============================================================================
// Globals.inl
// Common headers, command IDs, and global variable declarations
// Included by main.cpp
// =============================================================================
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef STRICT
#define STRICT
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

#include <windows.h>
#define _INC_WINDOWS_ENFORCED
#include <algorithm>
#include <commctrl.h>
#include <filesystem>
#include <fstream>
#include <imm.h>
#include <iostream>
#include <memory>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

#include "../include/Dialogs.h"
#include "../include/Editor.h"
#include "../include/EditorBufferRenderer.h"
#include "../include/Localization.h"
#include "../include/LspClient.h"
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
typedef void (*LogCallback)(const std::string &msg, LogLevel level);
extern LogCallback g_logCallback;
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
#define IDM_EDIT_TOGGLE_BOX 213
#define IDM_EDIT_TAG_JUMP 214

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
#define IDM_SHELL_ENC_UTF8 504
#define IDM_SHELL_ENC_SJIS 505
#define IDM_EDIT_FIND_IN_FILES 506
#define IDM_TOOLS_AI_ASSISTANT 507
#define IDM_TOOLS_AI_CONSOLE 508
#define IDM_TOOLS_AI_SET_KEY 509
#define IDM_AI_MANAGER 510
#define IDM_AI_SETUP_WIZARD 511

#define IDM_LANG_EN 601
#define IDM_LANG_JP 602
#define IDM_LANG_ES 603
#define IDM_LANG_FR 604
#define IDM_LANG_DE 605

#define IDM_BUFFERS_LIST 701
#define IDM_TAB_COPY_PATH 702
#define IDM_RECENT_START 2000
#define IDM_BUFFERS_START 1000

#define IDM_HELP_DOC 801
#define IDM_HELP_ABOUT 802
#define IDM_HELP_MESSAGES 803
#define IDM_HELP_KEYBINDINGS 804

#define WM_SHELL_OUTPUT (WM_USER + 101)

struct ShellOutput {
  Buffer *buffer;
  std::string text;
  std::string callback;
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
extern Editor *g_editor;
extern EditorBufferRenderer *g_renderer;
extern ScriptEngine *g_scriptEngine;
extern LspClient *g_lspClient;
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
inline std::string WStringToString(const std::wstring &ws) {
  if (ws.empty())
    return "";
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &ws[0], (int)ws.size(),
                                        NULL, 0, NULL, NULL);
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &ws[0], (int)ws.size(), &strTo[0],
                      size_needed, NULL, NULL);
  return strTo;
}

inline std::wstring StringToWString(const std::string &s) {
  if (s.empty())
    return L"";
  int size_needed =
      MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
  std::wstring wstrTo(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &wstrTo[0],
                      size_needed);
  return wstrTo;
}

inline std::string GetWin32ErrorString(DWORD errorCode) {
  LPSTR messageBuffer = nullptr;
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&messageBuffer, 0, NULL);
  if (size > 0 && messageBuffer) {
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    // Remove trailing newlines
    while (!message.empty() &&
           (message.back() == '\r' || message.back() == '\n')) {
      message.pop_back();
    }
    return message;
  }
  return "Unknown error (" + std::to_string(errorCode) + ")";
}
