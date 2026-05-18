#include "../include/SettingsManager.h"
#include <shlobj.h>
#include "../include/StringHelpers.h"

SettingsManager &SettingsManager::Instance() {
  static SettingsManager instance;
  return instance;
}

SettingsManager::SettingsManager()
    : m_maximized(false), m_fontFamily(L"Consolas"), m_fontSize(12.0f),
      m_language(0), m_wordWrap(false), m_fontWeight(400),
      m_enableLigatures(true), m_showStatusBar(true), m_logLevel(1),
      m_caretBlinking(true), m_shellEncoding(0) {
  m_windowRect = {100, 100, 900, 700};
}

std::wstring SettingsManager::GetAppDataPath() const {
  wchar_t path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
    std::wstring settingsDir = std::wstring(path) + L"\\Ecode";
    CreateDirectoryW(settingsDir.c_str(), NULL);
    return settingsDir;
  }
  return L".";
}

std::wstring SettingsManager::GetSettingsPath() const {
  return GetAppDataPath() + L"\\settings.ini";
}

void SettingsManager::Load() {
  std::wstring path = GetSettingsPath();

  m_windowRect.left =
      GetPrivateProfileIntW(L"Window", L"Left", 100, path.c_str());
  m_windowRect.top =
      GetPrivateProfileIntW(L"Window", L"Top", 100, path.c_str());
  m_windowRect.right =
      GetPrivateProfileIntW(L"Window", L"Right", 900, path.c_str());
  m_windowRect.bottom =
      GetPrivateProfileIntW(L"Window", L"Bottom", 700, path.c_str());
  m_maximized =
      GetPrivateProfileIntW(L"Window", L"Maximized", 0, path.c_str()) != 0;

  wchar_t fontBuf[256];
  GetPrivateProfileStringW(L"Editor", L"FontFamily", L"Consolas", fontBuf, 256,
                           path.c_str());
  m_fontFamily = fontBuf;

  wchar_t sizeBuf[32];
  GetPrivateProfileStringW(L"Editor", L"FontSize", L"12.0", sizeBuf, 32,
                           path.c_str());
  m_fontSize = std::wcstof(sizeBuf, nullptr);

  m_language = GetPrivateProfileIntW(L"Editor", L"Language", 0, path.c_str());
  m_wordWrap =
      GetPrivateProfileIntW(L"Editor", L"WordWrap", 0, path.c_str()) != 0;
  m_fontWeight =
      GetPrivateProfileIntW(L"Editor", L"FontWeight", 400, path.c_str());
  m_enableLigatures = GetPrivateProfileIntW(L"Editor", L"EnableLigatures", 1,
                                            path.c_str()) != 0;
  m_showStatusBar =
      GetPrivateProfileIntW(L"Editor", L"ShowStatusBar", 1, path.c_str()) != 0;
  m_showStatusBar =
      GetPrivateProfileIntW(L"Editor", L"ShowStatusBar", 1, path.c_str()) != 0;
  m_logLevel = GetPrivateProfileIntW(L"Editor", L"LogLevel", 1, path.c_str());
  m_caretBlinking =
      GetPrivateProfileIntW(L"Editor", L"CaretBlinking", 1, path.c_str()) != 0;
  m_caretStyle = 
      GetPrivateProfileIntW(L"Editor", L"CaretStyle", 0, path.c_str());
  m_shellEncoding =
      GetPrivateProfileIntW(L"Editor", L"ShellEncoding", 0, path.c_str());

  wchar_t projDirBuf[MAX_PATH];
  if (GetPrivateProfileStringW(L"Editor", L"ProjectDirectory", L"", projDirBuf,
                               MAX_PATH, path.c_str()) > 0) {
    m_projectDirectory = projDirBuf;
  }

  wchar_t findDirBuf[MAX_PATH];
  if (GetPrivateProfileStringW(L"Editor", L"FindStartDirectory", L"", findDirBuf,
                               MAX_PATH, path.c_str()) > 0) {
    m_findStartDir = findDirBuf;
  }

  m_recentFiles.clear();
  for (int i = 1; i <= 10; ++i) {
    std::wstring key = L"Recent" + std::to_wstring(i);
    wchar_t recentBuf[MAX_PATH];
    if (GetPrivateProfileStringW(L"Recent", key.c_str(), L"", recentBuf,
                                 MAX_PATH, path.c_str()) > 0) {
      m_recentFiles.push_back(recentBuf);
    }
  }

  wchar_t vendorBuf[256];
  GetPrivateProfileStringW(L"AI", L"Vendor", L"Gemini", vendorBuf, 256, path.c_str());
  m_aiVendor = vendorBuf;

  wchar_t modelBuf[256];
  GetPrivateProfileStringW(L"AI", L"Model", L"gemini-1.5-pro", modelBuf, 256, path.c_str());
  m_aiModel = modelBuf;

  m_aiApiKeys.clear();
  wchar_t keysBuf[4096];
  if (GetPrivateProfileSectionW(L"AI_API_KEYS", keysBuf, 4096, path.c_str()) > 0) {
    wchar_t* p = keysBuf;
    while (*p) {
      std::wstring line(p);
      size_t pos = line.find(L'=');
      if (pos != std::wstring::npos) {
        m_aiApiKeys.push_back({line.substr(0, pos), line.substr(pos + 1)});
      }
      p += line.length() + 1;
    }
  }
}

std::wstring SettingsManager::GetAIApiKey(const std::wstring &vendor) const {
  for (const auto& pair : m_aiApiKeys) {
    if (pair.first == vendor) return pair.second;
  }
  return L"";
}

void SettingsManager::SetAIApiKey(const std::wstring &vendor, const std::wstring &apiKey) {
  for (auto& pair : m_aiApiKeys) {
    if (pair.first == vendor) {
      pair.second = apiKey;
      return;
    }
  }
  m_aiApiKeys.push_back({vendor, apiKey});
}

void SettingsManager::Save() {
  std::wstring path = GetSettingsPath();

  auto WriteInt = [&](const wchar_t *sect, const wchar_t *key, int val) {
    WritePrivateProfileStringW(sect, key, std::to_wstring(val).c_str(),
                               path.c_str());
  };

  WriteInt(L"Window", L"Left", m_windowRect.left);
  WriteInt(L"Window", L"Top", m_windowRect.top);
  WriteInt(L"Window", L"Right", m_windowRect.right);
  WriteInt(L"Window", L"Bottom", m_windowRect.bottom);
  WriteInt(L"Window", L"Maximized", m_maximized ? 1 : 0);

  WritePrivateProfileStringW(L"Editor", L"FontFamily", m_fontFamily.c_str(),
                             path.c_str());
  WritePrivateProfileStringW(L"Editor", L"FontSize",
                             std::to_wstring(m_fontSize).c_str(), path.c_str());
  WriteInt(L"Editor", L"Language", m_language);
  WriteInt(L"Editor", L"WordWrap", m_wordWrap ? 1 : 0);
  WriteInt(L"Editor", L"FontWeight", m_fontWeight);
  WriteInt(L"Editor", L"EnableLigatures", m_enableLigatures ? 1 : 0);
  WriteInt(L"Editor", L"ShowStatusBar", m_showStatusBar ? 1 : 0);
  WriteInt(L"Editor", L"LogLevel", m_logLevel);
  WriteInt(L"Editor", L"CaretBlinking", m_caretBlinking ? 1 : 0);
  WriteInt(L"Editor", L"CaretStyle", m_caretStyle);
  WriteInt(L"Editor", L"ShellEncoding", m_shellEncoding);
  if (!m_projectDirectory.empty()) {
    WritePrivateProfileStringW(L"Editor", L"ProjectDirectory",
                               m_projectDirectory.c_str(), path.c_str());
  }
  if (!m_findStartDir.empty()) {
    WritePrivateProfileStringW(L"Editor", L"FindStartDirectory",
                               m_findStartDir.c_str(), path.c_str());
  }

  for (size_t i = 0; i < m_recentFiles.size(); ++i) {
    std::wstring key = L"Recent" + std::to_wstring(i + 1);
    WritePrivateProfileStringW(L"Recent", key.c_str(), m_recentFiles[i].c_str(),
                               path.c_str());
  }

  WritePrivateProfileStringW(L"AI", L"Vendor", m_aiVendor.c_str(), path.c_str());
  WritePrivateProfileStringW(L"AI", L"Model", m_aiModel.c_str(), path.c_str());

  WritePrivateProfileStringW(L"AI_API_KEYS", NULL, NULL, path.c_str());
  for (const auto& pair : m_aiApiKeys) {
    WritePrivateProfileStringW(L"AI_API_KEYS", pair.first.c_str(), pair.second.c_str(), path.c_str());
  }
}

void SettingsManager::AddRecentFile(const std::wstring &path) {
  auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), path);
  if (it != m_recentFiles.end()) {
    m_recentFiles.erase(it);
  }
  m_recentFiles.insert(m_recentFiles.begin(), path);
  if (m_recentFiles.size() > 10) {
    m_recentFiles.pop_back();
  }
  Save();
}
