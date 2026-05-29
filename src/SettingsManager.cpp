#include "../include/SettingsManager.h"
#include "../include/Buffer.h"
#include "../include/Editor.h"
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
  m_noTitleBar = false;
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

static std::wstring GetRecentFilePath() {
  wchar_t path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
    std::wstring dir = std::wstring(path) + L"\\Ecode";
    CreateDirectoryW(dir.c_str(), NULL);
    return dir + L"\\recent.ini";
  }
  return L".\\recent.ini";
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
  m_noTitleBar =
      GetPrivateProfileIntW(L"Window", L"NoTitleBar", 0, path.c_str()) != 0;

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
  m_vtDebug =
      GetPrivateProfileIntW(L"Editor", L"VTDebug", 0, path.c_str()) != 0;
  m_reverseScrollDirection =
      GetPrivateProfileIntW(L"Editor", L"ReverseScrollDirection", 0, path.c_str()) != 0;
  m_hideMessagesBuffer =
      GetPrivateProfileIntW(L"Editor", L"HideMessagesBuffer", 1, path.c_str()) != 0;
  m_tabGridCellW =
      GetPrivateProfileIntW(L"TabGrid", L"CellWidth",  240, path.c_str());
  m_tabGridCellH =
      GetPrivateProfileIntW(L"TabGrid", L"CellHeight", 170, path.c_str());
  if (m_tabGridCellW < 80)  m_tabGridCellW = 80;
  if (m_tabGridCellH < 60)  m_tabGridCellH = 60;
  m_tabGridRefreshEnabled =
      GetPrivateProfileIntW(L"TabGrid", L"RefreshEnabled", 0, path.c_str()) != 0;
  m_tabGridRefreshIntervalMs =
      GetPrivateProfileIntW(L"TabGrid", L"RefreshIntervalMs", 1000, path.c_str());
  if (m_tabGridRefreshIntervalMs < 200) m_tabGridRefreshIntervalMs = 200;

  wchar_t bashBuf[MAX_PATH];
  if (GetPrivateProfileStringW(L"Editor", L"BashPath", L"", bashBuf,
                               MAX_PATH, path.c_str()) > 0) {
    m_bashPath = bashBuf;
  }
  if (m_bashPath.empty()) {
    m_bashPath = DetectBashPath();
  }

  wchar_t defExtBuf[64];
  if (GetPrivateProfileStringW(L"Editor", L"DefaultExtension", L"", defExtBuf,
                                64, path.c_str()) > 0 && wcslen(defExtBuf) > 0) {
    m_defaultExtension = defExtBuf;
  } else {
    m_defaultExtension = L"*.txt;*.md;*.json";
  }

  wchar_t pluginDirBuf[MAX_PATH];
  if (GetPrivateProfileStringW(L"Editor", L"PluginsDirectory", L"", pluginDirBuf,
                               MAX_PATH, path.c_str()) > 0) {
    m_pluginsDirectory = pluginDirBuf;
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

  wchar_t findPatBuf[256];
  if (GetPrivateProfileStringW(L"Editor", L"FindPattern", L"", findPatBuf,
                               256, path.c_str()) > 0) {
    m_findPattern = findPatBuf;
  }

  wchar_t findExtBuf[256];
  if (GetPrivateProfileStringW(L"Editor", L"FindExtFilter", L"", findExtBuf,
                               256, path.c_str()) > 0) {
    m_findExtFilter = findExtBuf;
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

  // Load app entries
  m_appEntries.clear();
  for (int i = 0; i < 50; ++i) {
    std::wstring keyPath = L"App_Path_" + std::to_wstring(i);
    std::wstring keyLbl = L"App_Lbl_" + std::to_wstring(i);
    wchar_t pathBuf[MAX_PATH], lblBuf[256];
    if (GetPrivateProfileStringW(L"AppEntries", keyPath.c_str(), L"", pathBuf, MAX_PATH, path.c_str()) > 0) {
      GetPrivateProfileStringW(L"AppEntries", keyLbl.c_str(), L"", lblBuf, 256, path.c_str());
      m_appEntries.push_back({pathBuf, lblBuf});
    }
  }
  // Add default FastFileSearch if no entries exist
  if (m_appEntries.empty()) {
    wchar_t modPath[MAX_PATH];
    GetModuleFileNameW(nullptr, modPath, MAX_PATH);
    std::wstring exeDir = modPath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\/"));
    std::wstring ffsPath = exeDir + L"\\apps\\FastFileSearch.exe";
    if (GetFileAttributesW(ffsPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
      m_appEntries.push_back({ffsPath, L"FastFileSearch"});
    }
  }

  // Load folder entries
  m_folderEntries.clear();
  for (int i = 0; i < 50; ++i) {
    std::wstring keyPath = L"Folder_Path_" + std::to_wstring(i);
    std::wstring keyLbl = L"Folder_Lbl_" + std::to_wstring(i);
    wchar_t pathBuf[MAX_PATH], lblBuf[256];
    if (GetPrivateProfileStringW(L"FolderEntries", keyPath.c_str(), L"", pathBuf, MAX_PATH, path.c_str()) > 0) {
      GetPrivateProfileStringW(L"FolderEntries", keyLbl.c_str(), L"", lblBuf, 256, path.c_str());
      m_folderEntries.push_back({pathBuf, lblBuf});
    }
  }

  // Load CLI entries
  m_cliEntries.clear();
  for (int i = 0; i < 50; ++i) {
    std::wstring keyCmd = L"CLI_Cmd_" + std::to_wstring(i);
    std::wstring keyDir = L"CLI_Dir_" + std::to_wstring(i);
    std::wstring keyEnc = L"CLI_Enc_" + std::to_wstring(i);
    std::wstring keyShl = L"CLI_Shell_" + std::to_wstring(i);
    std::wstring keyIcn = L"CLI_Icon_" + std::to_wstring(i);
    std::wstring keyLbl = L"CLI_Lbl_" + std::to_wstring(i);
    wchar_t cmdBuf[1024], dirBuf[MAX_PATH], lblBuf[256];
    if (GetPrivateProfileStringW(L"CLI", keyCmd.c_str(), L"", cmdBuf, 1024, path.c_str()) > 0) {
      GetPrivateProfileStringW(L"CLI", keyDir.c_str(), L"", dirBuf, MAX_PATH, path.c_str());
      int enc = GetPrivateProfileIntW(L"CLI", keyEnc.c_str(), 0, path.c_str());
      int st = GetPrivateProfileIntW(L"CLI", keyShl.c_str(), 2, path.c_str());
      int icn = GetPrivateProfileIntW(L"CLI", keyIcn.c_str(), -1, path.c_str());
      GetPrivateProfileStringW(L"CLI", keyLbl.c_str(), L"", lblBuf, 256, path.c_str());
      m_cliEntries.push_back({cmdBuf, dirBuf, enc, st, icn, lblBuf});
    }
  }

  // Load themes
  m_themes.clear();
  wchar_t themeNamesBuf[4096];
  if (GetPrivateProfileSectionW(L"Themes", themeNamesBuf, 4096, path.c_str()) > 0) {
    wchar_t *p = themeNamesBuf;
    while (*p) {
      std::wstring line(p);
      size_t pos = line.find(L'=');
      if (pos != std::wstring::npos) {
        std::wstring key = line.substr(0, pos);
        if (key == L"List") {
          std::wstring list = line.substr(pos + 1);
          size_t start = 0, end;
          while ((end = list.find(L',', start)) != std::wstring::npos) {
            std::wstring tn = list.substr(start, end - start);
            if (!tn.empty()) {
              ThemeEntry te;
              te.name = tn;
              wchar_t val[256];
              auto ReadHex = [&](const wchar_t *sect, const wchar_t *k, const wchar_t *def) -> std::wstring {
                if (GetPrivateProfileStringW(sect, k, def, val, 256, path.c_str()) > 0)
                  return val;
                return def;
              };
              std::wstring sect = L"Theme_" + tn;
              te.background = ReadHex(sect.c_str(), L"Background", L"ffffffff");
              te.foreground = ReadHex(sect.c_str(), L"Foreground", L"000000ff");
              te.caret = ReadHex(sect.c_str(), L"Caret", L"000000ff");
              te.selection = ReadHex(sect.c_str(), L"Selection", L"add8e680");
              te.lineNumbers = ReadHex(sect.c_str(), L"LineNumbers", L"808080ff");
              te.keyword = ReadHex(sect.c_str(), L"Keyword", L"0000ffff");
              te.string = ReadHex(sect.c_str(), L"String", L"a32929ff");
              te.number = ReadHex(sect.c_str(), L"Number", L"008000ff");
              te.comment = ReadHex(sect.c_str(), L"Comment", L"006300ff");
              te.function = ReadHex(sect.c_str(), L"Function", L"800080ff");
              m_themes.push_back(te);
            }
            start = end + 1;
          }
          if (start < list.size()) {
            std::wstring tn = list.substr(start);
            if (!tn.empty()) {
              ThemeEntry te;
              te.name = tn;
              wchar_t val[256];
              auto ReadHex = [&](const wchar_t *sect, const wchar_t *k, const wchar_t *def) -> std::wstring {
                if (GetPrivateProfileStringW(sect, k, def, val, 256, path.c_str()) > 0)
                  return val;
                return def;
              };
              std::wstring sect = L"Theme_" + tn;
              te.background = ReadHex(sect.c_str(), L"Background", L"ffffffff");
              te.foreground = ReadHex(sect.c_str(), L"Foreground", L"000000ff");
              te.caret = ReadHex(sect.c_str(), L"Caret", L"000000ff");
              te.selection = ReadHex(sect.c_str(), L"Selection", L"add8e680");
              te.lineNumbers = ReadHex(sect.c_str(), L"LineNumbers", L"808080ff");
              te.keyword = ReadHex(sect.c_str(), L"Keyword", L"0000ffff");
              te.string = ReadHex(sect.c_str(), L"String", L"a32929ff");
              te.number = ReadHex(sect.c_str(), L"Number", L"008000ff");
              te.comment = ReadHex(sect.c_str(), L"Comment", L"006300ff");
              te.function = ReadHex(sect.c_str(), L"Function", L"800080ff");
              m_themes.push_back(te);
            }
          }
        }
      }
      p += line.length() + 1;
    }
  }

  if (m_themes.empty()) {
    // Default themes
    ThemeEntry light;
    light.name = L"Light";
    light.background = L"ffffffff"; light.foreground = L"000000ff"; light.caret = L"000000ff";
    light.selection = L"add8e680"; light.lineNumbers = L"808080ff";
    light.keyword = L"0000ffff"; light.string = L"a32929ff"; light.number = L"008000ff";
    light.comment = L"006300ff"; light.function = L"800080ff";
    m_themes.push_back(light);

    ThemeEntry dark;
    dark.name = L"Dark";
    dark.background = L"1e1e1eff"; dark.foreground = L"d4d4d4ff"; dark.caret = L"d4d4d4ff";
    dark.selection = L"264f7880"; dark.lineNumbers = L"858585ff";
    dark.keyword = L"569cd6ff"; dark.string = L"ce9178ff"; dark.number = L"b5cea8ff";
    dark.comment = L"6a9955ff"; dark.function = L"dcdcaaff";
    m_themes.push_back(dark);
  }

  wchar_t activeBuf[256];
  if (GetPrivateProfileStringW(L"Theme", L"Active", m_themes[0].name.c_str(), activeBuf, 256, path.c_str()) > 0) {
    m_activeTheme = activeBuf;
  } else {
    m_activeTheme = m_themes[0].name;
  }

  // Load hidden plugins
  m_hiddenPlugins.clear();
  wchar_t hiddenBuf[4096];
  if (GetPrivateProfileStringW(L"HiddenPlugins", L"List", L"", hiddenBuf, 4096, path.c_str()) > 0 && wcslen(hiddenBuf) > 0) {
    std::wstring list = hiddenBuf;
    size_t start = 0, end;
    while ((end = list.find(L',', start)) != std::wstring::npos) {
      std::wstring pn = list.substr(start, end - start);
      if (!pn.empty()) m_hiddenPlugins.push_back(pn);
      start = end + 1;
    }
    if (start < list.size()) {
      std::wstring pn = list.substr(start);
      if (!pn.empty()) m_hiddenPlugins.push_back(pn);
    }
  }

  m_enableSessionManagement =
      GetPrivateProfileIntW(L"Session", L"EnableSessionManagement", 0, path.c_str()) != 0;

  // Load Dired pairs
  m_diredPairs.clear();
  for (int i = 0; i < 50; ++i) {
    std::wstring keyLabel = L"DP_Label_" + std::to_wstring(i);
    std::wstring keyLeft  = L"DP_Left_" + std::to_wstring(i);
    std::wstring keyRight = L"DP_Right_" + std::to_wstring(i);
    wchar_t labelBuf[256], leftBuf[MAX_PATH], rightBuf[MAX_PATH];
    if (GetPrivateProfileStringW(L"DiredPairs", keyLabel.c_str(), L"", labelBuf, 256, path.c_str()) > 0) {
      GetPrivateProfileStringW(L"DiredPairs", keyLeft.c_str(), L"", leftBuf, MAX_PATH, path.c_str());
      GetPrivateProfileStringW(L"DiredPairs", keyRight.c_str(), L"", rightBuf, MAX_PATH, path.c_str());
      m_diredPairs.push_back({labelBuf, leftBuf, rightBuf});
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
  WriteInt(L"Window", L"NoTitleBar", m_noTitleBar ? 1 : 0);

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
  WriteInt(L"Editor", L"VTDebug", m_vtDebug ? 1 : 0);
  WriteInt(L"Editor", L"ReverseScrollDirection", m_reverseScrollDirection ? 1 : 0);
  WriteInt(L"Editor", L"HideMessagesBuffer", m_hideMessagesBuffer ? 1 : 0);
  WriteInt(L"TabGrid", L"CellWidth",  m_tabGridCellW);
  WriteInt(L"TabGrid", L"CellHeight", m_tabGridCellH);
  WriteInt(L"TabGrid", L"RefreshEnabled",    m_tabGridRefreshEnabled ? 1 : 0);
  WriteInt(L"TabGrid", L"RefreshIntervalMs", m_tabGridRefreshIntervalMs);
  if (!m_defaultExtension.empty()) {
    WritePrivateProfileStringW(L"Editor", L"DefaultExtension",
                               m_defaultExtension.c_str(), path.c_str());
  }
  if (!m_pluginsDirectory.empty()) {
    WritePrivateProfileStringW(L"Editor", L"PluginsDirectory",
                               m_pluginsDirectory.c_str(), path.c_str());
  }
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
  if (!m_findPattern.empty()) {
    WritePrivateProfileStringW(L"Editor", L"FindPattern",
                               m_findPattern.c_str(), path.c_str());
  }
  if (!m_findExtFilter.empty()) {
    WritePrivateProfileStringW(L"Editor", L"FindExtFilter",
                               m_findExtFilter.c_str(), path.c_str());
  }

  WritePrivateProfileStringW(L"Recent", NULL, NULL, path.c_str());
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

  WriteInt(L"Session", L"EnableSessionManagement", m_enableSessionManagement ? 1 : 0);

  SaveCliEntries();
  SaveAppEntries();
  SaveFolderEntries();
  SaveHiddenPlugins();
  SaveThemes();
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

bool SettingsManager::IsPluginHidden(const std::wstring &name) const {
  for (const auto &h : m_hiddenPlugins) {
    if (h == name) return true;
  }
  return false;
}

void SettingsManager::ToggleHiddenPlugin(const std::wstring &name) {
  for (auto it = m_hiddenPlugins.begin(); it != m_hiddenPlugins.end(); ++it) {
    if (*it == name) { m_hiddenPlugins.erase(it); return; }
  }
  m_hiddenPlugins.push_back(name);
}

void SettingsManager::SaveHiddenPlugins() {
  std::wstring path = GetSettingsPath();
  std::wstring list;
  for (size_t i = 0; i < m_hiddenPlugins.size(); ++i) {
    if (i > 0) list += L",";
    list += m_hiddenPlugins[i];
  }
  WritePrivateProfileStringW(L"HiddenPlugins", L"List", list.c_str(), path.c_str());
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

void SettingsManager::AddCliEntry(const std::wstring &cmd, const std::wstring &folder, int encoding, int shellType, int iconIndex, const std::wstring &label) {
  m_cliEntries.push_back({cmd, folder, encoding, shellType, iconIndex, label});
}

void SettingsManager::RemoveCliEntry(size_t index) {
  if (index < m_cliEntries.size()) {
    m_cliEntries.erase(m_cliEntries.begin() + index);
  }
}

void SettingsManager::SaveCliEntries() {
  std::wstring path = GetSettingsPath();
  WritePrivateProfileStringW(L"CLI", NULL, NULL, path.c_str());
  for (size_t i = 0; i < m_cliEntries.size(); ++i) {
    std::wstring keyCmd = L"CLI_Cmd_" + std::to_wstring(i);
    std::wstring keyDir = L"CLI_Dir_" + std::to_wstring(i);
    std::wstring keyEnc = L"CLI_Enc_" + std::to_wstring(i);
    std::wstring keyShl = L"CLI_Shell_" + std::to_wstring(i);
    WritePrivateProfileStringW(L"CLI", keyCmd.c_str(), m_cliEntries[i].command.c_str(), path.c_str());
    WritePrivateProfileStringW(L"CLI", keyDir.c_str(), m_cliEntries[i].folder.c_str(), path.c_str());
    WritePrivateProfileStringW(L"CLI", keyEnc.c_str(), std::to_wstring(m_cliEntries[i].encoding).c_str(), path.c_str());
    WritePrivateProfileStringW(L"CLI", keyShl.c_str(), std::to_wstring(m_cliEntries[i].shellType).c_str(), path.c_str());
    std::wstring keyIcn = L"CLI_Icon_" + std::to_wstring(i);
    WritePrivateProfileStringW(L"CLI", keyIcn.c_str(), std::to_wstring(m_cliEntries[i].iconIndex).c_str(), path.c_str());
    std::wstring keyLbl = L"CLI_Lbl_" + std::to_wstring(i);
    WritePrivateProfileStringW(L"CLI", keyLbl.c_str(), m_cliEntries[i].label.c_str(), path.c_str());
  }
}

void SettingsManager::AddAppEntry(const std::wstring &path, const std::wstring &label) {
  m_appEntries.push_back({path, label});
}

void SettingsManager::RemoveAppEntry(size_t index) {
  if (index < m_appEntries.size()) {
    m_appEntries.erase(m_appEntries.begin() + index);
  }
}

void SettingsManager::SaveAppEntries() {
  std::wstring path = GetSettingsPath();
  WritePrivateProfileStringW(L"AppEntries", NULL, NULL, path.c_str());
  for (size_t i = 0; i < m_appEntries.size(); ++i) {
    std::wstring keyPath = L"App_Path_" + std::to_wstring(i);
    std::wstring keyLbl = L"App_Lbl_" + std::to_wstring(i);
    WritePrivateProfileStringW(L"AppEntries", keyPath.c_str(), m_appEntries[i].path.c_str(), path.c_str());
    WritePrivateProfileStringW(L"AppEntries", keyLbl.c_str(), m_appEntries[i].label.c_str(), path.c_str());
  }
}

void SettingsManager::AddFolderEntry(const std::wstring &path, const std::wstring &label) {
  m_folderEntries.push_back({path, label});
}

void SettingsManager::UpdateFolderEntry(size_t index, const std::wstring &path, const std::wstring &label) {
  if (index < m_folderEntries.size()) {
    m_folderEntries[index] = {path, label};
  }
}

void SettingsManager::RemoveFolderEntry(size_t index) {
  if (index < m_folderEntries.size()) {
    m_folderEntries.erase(m_folderEntries.begin() + index);
  }
}

void SettingsManager::SaveFolderEntries() {
  std::wstring path = GetSettingsPath();
  WritePrivateProfileStringW(L"FolderEntries", NULL, NULL, path.c_str());
  for (size_t i = 0; i < m_folderEntries.size(); ++i) {
    std::wstring keyPath = L"Folder_Path_" + std::to_wstring(i);
    std::wstring keyLbl = L"Folder_Lbl_" + std::to_wstring(i);
    WritePrivateProfileStringW(L"FolderEntries", keyPath.c_str(), m_folderEntries[i].path.c_str(), path.c_str());
    WritePrivateProfileStringW(L"FolderEntries", keyLbl.c_str(), m_folderEntries[i].label.c_str(), path.c_str());
  }
}

void SettingsManager::AddDiredPair(const std::wstring &label, const std::wstring &leftDir, const std::wstring &rightDir) {
  m_diredPairs.push_back({label, leftDir, rightDir});
}

void SettingsManager::RemoveDiredPair(size_t index) {
  if (index < m_diredPairs.size()) {
    m_diredPairs.erase(m_diredPairs.begin() + index);
  }
}

void SettingsManager::SaveDiredPairs() {
  std::wstring path = GetSettingsPath();
  WritePrivateProfileStringW(L"DiredPairs", NULL, NULL, path.c_str());
  for (size_t i = 0; i < m_diredPairs.size(); ++i) {
    std::wstring keyLabel = L"DP_Label_" + std::to_wstring(i);
    std::wstring keyLeft  = L"DP_Left_" + std::to_wstring(i);
    std::wstring keyRight = L"DP_Right_" + std::to_wstring(i);
    WritePrivateProfileStringW(L"DiredPairs", keyLabel.c_str(), m_diredPairs[i].label.c_str(), path.c_str());
    WritePrivateProfileStringW(L"DiredPairs", keyLeft.c_str(), m_diredPairs[i].leftDir.c_str(), path.c_str());
    WritePrivateProfileStringW(L"DiredPairs", keyRight.c_str(), m_diredPairs[i].rightDir.c_str(), path.c_str());
  }
}

void SettingsManager::AddTheme(const ThemeEntry &theme) {
  m_themes.push_back(theme);
}

void SettingsManager::RemoveTheme(size_t index) {
  if (index < m_themes.size()) {
    if (m_themes[index].name == m_activeTheme && m_themes.size() > 1) {
      size_t next = (index == 0) ? 1 : 0;
      m_activeTheme = m_themes[next].name;
    }
    m_themes.erase(m_themes.begin() + index);
  }
}

const ThemeEntry *SettingsManager::FindTheme(const std::wstring &name) const {
  for (const auto &t : m_themes) {
    if (t.name == name) return &t;
  }
  return nullptr;
}

void SettingsManager::SaveThemes() {
  std::wstring path = GetSettingsPath();
  WritePrivateProfileStringW(L"Theme", NULL, NULL, path.c_str());
  WritePrivateProfileStringW(L"Theme", L"Active", m_activeTheme.c_str(), path.c_str());

  WritePrivateProfileStringW(L"Themes", NULL, NULL, path.c_str());
  std::wstring list;
  for (size_t i = 0; i < m_themes.size(); ++i) {
    if (i > 0) list += L",";
    list += m_themes[i].name;
  }
  WritePrivateProfileStringW(L"Themes", L"List", list.c_str(), path.c_str());

  auto WriteHex = [&](const wchar_t *sect, const wchar_t *key, const std::wstring &val) {
    WritePrivateProfileStringW(sect, key, val.c_str(), path.c_str());
  };

  for (const auto &t : m_themes) {
    std::wstring sect = L"Theme_" + t.name;
    WritePrivateProfileStringW(sect.c_str(), NULL, NULL, path.c_str());
    WriteHex(sect.c_str(), L"Background", t.background);
    WriteHex(sect.c_str(), L"Foreground", t.foreground);
    WriteHex(sect.c_str(), L"Caret", t.caret);
    WriteHex(sect.c_str(), L"Selection", t.selection);
    WriteHex(sect.c_str(), L"LineNumbers", t.lineNumbers);
    WriteHex(sect.c_str(), L"Keyword", t.keyword);
    WriteHex(sect.c_str(), L"String", t.string);
    WriteHex(sect.c_str(), L"Number", t.number);
    WriteHex(sect.c_str(), L"Comment", t.comment);
    WriteHex(sect.c_str(), L"Function", t.function);
  }
}

// -----------------------------------------------------------------------
// Session management
// -----------------------------------------------------------------------

std::wstring SettingsManager::GetSessionsDirectory() const {
  std::wstring dir = GetAppDataPath() + L"\\sessions";
  CreateDirectoryW(dir.c_str(), NULL);
  return dir;
}

std::wstring SettingsManager::GetSessionIndexPath() const {
  return GetSessionsDirectory() + L"\\sessions.ini";
}

std::wstring SettingsManager::GetSessionDir(int index) const {
  return GetSessionsDirectory() + L"\\session_" + std::to_wstring(index);
}

int SettingsManager::SaveSession(const std::wstring &name) {
  std::wstring idxPath = GetSessionIndexPath();

  // Find next free session index
  int index = 0;
  wchar_t buf[64];
  while (GetPrivateProfileStringW(L"Sessions", std::to_wstring(index).c_str(),
                                  L"", buf, 64, idxPath.c_str()) > 0) {
    index++;
  }

  std::wstring sessionDir = GetSessionDir(index);
  CreateDirectoryW(sessionDir.c_str(), NULL);

  // Write session metadata
  std::wstring iniPath = sessionDir + L"\\session.ini";

  auto WriteInt = [&](const wchar_t *sect, const wchar_t *key, int val) {
    WritePrivateProfileStringW(sect, key, std::to_wstring(val).c_str(), iniPath.c_str());
  };

  WritePrivateProfileStringW(L"Session", L"Name", name.c_str(), iniPath.c_str());

  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t timeBuf[64];
  swprintf(timeBuf, 64, L"%04d-%02d-%02d %02d:%02d:%02d",
           st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
  WritePrivateProfileStringW(L"Session", L"Time", timeBuf, iniPath.c_str());

  extern Editor *g_editor;
  int bufferCount = 0;
  if (g_editor) {
    const auto &buffers = g_editor->GetBuffers();
    for (size_t i = 0; i < buffers.size(); ++i) {
      auto *buf = buffers[i].get();
      std::wstring section = L"Buffer_" + std::to_wstring(i);
      WritePrivateProfileStringW(section.c_str(), L"Path", buf->GetPath().c_str(), iniPath.c_str());
      WriteInt(section.c_str(), L"CaretPos", (int)buf->GetCaretPos());
      WriteInt(section.c_str(), L"SelectionAnchor", (int)buf->GetSelectionAnchor());
      WriteInt(section.c_str(), L"ScrollLine", (int)buf->GetScrollLine());
      WriteInt(section.c_str(), L"ScrollX", (int)buf->GetScrollX());
      WriteInt(section.c_str(), L"Encoding", (int)buf->GetEncoding());
      WriteInt(section.c_str(), L"IsDirty", buf->IsDirty() ? 1 : 0);
      WriteInt(section.c_str(), L"IsScratch", buf->IsScratch() ? 1 : 0);
      WriteInt(section.c_str(), L"IsShell", buf->IsShell() ? 1 : 0);

      // Save folded lines
      std::wstring foldedStr;
      for (auto line : buf->GetFoldedLines()) {
        if (!foldedStr.empty()) foldedStr += L",";
        foldedStr += std::to_wstring((int)line);
      }
      WritePrivateProfileStringW(section.c_str(), L"FoldedLines", foldedStr.c_str(), iniPath.c_str());

      // Save content for non-file-backed buffers
      if (buf->GetPath().empty() || buf->IsScratch() || buf->IsShell()) {
        std::wstring contentFile = L"buffer_" + std::to_wstring(i) + L".txt";
        std::wstring contentPath = sessionDir + L"\\" + contentFile;
        std::string content = buf->GetText(0, buf->GetTotalLength());
        FILE *f = _wfopen(contentPath.c_str(), L"wb");
        if (f) {
          fwrite(content.c_str(), 1, content.size(), f);
          fclose(f);
        }
        WritePrivateProfileStringW(section.c_str(), L"ContentFile", contentFile.c_str(), iniPath.c_str());
      }

      bufferCount++;
    }
  }

  WriteInt(L"Session", L"BufferCount", bufferCount);

  // Update sessions.ini master index
  WritePrivateProfileStringW(L"Sessions", std::to_wstring(index).c_str(),
                             name.c_str(), idxPath.c_str());

  return index;
}

bool SettingsManager::LoadSession(int index) {
  std::wstring sessionDir = GetSessionDir(index);
  std::wstring iniPath = sessionDir + L"\\session.ini";

  if (GetFileAttributesW(sessionDir.c_str()) == INVALID_FILE_ATTRIBUTES)
    return false;

  int bufferCount = GetPrivateProfileIntW(L"Session", L"BufferCount", 0, iniPath.c_str());
  if (bufferCount == 0) return false;

  extern Editor *g_editor;
  if (!g_editor) return false;

  // Clear existing buffers (except close them properly)
  while (g_editor->GetBuffers().size() > 0) {
    g_editor->CloseBuffer(0);
  }

  for (int i = 0; i < bufferCount; ++i) {
    std::wstring section = L"Buffer_" + std::to_wstring(i);
    wchar_t pathBuf[MAX_PATH];
    GetPrivateProfileStringW(section.c_str(), L"Path", L"", pathBuf, MAX_PATH, iniPath.c_str());
    std::wstring path = pathBuf;

    wchar_t contentFileBuf[256];
    GetPrivateProfileStringW(section.c_str(), L"ContentFile", L"", contentFileBuf, 256, iniPath.c_str());
    std::wstring contentFile = contentFileBuf;

    size_t bufIdx;
    if (!path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
      bufIdx = g_editor->OpenFile(path);
    } else if (!contentFile.empty()) {
      g_editor->NewFile("Restored");
      bufIdx = g_editor->GetBuffers().size() - 1;
      std::wstring contentPath = sessionDir + L"\\" + contentFile;
      FILE *f = _wfopen(contentPath.c_str(), L"rb");
      if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len > 0) {
          std::string content((size_t)len, '\0');
          fread(&content[0], 1, len, f);
          g_editor->GetBuffers()[bufIdx]->Insert(0, content);
        }
        fclose(f);
      }
    } else {
      continue;
    }

    auto *buf = g_editor->GetBuffers()[bufIdx].get();
    buf->SetCaretPos(GetPrivateProfileIntW(section.c_str(), L"CaretPos", 0, iniPath.c_str()));
    buf->SetSelectionAnchor(GetPrivateProfileIntW(section.c_str(), L"SelectionAnchor", 0, iniPath.c_str()));
    buf->SetScrollLine(GetPrivateProfileIntW(section.c_str(), L"ScrollLine", 0, iniPath.c_str()));
    buf->SetScrollX((float)GetPrivateProfileIntW(section.c_str(), L"ScrollX", 0, iniPath.c_str()));
    buf->SetEncoding((Encoding)GetPrivateProfileIntW(section.c_str(), L"Encoding", 0, iniPath.c_str()));

    int isScratch = GetPrivateProfileIntW(section.c_str(), L"IsScratch", 0, iniPath.c_str());
    if (isScratch) buf->SetScratch(true);
    int isShell = GetPrivateProfileIntW(section.c_str(), L"IsShell", 0, iniPath.c_str());
    if (isShell) buf->SetShell(true);

    // Restore folded lines
    wchar_t foldedBuf[4096];
    if (GetPrivateProfileStringW(section.c_str(), L"FoldedLines", L"", foldedBuf, 4096, iniPath.c_str()) > 0) {
      std::wstring fStr = foldedBuf;
      size_t start = 0, end;
      while ((end = fStr.find(L',', start)) != std::wstring::npos) {
        int line = std::stoi(fStr.substr(start, end - start));
        buf->FoldLine((size_t)line);
        start = end + 1;
      }
      if (start < fStr.size()) {
        int line = std::stoi(fStr.substr(start));
        buf->FoldLine((size_t)line);
      }
    }
  }

  return true;
}

bool SettingsManager::DeleteSession(int index) {
  std::wstring sessionDir = GetSessionDir(index);
  if (GetFileAttributesW(sessionDir.c_str()) == INVALID_FILE_ATTRIBUTES)
    return false;

  // Delete all files in session directory
  std::wstring searchPath = sessionDir + L"\\*";
  WIN32_FIND_DATAW fd;
  HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      std::wstring fname = fd.cFileName;
      if (fname != L"." && fname != L"..") {
        std::wstring fullPath = sessionDir + L"\\" + fname;
        DeleteFileW(fullPath.c_str());
      }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
  }
  RemoveDirectoryW(sessionDir.c_str());

  // Remove from master index
  std::wstring idxPath = GetSessionIndexPath();
  WritePrivateProfileStringW(L"Sessions", std::to_wstring(index).c_str(), NULL, idxPath.c_str());

  return true;
}

std::vector<SessionInfo> SettingsManager::GetSessionList() const {
  std::vector<SessionInfo> list;
  std::wstring idxPath = GetSessionIndexPath();

  wchar_t buf[4096];
  if (GetPrivateProfileSectionW(L"Sessions", buf, 4096, idxPath.c_str()) > 0) {
    wchar_t *p = buf;
    while (*p) {
      std::wstring line(p);
      size_t eqPos = line.find(L'=');
      if (eqPos != std::wstring::npos) {
        SessionInfo info;
        info.index = std::stoi(line.substr(0, eqPos));
        info.name = line.substr(eqPos + 1);

        // Read time and buffer count from session ini
        std::wstring iniPath = GetSessionDir(info.index) + L"\\session.ini";
        wchar_t timeBuf[64];
        if (GetPrivateProfileStringW(L"Session", L"Time", L"", timeBuf, 64, iniPath.c_str()) > 0)
          info.time = timeBuf;
        info.bufferCount = GetPrivateProfileIntW(L"Session", L"BufferCount", 0, iniPath.c_str());

        list.push_back(info);
      }
      p += line.length() + 1;
    }
  }

  return list;
}

int SettingsManager::GetAutoSaveSessionIndex() const {
  return GetPrivateProfileIntW(L"Session", L"AutoSaveIndex", -1, GetSessionIndexPath().c_str());
}

void SettingsManager::SetAutoSaveSessionIndex(int index) {
  WritePrivateProfileStringW(L"Session", L"AutoSaveIndex",
                             std::to_wstring(index).c_str(), GetSessionIndexPath().c_str());
}

void SettingsManager::ClearAutoSaveSession() {
  int idx = GetAutoSaveSessionIndex();
  if (idx >= 0) {
    DeleteSession(idx);
    WritePrivateProfileStringW(L"Session", L"AutoSaveIndex", NULL, GetSessionIndexPath().c_str());
  }
}
