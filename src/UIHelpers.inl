// =============================================================================
// UIHelpers.inl
// Scrollbars, tabs, menu updates, and save prompts
// Included by main.cpp
// =============================================================================

bool PromptSaveBuffer(HWND hwnd, Buffer *buf) {
  if (!buf->IsDirty() || buf->IsScratch() || buf->IsShell())
    return true;

  auto res = Dialogs::ShowSaveConfirmationDialog(hwnd, buf->GetPath());
  if (res == Dialogs::ConfirmationResult::Cancel)
    return false;
  if (res == Dialogs::ConfirmationResult::Discard)
    return true;

  if (buf->GetPath().empty()) {
    std::wstring path = Dialogs::SaveFileDialog(hwnd);
    if (path.empty())
      return false;
    if (buf->SaveFile(path)) {
      SettingsManager::Instance().AddRecentFile(path);
      return true;
    }
    return false;
  } else {
    if (buf->SaveFile(buf->GetPath()))
      return true;
    return false;
  }
}

void UpdateScrollbars(HWND hwnd) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (!buf)
    return;

  RECT rc;
  GetClientRect(hwnd, &rc);
  float lineHeight = g_renderer->GetLineHeight();

  int statusHeight = 0;
  if (IsWindowVisible(g_statusHwnd)) {
    RECT rcStatus;
    GetWindowRect(g_statusHwnd, &rcStatus);
    statusHeight = rcStatus.bottom - rcStatus.top;
  }
  int tabHeight = 25;

  int availableHeight = rc.bottom - tabHeight - statusHeight;
  int visibleLines = (lineHeight > 0) ? (int)(availableHeight / lineHeight) : 1;
  int totalLines = (int)buf->GetVisibleLineCount();

  SCROLLINFO si = {0};
  si.cbSize = sizeof(si);
  si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
  si.nMin = 0;
  si.nMax = totalLines - 1;
  si.nPage = visibleLines;
  si.nPos = (int)buf->GetScrollLine();
  SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

  size_t scrollLine = buf->GetScrollLine();
  size_t viewportLineCount = g_renderer->CalculateVisibleLineCount();
  size_t actualLines = 0;
  std::string viewportContent =
      buf->GetViewportText(scrollLine, viewportLineCount, actualLines);

  float textWidth = g_renderer->GetTextWidth(viewportContent);
  float gutterWidth = 50.0f;
  int totalWidth = (int)(textWidth + gutterWidth + 20.0f);
  int visibleWidth = rc.right;

  si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
  si.nMin = 0;
  si.nMax = totalWidth;
  si.nPage = visibleWidth;
  si.nPos = (int)buf->GetScrollX();
  SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
}

void EnsureCaretVisible(HWND hwnd) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (!buf)
    return;

  size_t caretLine = buf->GetLineAtOffset(buf->GetCaretPos());
  size_t scrollLine = buf->GetScrollLine();

  RECT rc;
  GetClientRect(hwnd, &rc);
  float lineHeight = g_renderer->GetLineHeight();

  int statusHeight = 0;
  if (IsWindowVisible(g_statusHwnd)) {
    RECT rcStatus;
    GetWindowRect(g_statusHwnd, &rcStatus);
    statusHeight = rcStatus.bottom - rcStatus.top;
  }
  int tabHeight = 25;

  int availableHeight = rc.bottom - tabHeight - statusHeight;
  int visibleLines = (lineHeight > 0) ? (int)(availableHeight / lineHeight) : 1;

  if (caretLine < scrollLine) {
    buf->SetScrollLine(caretLine);
    UpdateScrollbars(hwnd);
  } else if (caretLine >= scrollLine + visibleLines) {
    buf->SetScrollLine(caretLine - visibleLines + 1);
    UpdateScrollbars(hwnd);
  }

  g_renderer->SetCaretVisible(true);
  SetTimer(hwnd, 1, 500, NULL);
}

void UpdateTabs(HWND hwnd) {
  if (!g_tabHwnd)
    return;
  TabCtrl_DeleteAllItems(g_tabHwnd);

  const auto &buffers = g_editor->GetBuffers();
  for (size_t i = 0; i < buffers.size(); ++i) {
    std::wstring name = buffers[i]->GetPath();
    if (name.empty()) {
      name = buffers[i]->IsScratch() ? L"Scratch" : L"Untitled";
    } else {
      size_t pos = name.find_last_of(L"\\/");
      if (pos != std::wstring::npos)
        name = name.substr(pos + 1);
    }

    TCITEMW tie = {0};
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)name.c_str();
    TabCtrl_InsertItem(g_tabHwnd, static_cast<int>(i), &tie);
  }
  TabCtrl_SetCurSel(g_tabHwnd,
                    static_cast<int>(g_editor->GetActiveBufferIndex()));
}

void UpdateMenu(HWND hwnd) {
  HMENU hMenu = CreateMenu();

  // File Menu
  HMENU hFile = CreatePopupMenu();
  AppendMenu(
      hFile, MF_STRING, IDM_FILE_NEW,
      L"New\tCtrl+N"); // Keep Ctrl+N for New? Emacs is usually C-x C-f for new
                       // file too if it doesn't exist. Let's keep Ctrl+N as
                       // alternative or just C-x C-f. User said "file operation
                       // keybindings use emacs like key bindings". Let's stick
                       // to the plan: Open, Save, Save As, Close, Exit.
  // Actually, I'll keep Ctrl+N for New as it wasn't explicitly mapped to
  // something else in my plan, but maybe C-x C-f does it all. The plan said:
  // Open: C-x C-f, Save: C-x C-s, Save As: C-x C-w, Close: C-x k, Exit: C-x
  // C-c.
  AppendMenu(hFile, MF_STRING, IDM_FILE_NEW, L"New\tCtrl+N");
  AppendMenu(hFile, MF_STRING, IDM_FILE_OPEN, L"Open\tC-x C-f");
  AppendMenu(hFile, MF_STRING, IDM_FILE_SAVE, L"Save\tC-x C-s");
  AppendMenu(hFile, MF_STRING, IDM_FILE_SAVE_AS, L"Save As\tC-x C-w");
  AppendMenu(hFile, MF_STRING, IDM_FILE_CLOSE, L"Close\tC-x k");

  HMENU hRecent = CreatePopupMenu();
  const auto &recent = SettingsManager::Instance().GetRecentFiles();
  if (recent.empty()) {
    AppendMenu(hRecent, MF_GRAYED, 0, L10N("menu_file_recent_empty"));
  } else {
    for (size_t i = 0; i < recent.size(); ++i) {
      AppendMenu(hRecent, MF_STRING, IDM_RECENT_START + i, recent[i].c_str());
    }
  }
  AppendMenu(hFile, MF_POPUP, (UINT_PTR)hRecent, L10N("menu_file_recent"));
  AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
  AppendMenu(hFile, MF_STRING, IDM_FILE_SCRATCH, L10N("menu_file_scratch"));
  AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
  AppendMenu(hFile, MF_STRING, IDM_FILE_EXIT, L"Exit\tC-x C-c");

  // Edit Menu
  HMENU hEdit = CreatePopupMenu();

  AppendMenu(hEdit, MF_STRING, IDM_EDIT_UNDO, L"Undo\tC-z");
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_REDO, L"Redo");
  AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_CUT, L"Cut\tC-w");
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_COPY, L"Copy\tM-w");
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_PASTE, L"Paste\tC-y");
  AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_SELECT_ALL, L"Select All\tC-a");
  AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_FIND, L"Find\tC-s");
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_REPLACE, L"Replace");
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_FIND_IN_FILES,
             L"Find in Files...\tC-S-f");
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_GOTO, L"Go to Line...\tAlt+G");
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_TOGGLE_BOX,
             L"Box Selection Mode\tAlt+Shift+Drag");

  // ... (View, Config, Tools, Language, Buffers, Help omitted for brevity, but
  // they'll be in the actual file) Actually, I should include everything since
  // this is write_to_file.

  // View Menu
  HMENU hView = CreatePopupMenu();
  AppendMenu(hView, MF_STRING, IDM_VIEW_TOGGLE_UI, L"Toggle UI\tF11");
  AppendMenu(hView, MF_SEPARATOR, 0, NULL);
  AppendMenu(hView, MF_STRING, IDM_VIEW_ZOOM_IN, L"Zoom In\tCtrl++");
  AppendMenu(hView, MF_STRING, IDM_VIEW_ZOOM_OUT, L"Zoom Out\tCtrl+-");
  AppendMenu(hView, MF_STRING, IDM_VIEW_ZOOM_RESET,
             L10N("menu_view_zoom_reset"));

  // Config Menu
  HMENU hConfig = CreatePopupMenu();
  AppendMenu(hConfig, MF_STRING, IDM_CONFIG_SETTINGS,
             L10N("menu_config_settings"));
  AppendMenu(hConfig, MF_STRING, IDM_CONFIG_THEME, L10N("menu_config_theme"));
  AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
  AppendMenu(hConfig, MF_STRING, IDM_CONFIG_EDIT_INIT,
             L10N("menu_config_edit_init"));
  AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);

  HMENU hEnc = CreatePopupMenu();
  int currentEnc = SettingsManager::Instance().GetShellEncoding();
  AppendMenu(hEnc, MF_STRING | (currentEnc == 0 ? MF_CHECKED : 0),
             IDM_SHELL_ENC_UTF8, L"UTF-8");
  AppendMenu(hEnc, MF_STRING | (currentEnc == 1 ? MF_CHECKED : 0),
             IDM_SHELL_ENC_SJIS, L"Shift-JIS");
  AppendMenu(hConfig, MF_POPUP, (UINT_PTR)hEnc, L"Shell Encoding");

  // Tools Menu
  HMENU hTools = CreatePopupMenu();
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_RUN_MACRO,
             L10N("menu_tools_run_macro"));
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_CONSOLE, L10N("menu_tools_console"));
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_MACRO_GALLERY,
             L10N("menu_tools_macro_gallery"));
  AppendMenu(hTools, MF_SEPARATOR, 0, NULL);
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_AI_ASSISTANT, L"AI Assistant\tAlt+A");
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_AI_CONSOLE, L"AI Console\tAlt+I");
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_AI_SET_KEY, L"Set AI API Key...");

  // Language Menu
  HMENU hLang = CreatePopupMenu();
  AppendMenu(hLang, MF_STRING, IDM_LANG_EN, L10N("menu_language_en"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_JP, L10N("menu_language_jp"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_ES, L10N("menu_language_es"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_FR, L10N("menu_language_fr"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_DE, L10N("menu_language_de"));

  // Buffers Menu
  HMENU hBuffers = CreatePopupMenu();
  const auto &buffers = g_editor->GetBuffers();
  for (size_t i = 0; i < buffers.size(); ++i) {
    std::wstring name = buffers[i]->GetPath();
    if (name.empty())
      name = buffers[i]->IsScratch() ? L"Scratch" : L"Untitled";
    else {
      size_t pos = name.find_last_of(L"\\/");
      if (pos != std::wstring::npos)
        name = name.substr(pos + 1);
    }
    UINT flags = MF_STRING;
    if (i == g_editor->GetActiveBufferIndex())
      flags |= MF_CHECKED;
    AppendMenu(hBuffers, flags, IDM_BUFFERS_START + i, name.c_str());
  }

  // Help Menu
  HMENU hHelp = CreatePopupMenu();
  AppendMenu(hHelp, MF_STRING, IDM_HELP_DOC, L10N("menu_help_doc"));
  AppendMenu(hHelp, MF_STRING, IDM_HELP_KEYBINDINGS,
             L10N("menu_help_keybindings"));
  AppendMenu(hHelp, MF_STRING, IDM_HELP_ABOUT, L10N("menu_help_about"));
  AppendMenu(hHelp, MF_STRING, IDM_HELP_MESSAGES, L"Show Messages");

  // AI Menu (New)
  HMENU hAi = CreatePopupMenu();
  AppendMenu(hAi, MF_STRING, IDM_AI_MANAGER, L"AI Agent Manager");
  AppendMenu(hAi, MF_STRING, IDM_AI_SETUP_WIZARD, L"AI Setup Wizard");
  AppendMenu(hAi, MF_SEPARATOR, 0, NULL);
  AppendMenu(hAi, MF_STRING, IDM_TOOLS_AI_ASSISTANT,
             L"Contextual AI Assistant\tAlt+A");
  AppendMenu(hAi, MF_STRING, IDM_TOOLS_AI_CONSOLE, L"AI Agents Console\tAlt+I");
  AppendMenu(hAi, MF_STRING, IDM_TOOLS_AI_SET_KEY, L"Configure Server Keys...");

  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFile, L10N("menu_file"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEdit, L10N("menu_edit"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hView, L10N("menu_view"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hConfig, L10N("menu_config"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hTools, L10N("menu_tools"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hAi, L"AI");
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hLang, L10N("menu_language"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hBuffers, L10N("menu_buffers"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelp, L10N("menu_help"));

  SetMenu(hwnd, hMenu);
  SetWindowText(hwnd, L10N("title"));
  UpdateScrollbars(hwnd);
  UpdateTabs(hwnd);
}
