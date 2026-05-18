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
  m_bashPath = DetectBashPath();
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
  m_showAI =
      GetPrivateProfileIntW(L"Editor", L"ShowAI", 0, path.c_str()) != 0;

  wchar_t bashBuf[MAX_PATH];
  if (GetPrivateProfileStringW(L"Editor", L"BashPath", L"", bashBuf,
                               MAX_PATH, path.c_str()) > 0) {
    m_bashPath = bashBuf;
  }
  if (m_bashPath.empty()) {
    m_bashPath = DetectBashPath();
  }

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

  // Load CLI entries
  m_cliEntries.clear();
  for (int i = 0; i < 50; ++i) {
    std::wstring keyCmd = L"CLI_Cmd_" + std::to_wstring(i);
    std::wstring keyDir = L"CLI_Dir_" + std::to_wstring(i);
    wchar_t cmdBuf[1024], dirBuf[MAX_PATH];
    if (GetPrivateProfileStringW(L"CLI", keyCmd.c_str(), L"", cmdBuf, 1024, path.c_str()) > 0) {
      GetPrivateProfileStringW(L"CLI", keyDir.c_str(), L"", dirBuf, MAX_PATH, path.c_str());
      m_cliEntries.push_back({cmdBuf, dirBuf});
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
  WriteInt(L"Editor", L"ShowAI", m_showAI ? 1 : 0);
  if (!m_bashPath.empty()) {
    WritePrivateProfileStringW(L"Editor", L"BashPath", m_bashPath.c_str(),
                               path.c_str());
  }
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

  SaveCliEntries();
}

std::wstring SettingsManager::DetectBashPath() {
  HKEY hKey;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\GitForWindows", 0,
                    KEY_READ, &hKey) == ERROR_SUCCESS) {
    wchar_t installPath[MAX_PATH];
    DWORD bufSize = sizeof(installPath);
    if (RegQueryValueExW(hKey, L"InstallPath", nullptr, nullptr,
                         (LPBYTE)installPath, &bufSize) == ERROR_SUCCESS) {
      RegCloseKey(hKey);
      std::wstring bashPath = std::wstring(installPath) + L"\\usr\\bin\\bash.exe";
      if (GetFileAttributesW(bashPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        return bashPath;
    } else {
      RegCloseKey(hKey);
    }
  }
  return L"";
}

std::wstring SettingsManager::GetBashCommand(std::wstring *workingDir) const {
  HKEY hKey;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Classes\\Directory\\Background\\shell\\git_shell\\command",
                    0, KEY_READ, &hKey) != ERROR_SUCCESS) {
    return L"";
  }

  wchar_t cmdBuf[1024];
  DWORD bufSize = sizeof(cmdBuf);
  if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr,
                       (LPBYTE)cmdBuf, &bufSize) != ERROR_SUCCESS) {
    RegCloseKey(hKey);
    return L"";
  }
  RegCloseKey(hKey);

  std::wstring rawCmd(cmdBuf);

  // Expand %v with current working directory + path separator
  wchar_t curDir[MAX_PATH];
  GetCurrentDirectoryW(MAX_PATH, curDir);
  std::wstring dirStr = std::wstring(curDir) + L"\\";
  size_t pctV = rawCmd.find(L"%v");
  while (pctV != std::wstring::npos) {
    rawCmd.replace(pctV, 2, dirStr);
    pctV = rawCmd.find(L"%v", pctV + dirStr.size());
  }

  // Parse with CommandLineToArgvW
  int argc;
  LPWSTR *argv = CommandLineToArgvW(rawCmd.c_str(), &argc);
  if (!argv || argc < 1) {
    if (argv) LocalFree(argv);
    return L"";
  }

  // Replace git-bash.exe with usr\bin\bash.exe for ConPTY embedding
  std::wstring exe = argv[0];
  size_t gpos = exe.find(L"git-bash.exe");
  if (gpos != std::wstring::npos) {
    exe = exe.substr(0, gpos) + L"usr\\bin\\bash.exe";
  }

  // Extract --cd value and strip --cd arguments; set working directory
  std::vector<std::wstring> filteredArgs;
  for (int i = 1; i < argc; ++i) {
    std::wstring arg = argv[i];
    if (arg.find(L"--cd=") == 0) {
      std::wstring cdVal = arg.substr(5);
      // Remove surrounding quotes if present
      if (cdVal.size() >= 2 && cdVal.front() == L'"' && cdVal.back() == L'"')
        cdVal = cdVal.substr(1, cdVal.size() - 2);
      // Remove trailing .\ or . if present
      while (!cdVal.empty() && (cdVal.back() == L'.' || cdVal.back() == L'\\'))
        cdVal.pop_back();
      if (workingDir) *workingDir = cdVal;
    } else {
      filteredArgs.push_back(arg);
    }
  }

  // Rebuild command line (exe only, without --cd)
  std::wstring result;
  result = (exe.find(L' ') != std::wstring::npos) ? (L"\"" + exe + L"\"") : exe;
  for (auto &arg : filteredArgs) {
    result += L' ';
    result += (arg.find(L' ') != std::wstring::npos) ? (L"\"" + arg + L"\"") : arg;
  }

  result += L" --login -i";

  LocalFree(argv);
  return result;
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

void SettingsManager::AddCliEntry(const std::wstring &cmd, const std::wstring &folder) {
  m_cliEntries.push_back({cmd, folder});
}

void SettingsManager::RemoveCliEntry(size_t index) {
  if (index < m_cliEntries.size()) {
    m_cliEntries.erase(m_cliEntries.begin() + index);
  }
}

void SettingsManager::SaveCliEntries() {
  std::wstring path = GetSettingsPath();
  // Clear existing CLI entries
  WritePrivateProfileStringW(L"CLI", NULL, NULL, path.c_str());
  for (size_t i = 0; i < m_cliEntries.size(); ++i) {
    std::wstring keyCmd = L"CLI_Cmd_" + std::to_wstring(i);
    std::wstring keyDir = L"CLI_Dir_" + std::to_wstring(i);
    WritePrivateProfileStringW(L"CLI", keyCmd.c_str(), m_cliEntries[i].command.c_str(), path.c_str());
    WritePrivateProfileStringW(L"CLI", keyDir.c_str(), m_cliEntries[i].folder.c_str(), path.c_str());
  }
}
