extern "C" const char* g_copyrightText;

static std::wstring GetLangArg() {
    Language lang = Localization::Instance().GetCurrentLanguage();
    switch (lang) {
    case Language::English:   return L"--lang en";
    case Language::Japanese:  return L"--lang jp";
    case Language::Spanish:   return L"--lang es";
    case Language::French:    return L"--lang fr";
    case Language::German:    return L"--lang de";
    }
    return L"--lang en";
}

struct EnumData {
  DWORD processId;
  HWND hwnd;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  EnumData *data = (EnumData *)lParam;
  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  if (processId == data->processId) {
    if (GetWindow(hwnd, GW_OWNER) == NULL) {
      data->hwnd = hwnd;
      return FALSE; // Stop enumerating
    }
  }
  return TRUE;
}

struct SearchThreadParams {
  HWND hwnd;
  int appIdx;
  HANDLE hProcess;
};

static DWORD WINAPI FastFileSearchThread(LPVOID lpParam) {
  SearchThreadParams *params = (SearchThreadParams *)lpParam;
  HWND hwnd = params->hwnd;
  int appIdx = params->appIdx;
  delete params;

  wchar_t modulePath[MAX_PATH];
  GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  std::wstring dir = modulePath;
  size_t pos = dir.find_last_of(L"\\/");
  if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);
  std::wstring exePath = dir + L"Application\\FastFileSearch\\build\\Release\\FastFileSearch.exe";
  std::wstring workDir = dir + L"Application\\FastFileSearch\\build\\Release";

  STARTUPINFOW si = { sizeof(si) };
  PROCESS_INFORMATION pi = { 0 };
  std::wstring cmd = L"\"" + exePath + L"\" --embedded " + GetLangArg();
  if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, &workDir[0], &si, &pi)) {
    HWND foundHwnd = NULL;
    for (int i = 0; i < 50; ++i) {
      Sleep(100);
      EnumData data = { pi.dwProcessId, NULL };
      EnumWindows(EnumWindowsProc, (LPARAM)&data);
      if (data.hwnd != NULL) {
        foundHwnd = data.hwnd;
        break;
      }
    }
    if (foundHwnd) {
      PostMessageW(hwnd, WM_EMBED_APP, (WPARAM)foundHwnd, (LPARAM)appIdx);
      PostMessageW(hwnd, WM_SET_PROCESS_HANDLE, (WPARAM)appIdx, (LPARAM)pi.hProcess);
    } else {
      CloseHandle(pi.hProcess);
    }
    CloseHandle(pi.hThread);
  }
  return 0;
}

std::wstring GetPluginPath(const std::wstring &name) {
    wchar_t modPath[MAX_PATH];
    GetModuleFileNameW(nullptr, modPath, MAX_PATH);
    std::wstring exeDir = modPath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\/"));
    std::wstring path = exeDir + L"\\plugins\\" + name;
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        path = exeDir + L"\\" + name;
    return path;
}

void OpenFastFileSearch(HWND hwnd) {
  AppTabInfo tab;
  tab.label = L"File Search";
  tab.type = 2;
  tab.iImage = AddFileTypeIcon(g_tabImageList, L"");
  if (!IsUserAnAdmin()) {
    tab.hwnd = CreateWindowExW(0, L"STATIC",
                               L"\n\n\n\n\n\n\n\n\n⚠️ Fast File Search requires Administrator privileges.\n\nPlease relaunch Ecode as Administrator (Right-click -> Run as Administrator) to use this tool.",
                               WS_CHILD | WS_VISIBLE | SS_CENTER,
                               0, 0, 100, 100, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
  } else {
    tab.hwnd = CreateWindowExW(0, L"STATIC", L"Starting Fast File Search...",
                               WS_CHILD | WS_VISIBLE | SS_CENTER,
                               0, 0, 100, 100, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
  }
  tab.data = nullptr;
  g_appTabs.push_back(std::move(tab));
  int appIdx = static_cast<int>(g_appTabs.size()) - 1;
  g_activeAppTab = appIdx;


  RECT rc;
  GetClientRect(hwnd, &rc);
  SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));

  g_suppressTabChange = true;
  size_t bufCount = g_editor->GetBuffers().size();
  TabCtrl_SetCurSel(g_tabHwnd, static_cast<int>(bufCount) + appIdx);
  g_suppressTabChange = false;

  for (auto &t : g_appTabs) {
    if (t.hwnd) {
      ShowWindow(t.hwnd, (t.hwnd == g_appTabs[appIdx].hwnd) ? SW_SHOW : SW_HIDE);
    }
  }


  UpdateMenu(hwnd);
  InvalidateRect(hwnd, NULL, FALSE);

  if (IsUserAnAdmin()) {
    SearchThreadParams *params = new SearchThreadParams();
    params->hwnd = hwnd;
    params->appIdx = appIdx;
    CreateThread(NULL, 0, FastFileSearchThread, params, 0, NULL);
  }
}

struct CSVThreadParams {
  HWND hwnd;
  int appIdx;
  HANDLE hProcess;
};

static DWORD WINAPI CSVEditorThread(LPVOID lpParam) {
  CSVThreadParams *params = (CSVThreadParams *)lpParam;
  HWND hwnd = params->hwnd;
  int appIdx = params->appIdx;
  delete params;

  wchar_t modulePath[MAX_PATH];
  GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  std::wstring dir = modulePath;
  size_t pos = dir.find_last_of(L"\\/");
  if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);
  std::wstring exePath = dir + L"Application\\CSVEditor\\build\\Release\\CSVEditor.exe";
  std::wstring workDir = dir + L"Application\\CSVEditor\\build\\Release";

  STARTUPINFOW si = { sizeof(si) };
  PROCESS_INFORMATION pi = { 0 };
  std::wstring cmd = L"\"" + exePath + L"\" --embedded " + GetLangArg();
  if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, &workDir[0], &si, &pi)) {
    HWND foundHwnd = NULL;
    for (int i = 0; i < 50; ++i) {
      Sleep(100);
      EnumData data = { pi.dwProcessId, NULL };
      EnumWindows(EnumWindowsProc, (LPARAM)&data);
      if (data.hwnd != NULL) {
        foundHwnd = data.hwnd;
        break;
      }
    }
    if (foundHwnd) {
      PostMessageW(hwnd, WM_EMBED_APP, (WPARAM)foundHwnd, (LPARAM)appIdx);
      PostMessageW(hwnd, WM_SET_PROCESS_HANDLE, (WPARAM)appIdx, (LPARAM)pi.hProcess);
    } else {
      CloseHandle(pi.hProcess);
    }
    CloseHandle(pi.hThread);
  }
  return 0;
}

void OpenCSVEditor(HWND hwnd) {
  AppTabInfo tab;
  tab.label = L"CSV Editor";
  tab.type = 3;
  tab.iImage = AddExeIcon(g_tabImageList, GetPluginPath(L"CSVEditor.exe"));
  tab.hwnd = CreateWindowExW(0, L"STATIC", L"Starting CSV Editor...",
                             WS_CHILD | WS_VISIBLE | SS_CENTER,
                             0, 0, 100, 100, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
  tab.data = nullptr;
  g_appTabs.push_back(std::move(tab));
  int appIdx = static_cast<int>(g_appTabs.size()) - 1;
  g_activeAppTab = appIdx;


  RECT rc;
  GetClientRect(hwnd, &rc);
  SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));

  g_suppressTabChange = true;
  size_t bufCount = g_editor->GetBuffers().size();
  TabCtrl_SetCurSel(g_tabHwnd, static_cast<int>(bufCount) + appIdx);
  g_suppressTabChange = false;

  for (auto &t : g_appTabs) {
    if (t.hwnd) {
      ShowWindow(t.hwnd, (t.hwnd == g_appTabs[appIdx].hwnd) ? SW_SHOW : SW_HIDE);
    }
  }


  UpdateMenu(hwnd);
  InvalidateRect(hwnd, NULL, FALSE);

  CSVThreadParams *params = new CSVThreadParams();
  params->hwnd = hwnd;
  params->appIdx = appIdx;
  CreateThread(NULL, 0, CSVEditorThread, params, 0, NULL);
}

struct JYThreadParams {
  HWND hwnd;
  int appIdx;
  HANDLE hProcess;
};

static DWORD WINAPI JYEditorThread(LPVOID lpParam) {
  JYThreadParams *params = (JYThreadParams *)lpParam;
  HWND hwnd = params->hwnd;
  int appIdx = params->appIdx;
  delete params;

  wchar_t modulePath[MAX_PATH];
  GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  std::wstring dir = modulePath;
  size_t pos = dir.find_last_of(L"\\/");
  if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);
  std::wstring exePath = dir + L"Application\\JYEditor\\build\\Release\\JYEditor.exe";
  std::wstring workDir = dir + L"Application\\JYEditor\\build\\Release";

  STARTUPINFOW si = { sizeof(si) };
  PROCESS_INFORMATION pi = { 0 };
  std::wstring cmd = L"\"" + exePath + L"\" --embedded " + GetLangArg();
  if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, &workDir[0], &si, &pi)) {
    HWND foundHwnd = NULL;
    for (int i = 0; i < 50; ++i) {
      Sleep(100);
      EnumData data = { pi.dwProcessId, NULL };
      EnumWindows(EnumWindowsProc, (LPARAM)&data);
      if (data.hwnd != NULL) {
        foundHwnd = data.hwnd;
        break;
      }
    }
    if (foundHwnd) {
      PostMessageW(hwnd, WM_EMBED_APP, (WPARAM)foundHwnd, (LPARAM)appIdx);
      PostMessageW(hwnd, WM_SET_PROCESS_HANDLE, (WPARAM)appIdx, (LPARAM)pi.hProcess);
    } else {
      CloseHandle(pi.hProcess);
    }
    CloseHandle(pi.hThread);
  }
  return 0;
}

void OpenJYEditor(HWND hwnd) {
  AppTabInfo tab;
  tab.label = L"JY Editor";
  tab.type = 4;
  tab.iImage = AddExeIcon(g_tabImageList, GetPluginPath(L"JYEditor.exe"));
  tab.hwnd = CreateWindowExW(0, L"STATIC", L"Starting JY Editor...",
                             WS_CHILD | WS_VISIBLE | SS_CENTER,
                             0, 0, 100, 100, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
  tab.data = nullptr;
  g_appTabs.push_back(std::move(tab));
  int appIdx = static_cast<int>(g_appTabs.size()) - 1;
  g_activeAppTab = appIdx;


  RECT rc;
  GetClientRect(hwnd, &rc);
  SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));

  g_suppressTabChange = true;
  size_t bufCount = g_editor->GetBuffers().size();
  TabCtrl_SetCurSel(g_tabHwnd, static_cast<int>(bufCount) + appIdx);
  g_suppressTabChange = false;

  for (auto &t : g_appTabs) {
    if (t.hwnd) {
      ShowWindow(t.hwnd, (t.hwnd == g_appTabs[appIdx].hwnd) ? SW_SHOW : SW_HIDE);
    }
  }


  UpdateMenu(hwnd);
  InvalidateRect(hwnd, NULL, FALSE);

  JYThreadParams *params = new JYThreadParams();
  params->hwnd = hwnd;
  params->appIdx = appIdx;
  CreateThread(NULL, 0, JYEditorThread, params, 0, NULL);
}

struct DiredThreadParams {
  HWND hwnd;
  int appIdx;
  HANDLE hProcess;
};

static DWORD WINAPI DiredThread(LPVOID lpParam) {
  DiredThreadParams *params = (DiredThreadParams *)lpParam;
  HWND hwnd = params->hwnd;
  int appIdx = params->appIdx;
  delete params;

  // Get current directory for initial path
  wchar_t curDir[MAX_PATH];
  GetCurrentDirectoryW(MAX_PATH, curDir);

  STARTUPINFOW si = { sizeof(si) };
  PROCESS_INFORMATION pi = { 0 };
  std::wstring diredPath;
  wchar_t modulePath[MAX_PATH];
  GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  diredPath = std::wstring(modulePath);
  size_t pos = diredPath.find_last_of(L"\\/");
  if (pos != std::wstring::npos) {
    std::wstring exeDir = diredPath.substr(0, pos + 1);
    diredPath = exeDir + L"plugins\\Dired.exe";
    if (GetFileAttributesW(diredPath.c_str()) == INVALID_FILE_ATTRIBUTES)
      diredPath = exeDir + L"Dired.exe";
  } else {
    diredPath = L"plugins\\Dired.exe";
  }

  std::wstring cmd = L"\"" + diredPath + L"\" --embedded " + GetLangArg() + L" \"" + curDir + L"\"";
  if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0,
                     nullptr, nullptr, &si, &pi)) {
    HWND foundHwnd = nullptr;
    for (int i = 0; i < 50; ++i) {
      Sleep(100);
      EnumData data = { pi.dwProcessId, nullptr };
      EnumWindows(EnumWindowsProc, (LPARAM)&data);
      if (data.hwnd != nullptr) {
        foundHwnd = data.hwnd;
        break;
      }
    }
    if (foundHwnd) {
      PostMessageW(hwnd, WM_EMBED_APP, (WPARAM)foundHwnd, (LPARAM)appIdx);
      PostMessageW(hwnd, WM_SET_PROCESS_HANDLE, (WPARAM)appIdx, (LPARAM)pi.hProcess);
    } else {
      CloseHandle(pi.hProcess);
    }
    CloseHandle(pi.hThread);
  }
  return 0;
}

void OpenDired(HWND hwnd) {
  AppTabInfo tab;
  tab.label = L"Dired";
  tab.type = 5;
  tab.iImage = AddExeIcon(g_tabImageList, GetPluginPath(L"Dired.exe"));
  tab.hwnd = CreateWindowExW(0, L"STATIC", L"Starting Dired...",
                             WS_CHILD | WS_VISIBLE | SS_CENTER,
                             0, 0, 100, 100, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
  tab.data = nullptr;
  g_appTabs.push_back(std::move(tab));
  int appIdx = static_cast<int>(g_appTabs.size()) - 1;
  g_activeAppTab = appIdx;


  RECT rc;
  GetClientRect(hwnd, &rc);
  SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));

  g_suppressTabChange = true;
  size_t bufCount = g_editor->GetBuffers().size();
  TabCtrl_SetCurSel(g_tabHwnd, static_cast<int>(bufCount) + appIdx);
  g_suppressTabChange = false;

  for (auto &t : g_appTabs) {
    if (t.hwnd) {
      ShowWindow(t.hwnd, (t.hwnd == g_appTabs[appIdx].hwnd) ? SW_SHOW : SW_HIDE);
    }
  }


  UpdateMenu(hwnd);
  InvalidateRect(hwnd, NULL, FALSE);

  DiredThreadParams *params = new DiredThreadParams();
  params->hwnd = hwnd;
  params->appIdx = appIdx;
  CreateThread(nullptr, 0, DiredThread, params, 0, nullptr);
}

void KillAppProcessByIndex(HWND hwnd, size_t idx) {
  if (idx >= g_appTabs.size()) return;
  if (g_appTabs[idx].hProcess) {
    TerminateProcess(g_appTabs[idx].hProcess, 0);
    CloseHandle(g_appTabs[idx].hProcess);
    g_appTabs[idx].hProcess = nullptr;
  }
  if (g_appTabs[idx].hwnd) DestroyWindow(g_appTabs[idx].hwnd);
  g_appTabs.erase(g_appTabs.begin() + idx);
  if (g_activeAppTab == static_cast<int>(idx)) g_activeAppTab = -1;
  else if (g_activeAppTab > static_cast<int>(idx)) g_activeAppTab--;
  UpdateMenu(hwnd);
  InvalidateRect(hwnd, NULL, FALSE);
}

void KillActiveAppProcess(HWND hwnd) {
  if (g_activeAppTab >= 0 && static_cast<size_t>(g_activeAppTab) < g_appTabs.size())
    KillAppProcessByIndex(hwnd, static_cast<size_t>(g_activeAppTab));
}

// ---------------------------------------------------------------------------
// Plugin system
// ---------------------------------------------------------------------------
void ScanPlugins() {
  g_plugins.clear();

  wchar_t modulePath[MAX_PATH];
  GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  std::wstring exeDir = modulePath;
  size_t pos = exeDir.find_last_of(L"\\/");
  if (pos != std::wstring::npos) exeDir = exeDir.substr(0, pos + 1);

  // 1. Scan default plugins dir alongside exe
  std::wstring pluginDir = exeDir + L"plugins\\";
  if (GetFileAttributesW(pluginDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
    std::wstring search = pluginDir + L"*.exe";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        std::wstring name = ffd.cFileName;
        size_t dot = name.rfind(L'.');
        if (dot != std::wstring::npos) name = name.substr(0, dot);
        PluginEntry e;
        e.name = name;
        e.path = pluginDir + ffd.cFileName;
        e.hidden = false;
        g_plugins.push_back(std::move(e));
      } while (FindNextFileW(hFind, &ffd));
      FindClose(hFind);
    }
  }

  // 2. Scan user-configured plugins directory (if set)
  std::wstring userDir = SettingsManager::Instance().GetPluginsDirectory();
  if (!userDir.empty() && GetFileAttributesW(userDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
    std::wstring search = userDir + L"\\*.exe";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        std::wstring name = ffd.cFileName;
        size_t dot = name.rfind(L'.');
        if (dot != std::wstring::npos) name = name.substr(0, dot);
        // Deduplicate: user dir trumps default dir
        bool dup = false;
        for (auto &p : g_plugins) {
          if (p.name == name) { p.path = userDir + L"\\" + ffd.cFileName; dup = true; break; }
        }
        if (!dup) {
          PluginEntry e;
          e.name = name;
          e.path = userDir + L"\\" + ffd.cFileName;
          e.hidden = false;
          g_plugins.push_back(std::move(e));
        }
      } while (FindNextFileW(hFind, &ffd));
      FindClose(hFind);
    }
  }

  // Mark hidden plugins
  for (auto &p : g_plugins) {
    p.hidden = SettingsManager::Instance().IsPluginHidden(p.name);
  }
}

void LaunchApp(HWND hwnd, const std::wstring& exePath, const std::wstring& args,
                      const std::wstring& label, int type, int iImage = -1) {
  if (GetFileAttributesW(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::wstring msg = L"Plugin not found:\n" + exePath;
    MessageBoxW(hwnd, msg.c_str(), L"Plugin Not Found", MB_OK | MB_ICONWARNING);
    return;
  }

  AppTabInfo tab;
  tab.label = label;
  tab.type  = type;
  if (iImage == -2)
    tab.iImage = -1;
  else
    tab.iImage = iImage >= 0 ? iImage : AddExeIcon(g_tabImageList, exePath);
  tab.hwnd  = CreateWindowExW(0, L"STATIC", L"Starting...",
                               WS_CHILD | WS_VISIBLE | SS_CENTER,
                               0, 0, 100, 100, hwnd, nullptr,
                               GetModuleHandleW(nullptr), nullptr);
  g_appTabs.push_back(std::move(tab));
  int appIdx = static_cast<int>(g_appTabs.size()) - 1;
  g_activeAppTab = appIdx;

  RECT rc;
  GetClientRect(hwnd, &rc);
  SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));

  g_suppressTabChange = true;
  size_t bufCount = g_editor->GetBuffers().size();
  TabCtrl_SetCurSel(g_tabHwnd, static_cast<int>(bufCount) + appIdx);
  g_suppressTabChange = false;

  for (auto& t : g_appTabs)
    if (t.hwnd) ShowWindow(t.hwnd, t.hwnd == g_appTabs[appIdx].hwnd ? SW_SHOW : SW_HIDE);
  ShowScrollBar(hwnd, SB_BOTH, FALSE);

  UpdateMenu(hwnd);
  InvalidateRect(hwnd, NULL, FALSE);

  std::wstring cmdLine = L"\"" + exePath + L"\" --embedded";
  if (exePath.find(L"Terminal.exe") == std::wstring::npos)
    cmdLine += L" " + GetLangArg();
  if (!args.empty()) cmdLine += L" " + args;
  if (g_editor && exePath.find(L"Terminal.exe") != std::wstring::npos)
    g_editor->LogMessage("[CreateProcessW] " + WStringToString(cmdLine));
  STARTUPINFOW si = { sizeof(si) };
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessW(nullptr, &cmdLine[0], nullptr, nullptr, FALSE,
                      0, nullptr, nullptr, &si, &pi))
    return;

  HWND foundHwnd = nullptr;
  for (int i = 0; i < 50 && !foundHwnd; ++i) {
    Sleep(100);
    EnumData data = { pi.dwProcessId, nullptr };
    EnumWindows(EnumWindowsProc, (LPARAM)&data);
    foundHwnd = data.hwnd;
  }
  if (foundHwnd) {
    PostMessageW(hwnd, WM_EMBED_APP, (WPARAM)foundHwnd, (LPARAM)appIdx);
    PostMessageW(hwnd, WM_SET_PROCESS_HANDLE, (WPARAM)appIdx, (LPARAM)pi.hProcess);
  } else {
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
  }
  CloseHandle(pi.hThread);
}

void LaunchPlugin(HWND hwnd, size_t index) {
  if (index >= g_plugins.size()) return;
  LaunchApp(hwnd, g_plugins[index].path, L"", g_plugins[index].name, 10);
}

// ---------------------------------------------------------------------------
// Plugin Configuration Dialog
// ---------------------------------------------------------------------------
INT_PTR CALLBACK PluginConfigDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                     LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    HWND hList = GetDlgItem(hDlg, IDC_PLUGIN_LIST);
    for (size_t i = 0; i < g_plugins.size(); ++i) {
      int idx = (int)SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)g_plugins[i].name.c_str());
      SendMessage(hList, LB_SETITEMDATA, idx, i);
      if (g_plugins[i].hidden)
        SendMessage(hList, LB_SETSEL, TRUE, idx);
    }
    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      HWND hList = GetDlgItem(hDlg, IDC_PLUGIN_LIST);
      int count = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
      for (int i = 0; i < count; ++i) {
        LRESULT sel = SendMessage(hList, LB_GETSEL, i, 0);
        LRESULT idx = SendMessage(hList, LB_GETITEMDATA, i, 0);
        if (idx >= 0 && idx < (int)g_plugins.size()) {
          std::wstring name = g_plugins[(size_t)idx].name;
          bool shouldHide = (sel != 0);
          if (shouldHide != SettingsManager::Instance().IsPluginHidden(name)) {
            SettingsManager::Instance().ToggleHiddenPlugin(name);
          }
        }
      }
      SettingsManager::Instance().SaveHiddenPlugins();
      SettingsManager::Instance().Save();
      ScanPlugins();
      EndDialog(hDlg, IDOK);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

void ShowPluginConfigDialog(HWND hwnd) {
  DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_PLUGIN_CONFIG), hwnd,
             PluginConfigDlgProc);
}

static INT_PTR CALLBACK CopyrightDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_INITDIALOG: {
    wchar_t wbuf[4096];
    MultiByteToWideChar(CP_UTF8, 0, g_copyrightText, -1, wbuf, 4096);
    SetDlgItemTextW(hDlg, IDC_COPYRIGHT_TEXT, wbuf);
    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

static LRESULT HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {
  switch (LOWORD(wParam)) {
  case IDM_VIEW_TABSWITCHER:
    ShowTabSwitcher(hwnd);
    return 0;
  case IDM_FILE_NEW:
    g_editor->NewFile();
    UpdateMenu(hwnd);
    break;
  case IDM_FILE_SCRATCH:
    g_editor->NewFile();
    if (g_editor->GetActiveBuffer()) {
      g_editor->GetActiveBuffer()->SetScratch(true);
      g_editor->GetActiveBuffer()->Insert(
          0, "// Scratch Buffer - Press Ctrl+Enter to execute JS\n");
    }
    UpdateMenu(hwnd);
    break;
  case IDM_LANG_EN:
    Localization::Instance().SetLanguage(Language::English);
    UpdateMenu(hwnd);
    break;
  case IDM_LANG_JP:
    Localization::Instance().SetLanguage(Language::Japanese);
    UpdateMenu(hwnd);
    break;
  case IDM_LANG_ES:
    Localization::Instance().SetLanguage(Language::Spanish);
    UpdateMenu(hwnd);
    break;
  case IDM_LANG_FR:
    Localization::Instance().SetLanguage(Language::French);
    UpdateMenu(hwnd);
    break;
  case IDM_LANG_DE:
    Localization::Instance().SetLanguage(Language::German);
    UpdateMenu(hwnd);
    break;
  case IDM_FILE_EXIT:
    PostQuitMessage(0);
    break;

  case IDM_FILE_OPEN: {
    std::wstring path = Dialogs::OpenFileDialog(hwnd);
    if (!path.empty()) {
      g_editor->OpenFile(path);
      SettingsManager::Instance().AddRecentFile(path);
      UpdateMenu(hwnd);
    }
    break;
  }
  case IDM_FILE_SAVE: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      if (buf->GetPath().empty()) {
        std::wstring path = Dialogs::SaveFileDialog(hwnd);
        if (!path.empty()) {
          if (buf->SaveFile(path)) {
            buf->SetPath(path);
            SettingsManager::Instance().AddRecentFile(path);
          }
        }
      } else {
        buf->SaveFile(buf->GetPath());
      }
    }
    UpdateMenu(hwnd);
    break;
  }
  case IDM_FILE_SAVE_AS: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      std::wstring path = Dialogs::SaveFileDialog(hwnd);
      if (!path.empty()) {
        if (buf->SaveFile(path)) {
          buf->SetPath(path);
          SettingsManager::Instance().AddRecentFile(path);
        }
        UpdateMenu(hwnd);
      }
    }
    break;
  }
  case IDM_HELP_DOC: {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string exePath(buffer);
    std::string dir = exePath.substr(0, exePath.find_last_of("\\/"));

    std::string filename = "documentation.md";
    Language lang = Localization::Instance().GetCurrentLanguage();
    if (lang == Language::Japanese)
      filename = "documentation_jp.md";
    else if (lang == Language::Spanish)
      filename = "documentation_es.md";
    else if (lang == Language::French)
      filename = "documentation_fr.md";
    else if (lang == Language::German)
      filename = "documentation_de.md";

    std::string docPath = dir + "/../doc/" + filename;

    std::ifstream f(docPath);
    if (!f.good()) {
      // Try dev path
      std::string devPath = "d:/ws/Ecode/doc/" + filename;
      std::ifstream f2(devPath);
      if (f2.good()) {
        docPath = devPath;
      } else {
        DebugLog("HandleCommand - Documentation file not found: " + filename,
                 LOG_ERROR);
        MessageBoxA(hwnd,
                    ("Could not find documentation file: " + filename).c_str(),
                    "Error", MB_ICONERROR);
      }
    }

    g_editor->OpenFile(StringToWString(docPath));
    break;
  }

  case IDM_HELP_ABOUT:
    Dialogs::ShowAboutDialog(hwnd);
    break;

  case IDM_HELP_COPYRIGHT:
    DialogBoxW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_COPYRIGHT), hwnd, CopyrightDlgProc);
    break;

  case IDM_HELP_KEYBINDINGS: {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string exePath(buffer);
    std::string dir = exePath.substr(0, exePath.find_last_of("\\/"));

    std::string filename = "keybindings.md";
    Language lang = Localization::Instance().GetCurrentLanguage();
    if (lang == Language::Japanese)
      filename = "keybindings_jp.md";
    else if (lang == Language::Spanish)
      filename = "keybindings_es.md";
    else if (lang == Language::French)
      filename = "keybindings_fr.md";
    else if (lang == Language::German)
      filename = "keybindings_de.md";

    std::string docPath = dir + "/../doc/" + filename;

    std::ifstream f(docPath);
    if (!f.good()) {
      // Try dev path
      std::string devPath = "d:/ws/Ecode/doc/" + filename;
      std::ifstream f2(devPath);
      if (f2.good()) {
        docPath = devPath;
      } else {
        DebugLog("HandleCommand - Keybindings file not found: " + filename,
                 LOG_ERROR);
        MessageBoxA(hwnd,
                    ("Could not find keybindings file: " + filename).c_str(),
                    "Error", MB_ICONERROR);
      }
    }

    g_editor->OpenFile(StringToWString(docPath));
    break;
  }

  case IDM_FILE_CLOSE:
    if (g_editor->GetActiveBuffer()) {
      if (!PromptSaveBuffer(hwnd, g_editor->GetActiveBuffer()))
        break;
      g_editor->CloseBuffer(g_editor->GetActiveBufferIndex());
      UpdateMenu(hwnd);
    }
    break;
  case IDM_EDIT_UNDO:
    g_editor->Undo();
    break;
  case IDM_EDIT_REDO:
    g_editor->Redo();
    break;
  case IDM_EDIT_CUT:
    g_editor->Cut(hwnd);
    break;
  case IDM_EDIT_COPY:
    g_editor->Copy(hwnd);
    break;
  case IDM_EDIT_PASTE:
    g_editor->Paste(hwnd);
    break;
  case IDM_EDIT_SELECT_ALL: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      buf->SetSelectionAnchor(0);
      buf->SetCaretPos(buf->GetTotalLength());
    }
    break;
  }
  case IDM_EDIT_FIND: {
    ZeroMemory(&g_fr, sizeof(g_fr));
    g_fr.lStructSize = sizeof(g_fr);
    g_fr.hwndOwner = hwnd;
    g_fr.lpstrFindWhat = g_szFindWhat;
    g_fr.wFindWhatLen = 256;
    g_hDlgFind = FindTextW(&g_fr);
    break;
  }
  case IDM_EDIT_GOTO:
    Dialogs::ShowJumpToLineDialog(hwnd);
    break;
  case IDM_EDIT_TOGGLE_BOX: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      if (buf->GetSelectionMode() == SelectionMode::Box)
        buf->SetSelectionMode(SelectionMode::Normal);
      else
        buf->SetSelectionMode(SelectionMode::Box);
      InvalidateRect(hwnd, NULL, FALSE);
    }
    break;
  }
  case IDM_EDIT_FIND_IN_FILES:
  case IDM_EDIT_GREP:
    Dialogs::ShowFindInFilesDialog(hwnd);
    break;
  case IDM_EDIT_FIND_FILE:
    Dialogs::ShowFindFileDialog(hwnd);
    break;
  case IDM_EDIT_REPLACE: {
    ZeroMemory(&g_fr, sizeof(g_fr));
    g_fr.lStructSize = sizeof(g_fr);
    g_fr.hwndOwner = hwnd;
    g_fr.lpstrFindWhat = g_szFindWhat;
    g_fr.wFindWhatLen = 256;
    g_fr.lpstrReplaceWith = g_szReplaceWith;
    g_fr.wReplaceWithLen = 256;
    g_hDlgFind = ReplaceTextW(&g_fr);
    break;
  }
  case IDM_VIEW_TOGGLE_UI:
    MessageBox(hwnd, L"Toggle UI - Under Construction", L"Info", MB_OK);
    break;
  case IDM_VIEW_ZOOM_IN:
    g_renderer->ZoomIn();
    UpdateScrollbars(hwnd);
    break;
  case IDM_VIEW_ZOOM_OUT:
    g_renderer->ZoomOut();
    UpdateScrollbars(hwnd);
    break;
  case IDM_VIEW_ZOOM_RESET:
    g_renderer->ZoomReset();
    UpdateScrollbars(hwnd);
    break;
  case IDM_CONFIG_SETTINGS:
    Dialogs::ShowSettingsDialog(hwnd);
    break;
  case IDM_CONFIG_THEME: {
    Dialogs::ShowThemeManagerDialog(hwnd);
    UpdateMenu(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
    break;
  }
  case IDM_SHELL_ENC_UTF8:
    SettingsManager::Instance().SetShellEncoding(0);
    SettingsManager::Instance().Save();
    UpdateMenu(hwnd);
    break;
  case IDM_SHELL_ENC_SJIS:
    SettingsManager::Instance().SetShellEncoding(1);
    SettingsManager::Instance().Save();
    UpdateMenu(hwnd);
    break;
  case IDM_TOOLS_MACRO_GALLERY:
    Dialogs::ShowMacroGalleryDialog(hwnd);
    break;
  case IDM_HELP_MESSAGES: {
    Buffer *msgBuf = g_editor->GetBufferByName(L"*Messages*");
    if (msgBuf) {
      const auto &buffers = g_editor->GetBuffers();
      for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers[i].get() == msgBuf) {
          g_editor->SwitchToBuffer(i);
          UpdateMenu(hwnd);
          break;
        }
      }
    }
    break;
  }
  case IDM_TOOLS_CONSOLE:
  case IDM_TOOLS_SHELL_MODE: {
    g_editor->OpenShell(L"cmd");
    UpdateMenu(hwnd);
    break;
  }
  case IDM_TOOLS_DIRED: {
    // Launch Dired via plugin system
    for (size_t i = 0; i < g_plugins.size(); ++i) {
      if (g_plugins[i].name == L"Dired") { LaunchPlugin(hwnd, i); break; }
    }
    break;
  }
  case IDM_TOOLS_TERMINAL:
  case IDM_TOOLS_TERMINAL_CMD:
  case IDM_TOOLS_TERMINAL_BASH: {
    wchar_t modPath[MAX_PATH];
    GetModuleFileNameW(nullptr, modPath, MAX_PATH);
    std::wstring exeDir = modPath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\/"));
    std::wstring termExe = exeDir + L"\\plugins\\Terminal.exe";
    if (GetFileAttributesW(termExe.c_str()) == INVALID_FILE_ATTRIBUTES)
      termExe = exeDir + L"\\Terminal.exe";
    if (LOWORD(wParam) == IDM_TOOLS_TERMINAL) {
      if (g_editor) g_editor->LogMessage("[Launch] Terminal: powershell.exe");
      LaunchApp(hwnd, termExe, L"powershell.exe", L"powershell", TAB_TYPE_TERMINAL);
    } else if (LOWORD(wParam) == IDM_TOOLS_TERMINAL_CMD) {
      if (g_editor) g_editor->LogMessage("[Launch] Terminal: cmd.exe");
      LaunchApp(hwnd, termExe, L"cmd.exe", L"cmd", TAB_TYPE_TERMINAL);
    } else {
      std::wstring bashDir;
      std::wstring cmd = SettingsManager::Instance().GetBashCommand(&bashDir);
      if (!cmd.empty()) {
        if (!bashDir.empty()) SetCurrentDirectoryW(bashDir.c_str());
        if (g_editor) g_editor->LogMessage("[Launch] Terminal: " + WStringToString(cmd));
        LaunchApp(hwnd, termExe, cmd, L"bash", TAB_TYPE_TERMINAL);
      }
    }
    break;
  }
  case IDM_TOOLS_AI_ASSISTANT: {
    if (g_scriptEngine) {
      g_scriptEngine->Evaluate("aiComplete()");
    }
    break;
  }
  case IDM_TOOLS_AI_CONSOLE: {
    if (g_scriptEngine) {
      g_scriptEngine->Evaluate("openAiConsole()");
    }
    break;
  }
  case IDM_AI_MANAGER: {
    if (g_scriptEngine) {
      g_scriptEngine->Evaluate("open_ai_agent_manager()");
    }
    break;
  }
  case IDM_AI_SETUP_WIZARD: {
    if (g_scriptEngine) {
      g_scriptEngine->Evaluate("setup_wizard()");
    }
    break;
  }
  case IDM_TOOLS_AI_SET_KEY: {
    if (g_scriptEngine) {
      g_scriptEngine->Evaluate("set_ai_key()");
    }
    break;
  }
  case IDM_TOOLS_FILE_SEARCH:
  case IDM_TOOLS_CSV_EDITOR:
  case IDM_TOOLS_JY_EDITOR: {
    // Route through plugin system
    const wchar_t *names[] = { L"FastFileSearch", L"CSVEditor", L"JYEditor" };
    int idx = (LOWORD(wParam) == IDM_TOOLS_FILE_SEARCH) ? 0 :
              (LOWORD(wParam) == IDM_TOOLS_CSV_EDITOR) ? 1 : 2;
    for (size_t i = 0; i < g_plugins.size(); ++i) {
      if (g_plugins[i].name == names[idx]) { LaunchPlugin(hwnd, i); break; }
    }
    break;
  }

  case IDM_CLI_CONFIGURE: {
    ShowCliDialog(hwnd);
    UpdateMenu(hwnd);
    break;
  }
  case IDM_APPS_CONFIGURE: {
    ShowAppEntriesDialog(hwnd);
    UpdateMenu(hwnd);
    break;
  }
  case IDM_FILE_OPEN_FOLDER: {
    Buffer *buf = g_editor ? g_editor->GetActiveBuffer() : nullptr;
    if (buf && !buf->GetPath().empty()) {
      std::wstring filePath = buf->GetPath();
      std::wstring param = L"/select,\"" + filePath + L"\"";
      ShellExecuteW(NULL, L"open", L"explorer.exe", param.c_str(), NULL, SW_SHOW);
    } else {
      const auto &entries = SettingsManager::Instance().GetFolderEntries();
      if (!entries.empty()) {
        ShellExecuteW(NULL, L"open", L"explorer.exe", entries[0].path.c_str(), NULL, SW_SHOW);
      }
    }
    break;
  }
  case IDM_FOLDER_CONFIGURE: {
    ShowFolderEntriesDialog(hwnd);
    UpdateMenu(hwnd);
    break;
  }
  case IDM_DIRED_CONFIGURE: {
    Dialogs::ShowDiredPairsDialog(hwnd);
    UpdateMenu(hwnd);
    break;
  }
  case IDM_PROCESS_KILL: {
    KillActiveAppProcess(hwnd);
    break;
  }
  case IDM_PLUGINS_RESCAN: {
    ScanPlugins();
    UpdateMenu(hwnd);
    break;
  }
  case IDM_PLUGINS_CONFIGURE: {
    ShowPluginConfigDialog(hwnd);
    UpdateMenu(hwnd);
    break;
  }
  default:
    if (LOWORD(wParam) >= IDM_DIRED_START && LOWORD(wParam) < IDM_DIRED_START + 100) {
      size_t idx = LOWORD(wParam) - IDM_DIRED_START;
      const auto &pairs = SettingsManager::Instance().GetDiredPairs();
      if (idx < pairs.size()) {
        std::wstring diredPath;
        wchar_t modulePath[MAX_PATH];
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        diredPath = std::wstring(modulePath);
        size_t pos = diredPath.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
          std::wstring exeDir = diredPath.substr(0, pos + 1);
          diredPath = exeDir + L"plugins\\Dired.exe";
          if (GetFileAttributesW(diredPath.c_str()) == INVALID_FILE_ATTRIBUTES)
            diredPath = exeDir + L"Dired.exe";
        } else {
          diredPath = L"plugins\\Dired.exe";
        }

        std::wstring cmd = L"\"" + diredPath + L"\" " + GetLangArg() + L" \"" + pairs[idx].leftDir + L"\"";
        if (!pairs[idx].rightDir.empty()) cmd += L" \"" + pairs[idx].rightDir + L"\"";

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
      }
    } else if (LOWORD(wParam) >= IDM_THEME_START && LOWORD(wParam) < IDM_THEME_START + 100) {
      size_t idx = LOWORD(wParam) - IDM_THEME_START;
      const auto &themes = SettingsManager::Instance().GetThemes();
      if (idx < themes.size()) {
        auto &t = themes[idx];
        Theme theme;
        theme.name = t.name;
        auto HexToColor = [](const std::wstring &hex) -> D2D1_COLOR_F {
          unsigned int r = 0, g = 0, b = 0, a = 255;
          if (hex.length() >= 8) {
            swscanf_s(hex.c_str(), L"%02x%02x%02x%02x", &r, &g, &b, &a);
          } else if (hex.length() >= 6) {
            swscanf_s(hex.c_str(), L"%02x%02x%02x", &r, &g, &b);
          }
          return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
        };
        theme.background = HexToColor(t.background);
        theme.foreground = HexToColor(t.foreground);
        theme.caret = HexToColor(t.caret);
        theme.selection = HexToColor(t.selection);
        theme.lineNumbers = HexToColor(t.lineNumbers);
        theme.keyword = HexToColor(t.keyword);
        theme.string = HexToColor(t.string);
        theme.number = HexToColor(t.number);
        theme.comment = HexToColor(t.comment);
        theme.function = HexToColor(t.function);
        g_renderer->SetTheme(theme);
        SettingsManager::Instance().SetActiveThemeName(t.name);
        SettingsManager::Instance().Save();
        UpdateMenu(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
      }
    } else if (LOWORD(wParam) >= IDM_CLI_START && LOWORD(wParam) < IDM_CLI_START + 100) {
      size_t idx = LOWORD(wParam) - IDM_CLI_START;
      const auto &entries = SettingsManager::Instance().GetCliEntries();
      if (idx < entries.size()) {
        std::wstring cliDir = entries[idx].folder;
        if (!cliDir.empty()) SetCurrentDirectoryW(cliDir.c_str());
        int enc = entries[idx].encoding;
        SettingsManager::Instance().SetShellEncoding(enc);
        std::wstring shellArgs;
        int st = entries[idx].shellType;
        if (st < 0 || st > 2) st = 2;
        if (st == 0) {
          wchar_t comspec[MAX_PATH];
          GetEnvironmentVariableW(L"COMSPEC", comspec, MAX_PATH);
          std::wstring chcpCmd = (enc == 1) ? L"chcp 932 > nul & " : L"chcp 65001 > nul & ";
          shellArgs = std::wstring(comspec) + L" /k \"" + chcpCmd + entries[idx].command + L"\"";
        } else if (st == 1) {
          std::wstring encSetup = (enc == 1)
            ? L"[Console]::OutputEncoding=[System.Text.Encoding]::GetEncoding(932);[Console]::InputEncoding=[System.Text.Encoding]::GetEncoding(932); "
            : L"[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;[Console]::InputEncoding=[System.Text.Encoding]::UTF8; ";
          shellArgs = L"powershell.exe -NoExit -Command \"" + encSetup + entries[idx].command + L"\"";
        } else {
          std::wstring bashDir;
          std::wstring bashCmd = SettingsManager::Instance().GetBashCommand(&bashDir);
          if (bashCmd.empty()) break;
          std::wstring localePrefix = (enc == 1) ? L"export LANG=ja_JP.SJIS; " : L"export LANG=en_US.UTF-8; ";
          shellArgs = bashCmd + L" -c \"" + localePrefix + entries[idx].command + L"\"";
        }
        std::wstring label = entries[idx].label.empty()
          ? cliDir.substr(cliDir.find_last_of(L"\\/") + 1)
          : entries[idx].label;
        if (label.empty()) label = L"CLI";
        wchar_t modPath[MAX_PATH];
        GetModuleFileNameW(nullptr, modPath, MAX_PATH);
        std::wstring exeDir = modPath;
        exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\/"));
        std::wstring termExe = exeDir + L"\\plugins\\Terminal.exe";
        if (GetFileAttributesW(termExe.c_str()) == INVALID_FILE_ATTRIBUTES)
          termExe = exeDir + L"\\Terminal.exe";
        if (g_editor) g_editor->LogMessage("[Launch] CLI Terminal: " + WStringToString(shellArgs));
        LaunchApp(hwnd, termExe, shellArgs, label, TAB_TYPE_TERMINAL, entries[idx].iconIndex);
      }
    } else if (LOWORD(wParam) >= IDM_APPS_START && LOWORD(wParam) < IDM_APPS_START + 100) {
      size_t idx = LOWORD(wParam) - IDM_APPS_START;
      const auto &entries = SettingsManager::Instance().GetAppEntries();
      if (idx < entries.size()) {
        std::wstring path = entries[idx].path;
        std::wstring label = entries[idx].label.empty()
          ? path.substr(path.find_last_of(L"\\/") + 1)
          : entries[idx].label;
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = path.c_str();
        sei.nShow = SW_SHOW;
        if (ShellExecuteExW(&sei)) {
          AppTabInfo tab;
          tab.label = label;
          tab.type = 0;
          tab.hProcess = sei.hProcess;
          g_appTabs.push_back(tab);
          if (g_activeAppTab < 0) g_activeAppTab = (int)g_appTabs.size() - 1;
          UpdateMenu(hwnd);
        }
      }
    } else if (LOWORD(wParam) >= IDM_FOLDERS_START && LOWORD(wParam) < IDM_FOLDERS_START + 100) {
      size_t idx = LOWORD(wParam) - IDM_FOLDERS_START;
      const auto &entries = SettingsManager::Instance().GetFolderEntries();
      if (idx < entries.size()) {
        ShellExecuteW(NULL, L"open", L"explorer.exe", entries[idx].path.c_str(), NULL, SW_SHOW);
      }
    } else if (LOWORD(wParam) >= IDM_BUFFERS_START &&
               LOWORD(wParam) < IDM_BUFFERS_START + 100) {
      for (auto &t : g_appTabs) if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
      g_activeAppTab = -1;
      g_editor->SwitchToBuffer(LOWORD(wParam) - IDM_BUFFERS_START);
      SetFocus(hwnd);
      UpdateMenu(hwnd);
    } else if (LOWORD(wParam) >= IDM_BUFFERS_START + 200 &&
               LOWORD(wParam) < IDM_BUFFERS_START + 300) {
      int appIdx = (LOWORD(wParam) - IDM_BUFFERS_START - 200);
      if (appIdx >= 0 && appIdx < static_cast<int>(g_appTabs.size())) {
        for (auto &t : g_appTabs) if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
        g_activeAppTab = appIdx;
        ShowScrollBar(hwnd, SB_BOTH, FALSE);
        if (g_appTabs[appIdx].hwnd) {
          ShowWindow(g_appTabs[appIdx].hwnd, SW_SHOW);
          PostMessage(hwnd, WM_DEFERRED_FOCUS, (WPARAM)g_appTabs[appIdx].hwnd, 0);
        }
        UpdateMenu(hwnd);
      }
    } else if (LOWORD(wParam) >= IDM_PROCESS_KILL_START &&
               LOWORD(wParam) < IDM_PROCESS_KILL_START + 100) {
      size_t idx = LOWORD(wParam) - IDM_PROCESS_KILL_START;
      KillAppProcessByIndex(hwnd, idx);
    } else if (LOWORD(wParam) >= IDM_PLUGINS_START &&
               LOWORD(wParam) < IDM_PLUGINS_START + 200) {
      size_t idx = LOWORD(wParam) - IDM_PLUGINS_START;
      LaunchPlugin(hwnd, idx);
    } else if (LOWORD(wParam) >= IDM_RECENT_START &&
               LOWORD(wParam) < IDM_RECENT_START + 10) {
      size_t index = LOWORD(wParam) - IDM_RECENT_START;
      const auto &recent = SettingsManager::Instance().GetRecentFiles();
      if (index < recent.size()) {
        g_editor->OpenFile(recent[index]);
        SettingsManager::Instance().AddRecentFile(recent[index]);
        UpdateMenu(hwnd);
      }
    }
    break;
  }
  InvalidateRect(hwnd, NULL, FALSE);
  return 0;
}
