#pragma once

#include <string>
#include <vector>
#include <windows.h>

struct AppEntry {
  std::wstring path;
  std::wstring label;
};

struct CliEntry {
  std::wstring command;
  std::wstring folder;
  int encoding; // 0=UTF8, 1=ShiftJIS
  int shellType; // 0=cmd, 1=powershell, 2=bash
  int iconIndex = -1; // -1 = auto (from exe), 0-11 = colored square
  std::wstring label; // custom tab label (empty = auto from folder)
};

struct DiredPair {
  std::wstring label;
  std::wstring leftDir;
  std::wstring rightDir;
};

struct SessionBufferState {
  std::wstring path;          // file path (empty for scratch/shell)
  size_t caretPos = 0;
  size_t selectionAnchor = 0;
  int scrollLine = 0;
  int scrollX = 0;
  int desiredColumn = 0;
  int encoding = 0;           // 0=UTF8, 1=UTF16LE, 2=UTF16BE, 3=ANSI
  bool isDirty = false;
  bool isScratch = false;
  bool isShell = false;
  std::vector<int> foldedLines;
  std::wstring contentFile;   // filename within session dir for non-file-backed content
};

struct SessionInfo {
  int index = 0;              // N from session_N
  std::wstring name;
  std::wstring time;
  int bufferCount = 0;
};

struct ThemeEntry {
  std::wstring name;
  std::wstring background;  // 8-char hex RRGGBBAA
  std::wstring foreground;
  std::wstring caret;
  std::wstring selection;
  std::wstring lineNumbers;
  std::wstring keyword;
  std::wstring string;
  std::wstring number;
  std::wstring comment;
  std::wstring function;
};

class SettingsManager {
public:
  static SettingsManager &Instance();

  void Load();
  void Save();

  // Window state
  void GetWindowRect(RECT &rc) const { rc = m_windowRect; }
  void SetWindowRect(const RECT &rc) { m_windowRect = rc; }
  bool IsWindowMaximized() const { return m_maximized; }
  void SetWindowMaximized(bool maximized) { m_maximized = maximized; }

  // Editor settings
  std::wstring GetFontFamily() const { return m_fontFamily; }
  void SetFontFamily(const std::wstring &family) { m_fontFamily = family; }
  float GetFontSize() const { return m_fontSize; }
  void SetFontSize(float size) { m_fontSize = size; }
  int GetLanguage() const { return m_language; }
  void SetLanguage(int lang) { m_language = lang; }
  bool IsWordWrap() const { return m_wordWrap; }
  void SetWordWrap(bool wrap) { m_wordWrap = wrap; }
  int GetFontWeight() const { return m_fontWeight; }
  void SetFontWeight(int weight) { m_fontWeight = weight; }
  bool IsEnableLigatures() const { return m_enableLigatures; }
  void SetEnableLigatures(bool enable) { m_enableLigatures = enable; }
  bool IsShowStatusBar() const { return m_showStatusBar; }
  void SetShowStatusBar(bool show) { m_showStatusBar = show; }
  int GetLogLevel() const { return m_logLevel; }
  void SetLogLevel(int level) { m_logLevel = level; }
  bool IsCaretBlinking() const { return m_caretBlinking; }
  void SetCaretBlinking(bool blink) { m_caretBlinking = blink; }
  int GetShellEncoding() const { return m_shellEncoding; }
  void SetShellEncoding(int encoding) { m_shellEncoding = encoding; }
  bool IsShowAI() const { return m_showAI; }
  void SetShowAI(bool show) { m_showAI = show; }
  bool IsVTDebug() const { return m_vtDebug; }
  void SetVTDebug(bool debug) { m_vtDebug = debug; }

  bool IsSessionManagementEnabled() const { return m_enableSessionManagement; }
  void SetSessionManagementEnabled(bool enabled) { m_enableSessionManagement = enabled; }

  int GetTabGridCellW() const { return m_tabGridCellW; }
  void SetTabGridCellW(int w) { m_tabGridCellW = w; }
  int GetTabGridCellH() const { return m_tabGridCellH; }
  void SetTabGridCellH(int h) { m_tabGridCellH = h; }
  bool IsTabGridRefreshEnabled() const { return m_tabGridRefreshEnabled; }
  void SetTabGridRefreshEnabled(bool enabled) { m_tabGridRefreshEnabled = enabled; }
  int GetTabGridRefreshIntervalMs() const { return m_tabGridRefreshIntervalMs; }
  void SetTabGridRefreshIntervalMs(int ms) { m_tabGridRefreshIntervalMs = ms; }

  // AI Settings
  std::wstring GetAIVendor() const { return m_aiVendor; }
  void SetAIVendor(const std::wstring &vendor) { m_aiVendor = vendor; }
  std::wstring GetAIModel() const { return m_aiModel; }
  void SetAIModel(const std::wstring &model) { m_aiModel = model; }
  std::wstring GetAIApiKey(const std::wstring &vendor) const;
  void SetAIApiKey(const std::wstring &vendor, const std::wstring &apiKey);

  const std::vector<std::wstring> &GetRecentFiles() const {
    return m_recentFiles;
  }
  void AddRecentFile(const std::wstring &path);

  std::wstring GetProjectDirectory() const { return m_projectDirectory; }
  void SetProjectDirectory(const std::wstring &dir) { m_projectDirectory = dir; }

  std::wstring GetFindStartDirectory() const { return m_findStartDir; }
  void SetFindStartDirectory(const std::wstring &dir) { m_findStartDir = dir; }

  int GetCaretStyle() const { return m_caretStyle; }
  void SetCaretStyle(int style) { m_caretStyle = style; }

  std::wstring GetBashPath() const { return m_bashPath; }
  void SetBashPath(const std::wstring &path) { m_bashPath = path; }

  std::wstring GetDefaultExtension() const { return m_defaultExtension; }
  void SetDefaultExtension(const std::wstring &ext) { m_defaultExtension = ext; }

  std::wstring GetPluginsDirectory() const { return m_pluginsDirectory; }
  void SetPluginsDirectory(const std::wstring &dir) { m_pluginsDirectory = dir; }

  // Session management
  std::wstring GetSessionsDirectory() const;
  std::wstring GetSessionIndexPath() const;
  std::wstring GetSessionDir(int index) const;
  int SaveSession(const std::wstring &name);
  bool LoadSession(int index);
  bool DeleteSession(int index);
  std::vector<SessionInfo> GetSessionList() const;
  int GetAutoSaveSessionIndex() const;
  void SetAutoSaveSessionIndex(int index);
  void ClearAutoSaveSession();

  bool IsNoTitleBar() const { return m_noTitleBar; }
  void SetNoTitleBar(bool v) { m_noTitleBar = v; }

  // Returns bash exe path (--cd stripped), sets workingDir to the directory from --cd if present
  std::wstring GetBashCommand(std::wstring *workingDir = nullptr) const;

  std::wstring GetAppDataPath() const;

  static std::wstring DetectBashPath();

  // CLI entries
  const std::vector<CliEntry> &GetCliEntries() const { return m_cliEntries; }
  void SetCliEntries(const std::vector<CliEntry> &entries) { m_cliEntries = entries; }
  void AddCliEntry(const std::wstring &cmd, const std::wstring &folder, int encoding = 0, int shellType = 2, int iconIndex = -1, const std::wstring &label = L"");
  void RemoveCliEntry(size_t index);
  void SaveCliEntries();

  // App entries
  const std::vector<AppEntry> &GetAppEntries() const { return m_appEntries; }
  void AddAppEntry(const std::wstring &path, const std::wstring &label);
  void RemoveAppEntry(size_t index);
  void SaveAppEntries();

  // Dired pairs
  const std::vector<DiredPair> &GetDiredPairs() const { return m_diredPairs; }
  void AddDiredPair(const std::wstring &label, const std::wstring &leftDir, const std::wstring &rightDir);
  void RemoveDiredPair(size_t index);
  void SaveDiredPairs();

  // Hidden plugins
  const std::vector<std::wstring> &GetHiddenPlugins() const { return m_hiddenPlugins; }
  void SetHiddenPlugins(const std::vector<std::wstring> &list) { m_hiddenPlugins = list; }
  bool IsPluginHidden(const std::wstring &name) const;
  void ToggleHiddenPlugin(const std::wstring &name);
  void SaveHiddenPlugins();

  // Theme management
  const std::vector<ThemeEntry> &GetThemes() const { return m_themes; }
  std::wstring GetActiveThemeName() const { return m_activeTheme; }
  void SetActiveThemeName(const std::wstring &name) { m_activeTheme = name; }
  void AddTheme(const ThemeEntry &theme);
  void RemoveTheme(size_t index);
  void SaveThemes();
  const ThemeEntry *FindTheme(const std::wstring &name) const;

private:
  SettingsManager();
  std::wstring GetSettingsPath() const;

  RECT m_windowRect;
  bool m_maximized;
  bool m_noTitleBar = false;
  std::wstring m_fontFamily;
  float m_fontSize;
  int m_language;
  bool m_wordWrap;
  int m_fontWeight;
  bool m_enableLigatures;
  bool m_showStatusBar;
  int m_logLevel;
  std::vector<std::wstring> m_recentFiles;
  bool m_caretBlinking;
  int m_caretStyle = 0;
  int m_shellEncoding; // 0=UTF8, 1=ShiftJIS
  std::wstring m_projectDirectory;
  std::wstring m_findStartDir;
  std::wstring m_bashPath;
  std::wstring m_defaultExtension;
  std::wstring m_pluginsDirectory;
  int m_tabGridCellW = 240;
  int m_tabGridCellH = 170;
  bool m_tabGridRefreshEnabled = false;
  int m_tabGridRefreshIntervalMs = 1000;
  bool m_showAI = false;
  bool m_vtDebug = false;
  bool m_enableSessionManagement = false;
  std::vector<CliEntry> m_cliEntries;
  std::vector<AppEntry> m_appEntries;
  std::vector<DiredPair> m_diredPairs;

  std::wstring m_aiVendor;
  std::wstring m_aiModel;
  std::vector<std::pair<std::wstring, std::wstring>> m_aiApiKeys; // vendor -> key

  std::vector<ThemeEntry> m_themes;
  std::wstring m_activeTheme;
  std::vector<std::wstring> m_hiddenPlugins;
};
