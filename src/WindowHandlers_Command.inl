// =============================================================================
// WindowHandlers_Command.inl
// Handle WM_COMMAND (menu items, buttons, etc)
// Included by main.cpp
// =============================================================================

void CreateNewTerminal(HWND hwnd, const std::wstring &shell, const std::wstring &label) {
  TerminalTabInfo tab;
  tab.view = new TerminalView();
  tab.hwnd = tab.view->Create(hwnd);
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

  // Position the terminal view to fill content area
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
  g_terminalTabs[termIdx].view->StartSession(g_terminalTabs[termIdx].shell, {});
  InvalidateRect(hwnd, NULL, FALSE);
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

  case IDM_CLI_CONFIGURE: {
    ShowCliDialog(hwnd);
    UpdateMenu(hwnd);
    break;
  }
  default:
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
          if (g_terminalTabs[termIdx].view && !g_terminalTabs[termIdx].view->IsStarted())
            g_terminalTabs[termIdx].view->StartSession(g_terminalTabs[termIdx].shell, {});
        }
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
