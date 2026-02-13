#include "../include/SettingsManager.h"
#include <shlobj.h>
#include <iostream>

SettingsManager& SettingsManager::Instance() {
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager() 
    : m_maximized(false), m_fontFamily(L"Consolas"), m_fontSize(12.0f), m_language(0), m_wordWrap(false) {
    m_windowRect = { 100, 100, 900, 700 };
}

std::wstring SettingsManager::GetSettingsPath() const {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::wstring settingsDir = std::wstring(path) + L"\\Ecode";
        CreateDirectoryW(settingsDir.c_str(), NULL);
        return settingsDir + L"\\settings.ini";
    }
    return L"settings.ini";
}

void SettingsManager::Load() {
    std::wstring path = GetSettingsPath();
    
    m_windowRect.left = GetPrivateProfileIntW(L"Window", L"Left", 100, path.c_str());
    m_windowRect.top = GetPrivateProfileIntW(L"Window", L"Top", 100, path.c_str());
    m_windowRect.right = GetPrivateProfileIntW(L"Window", L"Right", 900, path.c_str());
    m_windowRect.bottom = GetPrivateProfileIntW(L"Window", L"Bottom", 700, path.c_str());
    m_maximized = GetPrivateProfileIntW(L"Window", L"Maximized", 0, path.c_str()) != 0;

    wchar_t fontBuf[256];
    GetPrivateProfileStringW(L"Editor", L"FontFamily", L"Consolas", fontBuf, 256, path.c_str());
    m_fontFamily = fontBuf;
    
    wchar_t sizeBuf[32];
    GetPrivateProfileStringW(L"Editor", L"FontSize", L"12.0", sizeBuf, 32, path.c_str());
    m_fontSize = std::wcstof(sizeBuf, nullptr);

    m_language = GetPrivateProfileIntW(L"Editor", L"Language", 0, path.c_str());
    m_wordWrap = GetPrivateProfileIntW(L"Editor", L"WordWrap", 0, path.c_str()) != 0;
}

void SettingsManager::Save() {
    std::wstring path = GetSettingsPath();

    auto WriteInt = [&](const wchar_t* sect, const wchar_t* key, int val) {
        WritePrivateProfileStringW(sect, key, std::to_wstring(val).c_str(), path.c_str());
    };

    WriteInt(L"Window", L"Left", m_windowRect.left);
    WriteInt(L"Window", L"Top", m_windowRect.top);
    WriteInt(L"Window", L"Right", m_windowRect.right);
    WriteInt(L"Window", L"Bottom", m_windowRect.bottom);
    WriteInt(L"Window", L"Maximized", m_maximized ? 1 : 0);

    WritePrivateProfileStringW(L"Editor", L"FontFamily", m_fontFamily.c_str(), path.c_str());
    WritePrivateProfileStringW(L"Editor", L"FontSize", std::to_wstring(m_fontSize).c_str(), path.c_str());
    WriteInt(L"Editor", L"Language", m_language);
    WriteInt(L"Editor", L"WordWrap", m_wordWrap ? 1 : 0);
}
