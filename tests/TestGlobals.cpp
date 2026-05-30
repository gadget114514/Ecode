#include "../src/Globals.inl"

// Define globals required by the editor core when linked in tests
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
LogCallback g_logCallback = nullptr;
std::vector<TabRef> g_tabOrder;
HFONT g_hTabFont = nullptr;
HFONT g_hTabFontActive = nullptr;
int g_lastTabFontStyle = -1;

std::vector<AppTabInfo> g_appTabs;
int g_activeAppTab = -1;

struct TerminalTabInfo {
    HWND hwnd = nullptr;
    std::wstring label;
    HANDLE hProcess = nullptr;
};
std::vector<TerminalTabInfo> g_terminalTabs;
int g_activeTerminalTab = -1;
volatile LONG g_grepCancelFlag = 0;
bool g_grepSearchActive = false;
void UpdateMenu(HWND) {}
void UpdateTabs(HWND) {}
