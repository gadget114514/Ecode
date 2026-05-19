#pragma once

#include <string>
#include <vector>
#include <windows.h>

struct CliEntry {
  std::wstring command;
  std::wstring folder;
};

struct DiredPair {
  std::wstring label;
  std::wstring leftDir;
  std::wstring rightDir;
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

  // Returns bash exe path (--cd stripped), sets workingDir to the directory from --cd if present
  std::wstring GetBashCommand(std::wstring *workingDir = nullptr) const;

  std::wstring GetAppDataPath() const;

  static std::wstring DetectBashPath();

  // CLI entries
  const std::vector<CliEntry> &GetCliEntries() const { return m_cliEntries; }
  void SetCliEntries(const std::vector<CliEntry> &entries) { m_cliEntries = entries; }
  void AddCliEntry(const std::wstring &cmd, const std::wstring &folder);
  void RemoveCliEntry(size_t index);
  void SaveCliEntries();

  // Dired pairs
  const std::vector<DiredPair> &GetDiredPairs() const { return m_diredPairs; }
  void AddDiredPair(const std::wstring &label, const std::wstring &leftDir, const std::wstring &rightDir);
  void RemoveDiredPair(size_t index);
  void SaveDiredPairs();

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
  bool m_showAI = false;
  std::vector<CliEntry> m_cliEntries;
  std::vector<DiredPair> m_diredPairs;

  std::wstring m_aiVendor;
  std::wstring m_aiModel;
  std::vector<std::pair<std::wstring, std::wstring>> m_aiApiKeys; // vendor -> key

  std::vector<ThemeEntry> m_themes;
  std::wstring m_activeTheme;
};
