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
#include <dwmapi.h>
#ifndef HTMINBUTTON
#define HTMINBUTTON 8
#endif
#ifndef HTMAXBUTTON
#define HTMAXBUTTON 9
#endif
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
#include "../include/resource.h"

// Forward declarations
class Buffer;
void UpdateMenu(HWND hwnd);
void UpdateScrollbars(HWND hwnd);
void UpdateTabs(HWND hwnd);
void EnsureCaretVisible(HWND hwnd);
bool PromptSaveBuffer(HWND hwnd, Buffer *buf);
void HideMinibuffer();
void ShowCliDialog(HWND hwnd);
void ShowAppEntriesDialog(HWND hwnd);
void ShowFolderEntriesDialog(HWND hwnd);
void KillAppProcessByIndex(HWND hwnd, size_t idx);
void KillActiveAppProcess(HWND hwnd);
void ScanPlugins();
void LaunchPlugin(HWND hwnd, size_t index);
void ShowPluginConfigDialog(HWND hwnd);
void ShowTabSwitcher(HWND parentHwnd);
void HideTabSwitcher();

enum LogLevel { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };
extern int g_currentLogLevel;
typedef void (*LogCallback)(const std::string &msg, LogLevel level);
extern LogCallback g_logCallback;
void DebugLog(const std::string &msg, LogLevel level = LOG_INFO);

// Top bar functions (defined in TopBar.inl)
void DrawTopBar(HDC hdc, HWND hwnd);
void HandleTopBarClick(HWND hwnd, int x, int y);
bool HandleTopBarButtonDown(HWND hwnd, int x, int y);
void HandleTopBarButtonUp(HWND hwnd);
void HandleTopBarDoubleClick(HWND hwnd);
void HandleTopBarRightClick(HWND hwnd, int x, int y);
bool HandleTopBarSysKeyDown(HWND hwnd, WPARAM wParam);

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
#define IDM_FILE_RELOAD 108
#define IDM_FILE_CLEAR_AUTOSAVE 109

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
#define IDM_VIEW_TABSWITCHER 305

#define HOTKEY_ID_TABSWITCHER 1001

#define IDM_CONFIG_SETTINGS 401
#define IDM_CONFIG_THEME 402
#define IDM_CONFIG_EDIT_INIT 403

#define IDM_TOOLS_RUN_MACRO 501
#define IDM_TOOLS_CONSOLE 502
#define IDM_TOOLS_MACRO_GALLERY 503
#define IDM_SHELL_ENC_UTF8 504
#define IDM_SHELL_ENC_SJIS 505
#define IDM_EDIT_FIND_IN_FILES 506
#define IDM_EDIT_GREP 507
#define IDM_EDIT_FIND_FILE 215
#define IDM_TOOLS_DIRED 216
#define IDM_TOOLS_AI_ASSISTANT 508
#define IDM_TOOLS_AI_CONSOLE 509
#define IDM_TOOLS_AI_SET_KEY 510
#define IDM_AI_MANAGER 511
#define IDM_AI_SETUP_WIZARD 512
#define IDM_TOOLS_TERMINAL 513
#define IDM_TOOLS_SHELL_MODE 514
#define IDM_TOOLS_TERMINAL_CMD 515
#define IDM_TOOLS_TERMINAL_BASH 516
#define IDM_TOOLS_FILE_SEARCH 517
#define IDM_TOOLS_CSV_EDITOR 518
#define IDM_TOOLS_JY_EDITOR 519
#define IDM_PROCESS_KILL 520
#define IDM_PROCESS_KILL_START 7000

#define IDM_LANG_EN 601
#define IDM_LANG_JP 602
#define IDM_LANG_ES 603
#define IDM_LANG_FR 604
#define IDM_LANG_DE 605

#define IDM_BUFFERS_LIST 701
#define IDM_TAB_COPY_PATH 702
#define IDM_TAB_CLOSE_TERMINAL 703
#define IDM_CLI_CONFIGURE 704
#define IDM_TAB_RELOAD 711
#define IDM_TAB_SAVE 712
#define IDM_TAB_SAVE_AS 713
#define IDM_APPS_CONFIGURE 708
#define IDM_RECENT_START 2000
#define IDM_CLI_START 3000
#define IDM_FILE_OPEN_FOLDER 709
#define IDM_FOLDER_CONFIGURE 710
#define IDM_APPS_START 9000
#define IDM_FOLDERS_START 9500
#define IDM_TERMINALS_START 4000
#define IDM_DIRED_START 5000
#define IDM_DIRED_CONFIGURE 705
#define IDM_PLUGINS_CONFIGURE 706
#define IDM_PLUGINS_RESCAN 707
#define IDM_BUFFERS_START 1000
#define IDM_THEME_START 6000
#define IDM_PLUGINS_START 8000

#define IDM_HELP_DOC 801
#define IDM_HELP_ABOUT 802
#define IDM_HELP_MESSAGES 803
#define IDM_HELP_KEYBINDINGS 804
#define IDM_HELP_COPYRIGHT 805
#define IDM_HELP_CLEAR_MESSAGES 806

#define WM_SHELL_OUTPUT (WM_USER + 101)
#define WM_EMBED_APP (WM_USER + 200)
#define WM_EMBED_TERMINAL (WM_USER + 201)
#define WM_GREP_RESULT   (WM_USER + 202)
#define WM_GREP_PROGRESS (WM_USER + 203)
#define WM_GREP_COMPLETE (WM_USER + 204)
#define WM_GREP_CURRENT_FILE (WM_USER + 205)
#define WM_DEFERRED_FOCUS (WM_USER + 207)
#define WM_SET_PROCESS_HANDLE (WM_USER + 206)
#define WM_TERMINAL_PROGRESS (WM_USER + 299)
#define WM_NOTIFY_APP_TERMINATED (WM_USER + 300)

// Forward declaration – defined in WindowHandlers_Command.inl
static void CALLBACK OnAppTerminated(PVOID lpParam, BOOLEAN TimerOrWaitFired);

struct GrepSearchParams {
    std::wstring dir;
    std::wstring pattern;
    std::wstring extFilter;
    bool useRegex;
    bool matchCase;
    bool showCurrentFile;
    bool verbose;
};
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
constexpr int TAB_TYPE_TERMINAL = 10;

struct PluginEntry {
    std::wstring name;
    std::wstring path;
    bool hidden;
};
extern std::vector<PluginEntry> g_plugins;

// Top bar / no-title-bar mode
extern bool g_noTitleBar;
extern int g_topBarHeight;
struct MenuLabel {
    std::wstring label;
    HMENU        popup;
    RECT         rect; // client-relative
};
extern std::vector<MenuLabel> g_menuLabels;
extern RECT g_minButtonRect;
extern RECT g_maxButtonRect;
extern RECT g_closeButtonRect;
extern bool g_isActive;
extern bool g_menuTracking; // true while TrackPopupMenu is running (suppresses WM_NCACTIVATE flicker)
extern std::wstring g_windowTitle;
extern HMENU g_hSysMenu;
extern int g_topBarButtonPushed; // 0=none, 1=min, 2=max, 3=close

constexpr int TAB_ICON_GRAY   = 0;
constexpr int TAB_ICON_BLUE   = 1;
constexpr int TAB_ICON_GREEN  = 2;
constexpr int TAB_ICON_ORANGE = 3;
constexpr int TAB_ICON_PURPLE = 4;
constexpr int TAB_ICON_TEAL   = 5;
constexpr int TAB_ICON_RED    = 6;
constexpr int TAB_ICON_PINK   = 7;
constexpr int TAB_ICON_YELLOW = 8;
constexpr int TAB_ICON_CYAN   = 9;
constexpr int TAB_ICON_LIME   = 10;
constexpr int TAB_ICON_BROWN  = 11;
constexpr int TAB_ICON_COUNT  = 12;

extern HIMAGELIST g_tabImageList;
extern const wchar_t* g_tabIconNames[TAB_ICON_COUNT];

struct AppTabInfo {
    HWND hwnd = nullptr;
    std::wstring label;
    int type; // 0 = file search, 1 = grep results, 5 = dired, 10 = terminal
    int iImage = -1;

    void *data = nullptr; // type-specific data (e.g. ListView HWND or result list)
    HANDLE hProcess = nullptr; // process handle for killing
    HANDLE hWaitObject = nullptr; // RegisterWaitForSingleObject registration
    std::wstring command;   // CLI command (for terminal tabs)
    std::wstring directory; // working directory (for terminal tabs)
};
extern std::vector<AppTabInfo> g_appTabs;
extern int g_activeAppTab; // -1 = no app tab active, 0+ = index in g_appTabs
extern volatile LONG g_grepCancelFlag;
extern bool g_grepSearchActive;
extern bool g_isDragging;
extern bool g_isDraggingTab;
extern int  g_dragTabFrom;
extern bool g_suppressTabChange;
extern WNDPROC g_oldTabProc;
extern HFONT g_hTabFont;
extern HFONT g_hTabFontActive;
extern int g_lastTabFontStyle;
extern UINT g_uFindMsgString;

struct TabRef {
  bool isBuffer; // true = buffer tab, false = app tab
  int  index;    // index in Editor::m_buffers or g_appTabs
};
extern std::vector<TabRef> g_tabOrder;

inline void RebuildTabOrder() {
  g_tabOrder.clear();
  if (g_editor) {
    const auto &buffers = g_editor->GetBuffers();
    for (size_t i = 0; i < buffers.size(); i++)
      if (!buffers[i]->IsHidden())
        g_tabOrder.push_back({true, (int)i});
  }
  for (size_t i = 0; i < g_appTabs.size(); i++)
    g_tabOrder.push_back({false, (int)i});
}

// IME inline composition state
extern std::wstring g_imeComposition;   // current composition string (UTF-16)
extern std::vector<BYTE> g_imeCompAttr; // per-char attribute for composition
extern bool g_imeComposing;             // true during composition
extern std::string g_imeCompUtf8;       // composition as UTF-8 (for viewport insert)
extern size_t g_imeCompViewOffset;      // byte offset of composition in viewport text
extern size_t g_imeCompViewLen;         // byte length of composition in viewport text
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

inline size_t VisibleBufferCount() {
  size_t count = 0;
  for (auto &buf : g_editor->GetBuffers())
    if (!buf->IsHidden()) count++;
  return count;
}

inline int TabToBufferIndex(int tabIndex) {
  if (tabIndex >= 0 && tabIndex < (int)g_tabOrder.size() && g_tabOrder[tabIndex].isBuffer)
    return g_tabOrder[tabIndex].index;
  return -1;
}

inline int BufferToTabIndex(size_t bufferIndex) {
  for (size_t i = 0; i < g_tabOrder.size(); i++)
    if (g_tabOrder[i].isBuffer && g_tabOrder[i].index == (int)bufferIndex)
      return (int)i;
  return -1;
}
