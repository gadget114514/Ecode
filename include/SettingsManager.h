#pragma once

#include <string>
#include <vector>
#include <windows.h>

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

  const std::vector<std::wstring> &GetRecentFiles() const {
    return m_recentFiles;
  }
  void AddRecentFile(const std::wstring &path);

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
  int m_shellEncoding; // 0=UTF8, 1=ShiftJIS
};
