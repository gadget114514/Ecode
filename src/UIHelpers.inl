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
      buf->SetPath(path);
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
  // Suppress redraw to avoid flicker during tab rebuild
  SendMessage(g_tabHwnd, WM_SETREDRAW, FALSE, 0);
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
  // Append all app tabs after buffer tabs
  int appStart = static_cast<int>(buffers.size());
  for (size_t i = 0; i < g_appTabs.size(); ++i) {
    TCITEMW tci = {0};
    tci.mask = TCIF_TEXT;
    tci.pszText = (LPWSTR)g_appTabs[i].label.c_str();
    TabCtrl_InsertItem(g_tabHwnd, appStart + static_cast<int>(i), &tci);
  }

  // Restore selection
  int curSel = (g_activeAppTab >= 0)
    ? appStart + g_activeAppTab
    : static_cast<int>(g_editor->GetActiveBufferIndex());
  TabCtrl_SetCurSel(g_tabHwnd, curSel);
  // Re-enable redraw
  SendMessage(g_tabHwnd, WM_SETREDRAW, TRUE, 0);
  InvalidateRect(g_tabHwnd, NULL, TRUE);
}

void UpdateMenu(HWND hwnd) {
  HMENU hMenu = CreateMenu();

  // File Menu
  HMENU hFile = CreatePopupMenu();
  {
    HMENU hNew = CreatePopupMenu();
    AppendMenu(hNew, MF_STRING, IDM_FILE_NEW, L"New File\tCtrl+N");
    AppendMenu(hNew, MF_SEPARATOR, 0, NULL);
    AppendMenu(hNew, MF_STRING, IDM_FILE_SCRATCH, L10N("menu_file_scratch"));
    AppendMenu(hFile, MF_POPUP, (UINT_PTR)hNew, L10N("menu_file_new"));
  }
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
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_GREP,
             L"Grep...");
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_FIND_FILE,
             L"Find File...\tC-S-F");
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_GOTO, L"Go to Line...\tAlt+G");
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_TOGGLE_BOX,
             L"Box Selection Mode\tAlt+Shift+Drag");

  // ... (View, Config, Tools, Language, Buffers, Help omitted for brevity, but
  // they'll be in the actual file) Actually, I should include everything since
  // this is write_to_file.

  // View Menu
  HMENU hView = CreatePopupMenu();
  AppendMenu(hView, MF_STRING, IDM_VIEW_TABSWITCHER, L"Tab Grid View\tCtrl+Shift+T");
  AppendMenu(hView, MF_SEPARATOR, 0, NULL);
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

  // Themes submenu
  HMENU hThemes = CreatePopupMenu();
  const auto &themes = SettingsManager::Instance().GetThemes();
  std::wstring activeTheme = SettingsManager::Instance().GetActiveThemeName();
  for (size_t i = 0; i < themes.size(); ++i) {
    UINT flags = MF_STRING;
    if (themes[i].name == activeTheme) flags |= MF_CHECKED;
    AppendMenu(hThemes, flags, IDM_THEME_START + i, themes[i].name.c_str());
  }
  if (!themes.empty())
    AppendMenu(hThemes, MF_SEPARATOR, 0, NULL);
  AppendMenu(hThemes, MF_STRING, IDM_CONFIG_THEME, L10N("menu_config_theme"));
  AppendMenu(hConfig, MF_POPUP, (UINT_PTR)hThemes, L"Themes");

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

  // Tools Menu (built-in tools only; app launchers moved to Plugins menu)
  HMENU hTools = CreatePopupMenu();
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_RUN_MACRO,
             L10N("menu_tools_run_macro"));
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_CONSOLE, L10N("menu_tools_console"));
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_SHELL_MODE, L"Shell Mode");
  AppendMenu(hTools, MF_SEPARATOR, 0, NULL);
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_TERMINAL, L"New Terminal (powershell)");
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_TERMINAL_CMD, L"New Terminal (cmd)");
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_TERMINAL_BASH, L"New Terminal (bash)");
  AppendMenu(hTools, MF_SEPARATOR, 0, NULL);
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_MACRO_GALLERY,
             L10N("menu_tools_macro_gallery"));
  // Language Menu
  HMENU hLang = CreatePopupMenu();
  AppendMenu(hLang, MF_STRING, IDM_LANG_EN, L10N("menu_language_en"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_JP, L10N("menu_language_jp"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_ES, L10N("menu_language_es"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_FR, L10N("menu_language_fr"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_DE, L10N("menu_language_de"));

  // Buffers Menu - integrated buffer/app tabs with type indicators
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
    name += L"  [Buffer]";
    UINT flags = MF_STRING;
    if (g_activeAppTab < 0 && i == g_editor->GetActiveBufferIndex())
      flags |= MF_CHECKED;
    AppendMenu(hBuffers, flags, IDM_BUFFERS_START + i, name.c_str());
  }
  // Append app tabs (terminals show as [Terminal], others as [App])
  for (size_t i = 0; i < g_appTabs.size(); ++i) {
    std::wstring tag = (g_appTabs[i].type == TAB_TYPE_TERMINAL) ? L"  [Terminal]" : L"  [App]";
    std::wstring name = g_appTabs[i].label + tag;
    UINT flags = MF_STRING;
    if (g_activeAppTab == static_cast<int>(i)) flags |= MF_CHECKED;
    AppendMenu(hBuffers, flags, IDM_BUFFERS_START + 200 + i, name.c_str());
  }

  // Help Menu
  HMENU hHelp = CreatePopupMenu();
  AppendMenu(hHelp, MF_STRING, IDM_HELP_DOC, L10N("menu_help_doc"));
  AppendMenu(hHelp, MF_STRING, IDM_HELP_KEYBINDINGS,
             L10N("menu_help_keybindings"));
  AppendMenu(hHelp, MF_STRING, IDM_HELP_ABOUT, L10N("menu_help_about"));
  AppendMenu(hHelp, MF_STRING, IDM_HELP_MESSAGES, L"Show Messages");

  // AI Menu (hidden by default, only shown when IsShowAI)
  HMENU hAi = CreatePopupMenu();
  AppendMenu(hAi, MF_STRING, IDM_AI_MANAGER, L"AI Agent Manager");
  AppendMenu(hAi, MF_STRING, IDM_AI_SETUP_WIZARD, L"AI Setup Wizard");

  // CLI Menu
  HMENU hCli = CreatePopupMenu();
  const auto &cliEntries = SettingsManager::Instance().GetCliEntries();
  for (size_t i = 0; i < cliEntries.size(); ++i) {
    std::wstring text = cliEntries[i].command + L"  →  " + cliEntries[i].folder;
    AppendMenu(hCli, MF_STRING, IDM_CLI_START + i, text.c_str());
  }
  if (!cliEntries.empty())
    AppendMenu(hCli, MF_SEPARATOR, 0, NULL);
  AppendMenu(hCli, MF_STRING, IDM_CLI_CONFIGURE, L"Configure CLI Entries...");

  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFile, L10N("menu_file"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEdit, L10N("menu_edit"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hView, L10N("menu_view"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hConfig, L10N("menu_config"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hTools, L10N("menu_tools"));

  // Plugins Menu
  HMENU hPlugins = CreatePopupMenu();
  {
    bool anyVisible = false;
    for (size_t i = 0; i < g_plugins.size(); ++i) {
      if (g_plugins[i].hidden) continue;
      anyVisible = true;
      AppendMenu(hPlugins, MF_STRING, IDM_PLUGINS_START + i, g_plugins[i].name.c_str());
    }
    if (!anyVisible)
      AppendMenu(hPlugins, MF_GRAYED, 0, L"(No plugins found)");
    else
      AppendMenu(hPlugins, MF_SEPARATOR, 0, NULL);
  }
  AppendMenu(hPlugins, MF_STRING, IDM_PLUGINS_RESCAN, L"Rescan Plugins");
  AppendMenu(hPlugins, MF_STRING, IDM_PLUGINS_CONFIGURE, L"Configure Plugins...");
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hPlugins, L"Plugins");

  if (SettingsManager::Instance().IsShowAI())
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hAi, L"AI");
  else
    DestroyMenu(hAi);
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hLang, L10N("menu_language"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hBuffers, L10N("menu_buffers"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hCli, L"CLI");
  // Process Menu - kill process-bound apps and terminals
  HMENU hProcess = CreatePopupMenu();
  bool hasProcesses = false;
  for (size_t i = 0; i < g_appTabs.size(); ++i) {
    UINT flags = MF_STRING;
    if (g_activeAppTab == static_cast<int>(i)) flags |= MF_CHECKED;
    std::wstring tag = (g_appTabs[i].type == TAB_TYPE_TERMINAL) ? L" [Terminal]" : L" [App]";
    AppendMenu(hProcess, flags, IDM_PROCESS_KILL_START + i, (L"Kill " + g_appTabs[i].label + tag).c_str());
    hasProcesses = true;
  }
  if (hasProcesses) {
    AppendMenu(hProcess, MF_SEPARATOR, 0, NULL);
    AppendMenu(hProcess, MF_STRING, IDM_PROCESS_KILL, L"Kill Active Process");
  } else {
    AppendMenu(hProcess, MF_GRAYED, 0, L"(No running processes)");
  }
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hProcess, L"Process");
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelp, L10N("menu_help"));

  SetMenu(hwnd, hMenu);
  // Set window title: "Ecode - {full file path / tab name}"
  std::wstring title = L"Ecode";
  if (g_activeAppTab >= 0 && static_cast<size_t>(g_activeAppTab) < g_appTabs.size()) {
    title = L"Ecode - " + g_appTabs[g_activeAppTab].label;
  } else {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      std::wstring name = buf->GetPath();
      if (name.empty())
        name = buf->IsScratch() ? L"Scratch" : L"Untitled";
      title = L"Ecode - " + name;
    }
  }
  SetWindowText(hwnd, title.c_str());
  UpdateScrollbars(hwnd);
  UpdateTabs(hwnd);
}
