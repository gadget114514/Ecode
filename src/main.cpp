// =============================================================================
// main.cpp
// Ecode - Native Win32 Text Editor
// =============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define STRICT

#include <windows.h>
#include <windowsx.h>

#include "Globals.inl"

// Global implementations
HWND g_mainHwnd = NULL;
HWND g_statusHwnd = NULL;
HWND g_progressHwnd = NULL;
HWND g_tabHwnd = NULL;
HWND g_minibufferHwnd = NULL;
HWND g_minibufferPromptHwnd = NULL;
bool g_minibufferVisible = false;
std::string g_minibufferPrompt = ":";
int g_minibufferMode = MB_EVAL;
std::string g_minibufferJsCallback;
Editor *g_editor = nullptr;
EditorBufferRenderer *g_renderer = nullptr;
ScriptEngine *g_scriptEngine = nullptr;
LspClient *g_lspClient = nullptr;
LogCallback g_logCallback = nullptr;
std::wstring g_scriptsDir;
bool g_isDragging = false;
UINT g_uFindMsgString = 0;
FINDREPLACEW g_fr = {0};
WCHAR g_szFindWhat[256] = L"";
WCHAR g_szReplaceWith[256] = L"";
HWND g_hDlgFind = NULL;
std::vector<std::string> g_minibufferHistory;
int g_historyIndex = -1;
int g_currentLogLevel = LOG_INFO;
WNDPROC g_oldMinibufferProc = NULL;
bool g_bypassCache = false;
bool g_compileAllScripts = false;

#include "AppMain.inl"
#include "MinibufferHandler.inl"
#include "UIHelpers.inl"
#include "WindowHandlers_Command.inl"
#include "WindowHandlers_Core.inl"
#include "WindowHandlers_Input.inl"
