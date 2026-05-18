struct EnumData {
  DWORD processId;
  HWND hwnd;
};

struct TerminalThreadParams {
  HWND hwnd;
  int termIdx;
  std::wstring shell;
  std::wstring uniqueTitle;
  HWND foundHwnd;
};

static BOOL CALLBACK EnumConsoleWindowsProc(HWND hwnd, LPARAM lParam) {
  TerminalThreadParams *params = (TerminalThreadParams *)lParam;
  wchar_t className[256];
  GetClassNameW(hwnd, className, 256);
  if (wcscmp(className, L"ConsoleWindowClass") == 0) {
    wchar_t windowTitle[512];
    GetWindowTextW(hwnd, windowTitle, 512);
    if (wcsstr(windowTitle, params->uniqueTitle.c_str()) != nullptr) {
      params->foundHwnd = hwnd;
      return FALSE; // found, stop
    }
  }
  return TRUE;
}

static DWORD WINAPI TerminalEmbedThread(LPVOID lpParam) {
  TerminalThreadParams *params = (TerminalThreadParams *)lpParam;
  HWND hwnd = params->hwnd;
  int termIdx = params->termIdx;
  std::wstring shell = params->shell;
  std::wstring uniqueTitle = params->uniqueTitle;

  STARTUPINFOW si = { sizeof(si) };
  si.lpTitle = &uniqueTitle[0];
  PROCESS_INFORMATION pi = { 0 };
  std::wstring cmd = L"conhost.exe " + shell;
  if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
    HWND foundHwnd = NULL;
    for (int i = 0; i < 50; ++i) {
      Sleep(100);
      params->foundHwnd = NULL;
      EnumWindows(EnumConsoleWindowsProc, (LPARAM)params);
      if (params->foundHwnd != NULL) {
        foundHwnd = params->foundHwnd;
        break;
      }
    }
    if (foundHwnd) {
      PostMessageW(hwnd, WM_EMBED_TERMINAL, (WPARAM)foundHwnd, (LPARAM)termIdx);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  delete params;
  return 0;
}

void CreateNewTerminal(HWND hwnd, const std::wstring &shell, const std::wstring &label) {
  TerminalTabInfo tab;
  tab.view = nullptr;
  tab.hwnd = CreateWindowExW(0, L"STATIC", L"Starting Terminal...",
                             WS_CHILD | WS_VISIBLE | SS_CENTER,
                             0, 0, 100, 100, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
  tab.shell = shell;
  if (label.empty()) {
    int count = static_cast<int>(g_terminalTabs.size());
    tab.label = (count == 0) ? L"Terminal" : (L"Terminal " + std::to_wstring(count + 1));
  } else {
    tab.label = label;
  }
  g_terminalTabs.push_back(std::move(tab));
  UpdateMenu(hwnd);

  int termIdx = static_cast<int>(g_terminalTabs.size()) - 1;
  size_t bufCount = g_editor->GetBuffers().size();
  int tabIndex = static_cast<int>(bufCount) + termIdx;
  g_suppressTabChange = true;
  TabCtrl_SetCurSel(g_tabHwnd, tabIndex);
  g_suppressTabChange = false;

  // Position the terminal placeholder to fill content area
  RECT rc;
  GetClientRect(hwnd, &rc);
  int treeWidth = g_treeVisible ? 200 : 0;
  int tabHeight = 25;
  int statusHeight = 0;
  if (IsWindowVisible(g_statusHwnd)) {
    RECT rcStatus;
    GetWindowRect(g_statusHwnd, &rcStatus);
    statusHeight = rcStatus.bottom - rcStatus.top;
  }
  int minibufferHeight = g_minibufferVisible ? 24 : 0;
  int safetyMargin = 50;
  int contentTop = tabHeight;
  int contentHeight = rc.bottom - tabHeight - statusHeight - minibufferHeight - safetyMargin;
  int contentWidth = rc.right - treeWidth;
  MoveWindow(g_terminalTabs[termIdx].hwnd, treeWidth, contentTop,
             contentWidth, contentHeight + safetyMargin, TRUE);

  for (size_t i = 0; i < g_terminalTabs.size(); ++i) {
    if (g_terminalTabs[i].hwnd)
      ShowWindow(g_terminalTabs[i].hwnd, static_cast<int>(i) == termIdx ? SW_SHOW : SW_HIDE);
  }
  ShowScrollBar(hwnd, SB_BOTH, FALSE);
  g_activeTerminalTab = termIdx;
  SetFocus(g_terminalTabs[termIdx].hwnd);
  InvalidateRect(hwnd, NULL, FALSE);

  TerminalThreadParams *params = new TerminalThreadParams();
  params->hwnd = hwnd;
  params->termIdx = termIdx;
  params->shell = shell;
  params->uniqueTitle = L"EcodeTerminalTab_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(termIdx);
  params->foundHwnd = NULL;
  CreateThread(NULL, 0, TerminalEmbedThread, params, 0, NULL);
}



static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  EnumData *data = (EnumData *)lParam;
  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  if (processId == data->processId) {
    if (IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == NULL) {
      data->hwnd = hwnd;
      return FALSE; // Stop enumerating
    }
  }
  return TRUE;
}

struct SearchThreadParams {
  HWND hwnd;
  int appIdx;
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
  std::wstring cmd = L"\"" + exePath + L"\"";
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
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  return 0;
}

void OpenFastFileSearch(HWND hwnd) {
  AppTabInfo tab;
  tab.label = L"File Search";
  tab.type = 2;
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
  g_activeTerminalTab = -1;

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
  for (auto &t : g_terminalTabs) {
    if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
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
  std::wstring cmd = L"\"" + exePath + L"\"";
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
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  return 0;
}

void OpenCSVEditor(HWND hwnd) {
  AppTabInfo tab;
  tab.label = L"CSV Editor";
  tab.type = 3;
  tab.hwnd = CreateWindowExW(0, L"STATIC", L"Starting CSV Editor...",
                             WS_CHILD | WS_VISIBLE | SS_CENTER,
                             0, 0, 100, 100, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
  tab.data = nullptr;
  g_appTabs.push_back(std::move(tab));
  int appIdx = static_cast<int>(g_appTabs.size()) - 1;
  g_activeAppTab = appIdx;
  g_activeTerminalTab = -1;

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
  for (auto &t : g_terminalTabs) {
    if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
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
  std::wstring cmd = L"\"" + exePath + L"\"";
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
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  return 0;
}

void OpenJYEditor(HWND hwnd) {
  AppTabInfo tab;
  tab.label = L"JY Editor";
  tab.type = 4;
  tab.hwnd = CreateWindowExW(0, L"STATIC", L"Starting JY Editor...",
                             WS_CHILD | WS_VISIBLE | SS_CENTER,
                             0, 0, 100, 100, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
  tab.data = nullptr;
  g_appTabs.push_back(std::move(tab));
  int appIdx = static_cast<int>(g_appTabs.size()) - 1;
  g_activeAppTab = appIdx;
  g_activeTerminalTab = -1;

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
  for (auto &t : g_terminalTabs) {
    if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
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
  if (pos != std::wstring::npos)
    diredPath = diredPath.substr(0, pos + 1) + L"Dired.exe";
  else
    diredPath = L"Dired.exe";

  std::wstring cmd = L"\"" + diredPath + L"\" \"" + curDir + L"\"";
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
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  return 0;
}

void OpenDired(HWND hwnd) {
  AppTabInfo tab;
  tab.label = L"Dired";
  tab.type = 5;
  tab.hwnd = CreateWindowExW(0, L"STATIC", L"Starting Dired...",
                             WS_CHILD | WS_VISIBLE | SS_CENTER,
                             0, 0, 100, 100, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
  tab.data = nullptr;
  g_appTabs.push_back(std::move(tab));
  int appIdx = static_cast<int>(g_appTabs.size()) - 1;
  g_activeAppTab = appIdx;
  g_activeTerminalTab = -1;

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
  for (auto &t : g_terminalTabs) {
    if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
  }

  UpdateMenu(hwnd);
  InvalidateRect(hwnd, NULL, FALSE);

  DiredThreadParams *params = new DiredThreadParams();
  params->hwnd = hwnd;
  params->appIdx = appIdx;
  CreateThread(nullptr, 0, DiredThread, params, 0, nullptr);
}

static LRESULT HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {
  switch (LOWORD(wParam)) {
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
        if (!path.empty())
          buf->SaveFile(path);
      } else {
        buf->SaveFile(buf->GetPath());
      }
    }
    break;
  }
  case IDM_FILE_SAVE_AS: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      std::wstring path = Dialogs::SaveFileDialog(hwnd);
      if (!path.empty()) {
        buf->SaveFile(path);
        SettingsManager::Instance().AddRecentFile(path);
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
    OpenDired(hwnd);
    break;
  }
  case IDM_TOOLS_TERMINAL: {
    CreateNewTerminal(hwnd, L"powershell.exe");
    break;
  }
  case IDM_TOOLS_TERMINAL_CMD: {
    CreateNewTerminal(hwnd, L"cmd.exe");
    break;
  }
  case IDM_TOOLS_TERMINAL_BASH: {
    std::wstring bashDir;
    std::wstring cmd = SettingsManager::Instance().GetBashCommand(&bashDir);
    if (!cmd.empty()) {
      if (!bashDir.empty()) SetCurrentDirectoryW(bashDir.c_str());
      CreateNewTerminal(hwnd, cmd);
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
  case IDM_TOOLS_FILE_SEARCH: {
    OpenFastFileSearch(hwnd);
    break;
  }
  case IDM_TOOLS_CSV_EDITOR: {
    OpenCSVEditor(hwnd);
    break;
  }
  case IDM_TOOLS_JY_EDITOR: {
    OpenJYEditor(hwnd);
    break;
  }

  case IDM_CLI_CONFIGURE: {
    ShowCliDialog(hwnd);
    UpdateMenu(hwnd);
    break;
  }
  case IDM_DIRED_CONFIGURE: {
    Dialogs::ShowDiredPairsDialog(hwnd);
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
        if (pos != std::wstring::npos) diredPath = diredPath.substr(0, pos + 1);
        diredPath += L"Dired.exe";

        std::wstring cmd = L"\"" + diredPath + L"\" \"" + pairs[idx].leftDir + L"\"";
        if (!pairs[idx].rightDir.empty()) cmd += L" \"" + pairs[idx].rightDir + L"\"";

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
      }
    } else
    if (LOWORD(wParam) >= IDM_TERMINALS_START && LOWORD(wParam) < IDM_TERMINALS_START + 100) {
      int termIdx = LOWORD(wParam) - IDM_TERMINALS_START;
      if (termIdx >= 0 && termIdx < static_cast<int>(g_terminalTabs.size())) {
        size_t bufCount = g_editor->GetBuffers().size();
        int tabIndex = static_cast<int>(bufCount) + termIdx;
        g_suppressTabChange = true;
        TabCtrl_SetCurSel(g_tabHwnd, tabIndex);
        g_suppressTabChange = false;
        for (size_t i = 0; i < g_terminalTabs.size(); ++i) {
          if (g_terminalTabs[i].hwnd)
            ShowWindow(g_terminalTabs[i].hwnd, static_cast<int>(i) == termIdx ? SW_SHOW : SW_HIDE);
        }
        ShowScrollBar(hwnd, SB_BOTH, FALSE);
        g_activeTerminalTab = termIdx;
        if (g_terminalTabs[termIdx].hwnd) {
          SetFocus(g_terminalTabs[termIdx].hwnd);
        }
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
        std::wstring bashDir;
        std::wstring bashCmd = SettingsManager::Instance().GetBashCommand(&bashDir);
        if (!bashCmd.empty()) {
          std::wstring cliDir = entries[idx].folder;
          if (!cliDir.empty()) SetCurrentDirectoryW(cliDir.c_str());
          std::wstring cmdLine = bashCmd + L" -c \"" + entries[idx].command + L"; exec bash --login -i\"";
          std::wstring label = cliDir.substr(cliDir.find_last_of(L"\\/") + 1);
          if (label.empty()) label = L"CLI";
          CreateNewTerminal(hwnd, cmdLine, label);
        }
      }
    } else if (LOWORD(wParam) >= IDM_BUFFERS_START &&
               LOWORD(wParam) < IDM_BUFFERS_START + 100) {
      g_editor->SwitchToBuffer(LOWORD(wParam) - IDM_BUFFERS_START);
      UpdateMenu(hwnd);
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
