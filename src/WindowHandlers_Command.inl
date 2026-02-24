// =============================================================================
// WindowHandlers_Command.inl
// Handle WM_COMMAND (menu items, buttons, etc)
// Included by main.cpp
// =============================================================================

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
  case IDM_TOOLS_CONSOLE: {
    g_editor->OpenShell(L"cmd");
    UpdateMenu(hwnd);
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

  default:
    if (LOWORD(wParam) >= IDM_BUFFERS_START &&
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
