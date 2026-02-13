#pragma once

#include <string>
#include <windows.h>

class SettingsManager {
public:
    static SettingsManager& Instance();

    void Load();
    void Save();

    // Window state
    void GetWindowRect(RECT& rc) const { rc = m_windowRect; }
    void SetWindowRect(const RECT& rc) { m_windowRect = rc; }
    bool IsWindowMaximized() const { return m_maximized; }
    void SetWindowMaximized(bool maximized) { m_maximized = maximized; }

    // Editor settings
    std::wstring GetFontFamily() const { return m_fontFamily; }
    void SetFontFamily(const std::wstring& family) { m_fontFamily = family; }
    float GetFontSize() const { return m_fontSize; }
    void SetFontSize(float size) { m_fontSize = size; }
    int GetLanguage() const { return m_language; }
    void SetLanguage(int lang) { m_language = lang; }
    bool IsWordWrap() const { return m_wordWrap; }
    void SetWordWrap(bool wrap) { m_wordWrap = wrap; }

private:
    SettingsManager();
    std::wstring GetSettingsPath() const;

    RECT m_windowRect;
    bool m_maximized;
    std::wstring m_fontFamily;
    float m_fontSize;
    int m_language;
    bool m_wordWrap;
};
