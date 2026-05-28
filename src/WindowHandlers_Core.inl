// =============================================================================
// WindowHandlers_Core.inl
// Handle WM_CREATE, WM_SIZE, WM_CLOSE, WM_DESTROY, etc.
// Included by main.cpp
// =============================================================================

#include <cstdio>
#include <string>

void DebugLog(const std::string &msg, LogLevel level);

static void InternalLogCallback(const std::string &msg, LogLevel level) {
  if (g_editor) {
    const char *levelStr[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    g_editor->LogMessage("[" + std::string(levelStr[level]) + "] " + msg);
  }
}

static LRESULT HandleCreate(HWND hwnd) {
  SetWindowLong(hwnd, GWL_STYLE,
                GetWindowLong(hwnd, GWL_STYLE) | WS_CLIPCHILDREN);
  g_mainHwnd = hwnd;

  g_logCallback = InternalLogCallback;
  g_editor = new Editor();
  g_editor->LogMessage("--- Ecode Session Started ---");

  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(icex);
  icex.dwICC = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS | ICC_TAB_CLASSES;
  InitCommonControlsEx(&icex);

  g_tabHwnd = CreateWindowEx(0, WC_TABCONTROL, NULL,
                             WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_TOOLTIPS, 0, 0, 0, 0, hwnd,
                             (HMENU)2002, GetModuleHandle(NULL), NULL);
  {
    NONCLIENTMETRICSW ncm = {0};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    HFONT hTabFont = CreateFontIndirectW(&ncm.lfMenuFont);
    SendMessage(g_tabHwnd, WM_SETFONT, (WPARAM)hTabFont, TRUE);
  }
  
  g_treeHwnd = CreateWindowEx(0, WC_TREEVIEW, NULL,
                              WS_CHILD | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
                              0, 0, 0, 0, hwnd, (HMENU)2005, GetModuleHandle(NULL), NULL);

  g_statusHwnd = CreateWindowEx(
      0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0,
      0, hwnd, (HMENU)2000, GetModuleHandle(NULL), NULL);
  g_progressHwnd = CreateWindowEx(
      0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0, 0, 0, 0,
      g_statusHwnd, (HMENU)2001, GetModuleHandle(NULL), NULL);
  g_minibufferPromptHwnd =
      CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_CLIPSIBLINGS | SS_LEFT, 0,
                     0, 0, 0, hwnd, (HMENU)2004, GetModuleHandle(NULL), NULL);
  SendMessage(g_minibufferPromptHwnd, WM_SETFONT,
              (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

  g_minibufferHwnd =
      CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
                     WS_CHILD | WS_CLIPSIBLINGS | ES_AUTOHSCROLL, 0, 0, 0, 0,
                     hwnd, (HMENU)2003, GetModuleHandle(NULL), NULL);
  SendMessage(g_minibufferHwnd, WM_SETFONT,
              (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
  g_oldMinibufferProc = (WNDPROC)SetWindowLongPtr(
      g_minibufferHwnd, GWLP_WNDPROC, (LONG_PTR)MinibufferSubclassProc);

  int parts[] = {300, 600, -1};
  SendMessage(g_statusHwnd, SB_SETPARTS, 3, (LPARAM)parts);
  SendMessage(g_statusHwnd, SB_SETTEXT, 0, (LPARAM)L"Ready");

  g_renderer = new EditorBufferRenderer();
  if (!g_renderer->Initialize(hwnd))
    return -1;

  g_editor->SetProgressCallback([](float progress) {
    if (g_progressHwnd) {
      SendMessage(g_progressHwnd, PBM_SETPOS, (int)(progress * 100), 0);
      UpdateWindow(g_statusHwnd);
    }
  });

  g_scriptEngine = new ScriptEngine();
  g_scriptEngine->SetBypassCache(g_bypassCache);
  bool initOk = g_scriptEngine->Initialize();
  if (!initOk && g_scriptEngine->IsFatalError()) {
    DebugLog("ScriptEngine::Initialize failed with fatal error", LOG_ERROR);
  }
  if (initOk && g_compileAllScripts)
    g_scriptEngine->CompileAllScripts();
  g_editor->NewFile();
  ScanPlugins();
  UpdateMenu(hwnd);
  SetTimer(hwnd, 1, 500, NULL);
  g_renderer->SetCaretBlinking(SettingsManager::Instance().IsCaretBlinking());
  g_uFindMsgString = RegisterWindowMessageW(FINDMSGSTRINGW);
  DragAcceptFiles(hwnd, TRUE);

  // Global hotkey: Ctrl+Shift+T → Tab Switcher
  RegisterHotKey(hwnd, HOTKEY_ID_TABSWITCHER, MOD_CONTROL | MOD_SHIFT, 'T');

  // Create tab icon image list
  g_tabImageList = ImageList_Create(16, 16, ILC_COLOR32, 16, 16);
  if (g_tabImageList) {
    TabCtrl_SetImageList(g_tabHwnd, g_tabImageList);
    InitTabIcons(g_tabImageList);
  }

  // Subclass tab control for drag-reorder support
  g_oldTabProc = (WNDPROC)SetWindowLongPtr(
      g_tabHwnd, GWLP_WNDPROC, (LONG_PTR)TabSubclassProc);

  auto &settings = SettingsManager::Instance();
  settings.Load();

  std::wstring projDir = settings.GetProjectDirectory();
  if (!projDir.empty()) {
    SetCurrentDirectoryW(projDir.c_str());
    RefreshTreeView(g_treeHwnd, projDir);
  }

  RECT rc = {0};
  settings.GetWindowRect(rc);
  if (rc.right > rc.left && rc.bottom > rc.top)
    MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
               TRUE);
  if (settings.IsWindowMaximized())
    ShowWindow(hwnd, SW_MAXIMIZE);
  ShowWindow(g_statusHwnd, settings.IsShowStatusBar() ? SW_SHOW : SW_HIDE);
  g_noTitleBar = settings.IsNoTitleBar();
  if (g_noTitleBar) {
    MARGINS margins = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
  }
  g_renderer->SetFont(settings.GetFontFamily(), settings.GetFontSize());
  g_renderer->SetWordWrap(settings.IsWordWrap());
  g_renderer->SetCaretStyle((EditorBufferRenderer::CaretStyle)settings.GetCaretStyle());

  // Load active theme
  {
    const auto &themes = settings.GetThemes();
    std::wstring activeName = settings.GetActiveThemeName();
    auto HexToColor = [](const std::wstring &hex) -> D2D1_COLOR_F {
      unsigned int r = 0, g = 0, b = 0, a = 255;
      if (hex.length() >= 8) {
        swscanf_s(hex.c_str(), L"%02x%02x%02x%02x", &r, &g, &b, &a);
      } else if (hex.length() >= 6) {
        swscanf_s(hex.c_str(), L"%02x%02x%02x", &r, &g, &b);
      }
      return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    };
    for (const auto &t : themes) {
      if (t.name == activeName) {
        Theme theme;
        theme.name = t.name;
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
        break;
      }
    }
  }

  Localization::Instance().SetLanguage(
      static_cast<Language>(settings.GetLanguage()));
  UpdateMenu(hwnd);

  return 0;
}

static LRESULT HandleSize(HWND hwnd, LPARAM lParam) {
  UINT width = LOWORD(lParam), height = HIWORD(lParam);
  if (width == 0 || height == 0) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;
  }
   
  int topBarOffset = g_noTitleBar ? g_topBarHeight : 0;
  int treeWidth = g_treeVisible ? 200 : 0;
  if (g_treeVisible) {
      MoveWindow(g_treeHwnd, 0, 0, treeWidth, height, TRUE);
  }
            
  MoveWindow(g_tabHwnd, treeWidth, topBarOffset, width - treeWidth, 25, TRUE);
  int tabHeight = 25;
  if (IsWindowVisible(g_statusHwnd))
    SendMessage(g_statusHwnd, WM_SIZE, 0, 0);
  RECT rcStatusWnd = {0};
  int statusHeight = 0;
  if (IsWindowVisible(g_statusHwnd)) {
    GetWindowRect(g_statusHwnd, &rcStatusWnd);
    statusHeight = rcStatusWnd.bottom - rcStatusWnd.top;
  }
  int minibufferHeight = g_minibufferVisible ? 24 : 0;
  int promptWidth = 0;
  if (g_minibufferVisible) {
    HDC hdc = GetDC(g_minibufferPromptHwnd);
    HFONT hFont = (HFONT)SendMessage(g_minibufferPromptHwnd, WM_GETFONT, 0, 0);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    std::wstring wprompt = StringToWString(g_minibufferPrompt);
    SIZE sz;
    GetTextExtentPoint32W(hdc, wprompt.c_str(), (int)wprompt.length(), &sz);
    promptWidth = sz.cx + 6;
    SelectObject(hdc, hOldFont);
    ReleaseDC(g_minibufferPromptHwnd, hdc);
    SetWindowTextW(g_minibufferPromptHwnd, wprompt.c_str());
  }
  int mbTop = height - statusHeight - minibufferHeight;
  MoveWindow(g_minibufferPromptHwnd, treeWidth, mbTop, promptWidth, minibufferHeight,
             TRUE);
  ShowWindow(g_minibufferPromptHwnd, g_minibufferVisible ? SW_SHOW : SW_HIDE);
  MoveWindow(g_minibufferHwnd, treeWidth + promptWidth, mbTop, width - treeWidth - promptWidth,
             minibufferHeight, TRUE);
  ShowWindow(g_minibufferHwnd, g_minibufferVisible ? SW_SHOW : SW_HIDE);

  if (IsWindowVisible(g_statusHwnd)) {
    RECT rcStatus;
    SendMessage(g_statusHwnd, SB_GETRECT, 2, (LPARAM)&rcStatus);
    MoveWindow(g_progressHwnd, rcStatus.left + 2, rcStatus.top + 2,
               rcStatus.right - rcStatus.left - 4,
               rcStatus.bottom - rcStatus.top - 4, TRUE);
  }
  int safetyMargin  = 50;
  int contentTop    = topBarOffset + tabHeight;
  int contentHeight = (int)height - contentTop - statusHeight - minibufferHeight - safetyMargin;
  int contentWidth  = (int)width  - treeWidth;

  g_renderer->SetTopOffset((float)(topBarOffset + tabHeight));
  g_renderer->SetLeftOffset((float)treeWidth);
  g_renderer->Resize(width, height);

  // Position all app tabs to cover the content area
  for (auto &t : g_appTabs) {
    if (t.hwnd)
      MoveWindow(t.hwnd, treeWidth, contentTop,
                 contentWidth, contentHeight + safetyMargin, TRUE);
  }
  InvalidateRect(hwnd, NULL, FALSE);

  UpdateScrollbars(hwnd);
  InvalidateRect(hwnd, NULL, FALSE);
  return 0;
}

static LRESULT HandleFindReplace(HWND hwnd, LPARAM lParam) {
  LPFINDREPLACEW lpfr = (LPFINDREPLACEW)lParam;
  Buffer *buf = g_editor->GetActiveBuffer();
  if (!buf)
    return 0;

  auto WToUTF8 = [](LPCWSTR wstr) -> std::string {
    if (!wstr)
      return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (len <= 0)
      return "";
    std::string res(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &res[0], len, NULL, NULL);
    return res;
  };

  if (lpfr->Flags & FR_DIALOGTERM)
    g_hDlgFind = NULL;
  else if (lpfr->Flags & FR_FINDNEXT) {
    std::string findWhat = WToUTF8(lpfr->lpstrFindWhat);
    bool forward = (lpfr->Flags & FR_DOWN) != 0;
    bool matchCase = (lpfr->Flags & FR_MATCHCASE) != 0;
    bool useRegex = false;
    if (findWhat.length() >= 2 && findWhat.front() == '/' &&
        findWhat.back() == '/') {
      findWhat = findWhat.substr(1, findWhat.length() - 2);
      useRegex = true;
    }
    size_t startPos = buf->GetCaretPos();
    if (forward && buf->HasSelection()) {
      size_t s, e;
      buf->GetSelectionRange(s, e);
      startPos = e;
    } else if (!forward && buf->HasSelection()) {
      size_t s, e;
      buf->GetSelectionRange(s, e);
      startPos = s;
    }
    size_t pos = buf->Find(findWhat, startPos, forward, useRegex, matchCase);
    if (pos != std::string::npos) {
      buf->SetSelectionAnchor(pos);
      buf->SetCaretPos(pos + findWhat.length());
      EnsureCaretVisible(hwnd);
      InvalidateRect(hwnd, NULL, FALSE);
    } else
      MessageBeep(MB_ICONWARNING);
  } else if (lpfr->Flags & FR_REPLACE) {
    std::string findWhat = WToUTF8(lpfr->lpstrFindWhat);
    std::string replaceWith = WToUTF8(lpfr->lpstrReplaceWith);
    size_t s, e;
    buf->GetSelectionRange(s, e);
    if (buf->GetText(s, e - s) == findWhat) {
      buf->Replace(s, e, replaceWith);
      buf->SetCaretPos(s + replaceWith.length());
      buf->SetSelectionAnchor(s + replaceWith.length());
    }
    SendMessage(hwnd, g_uFindMsgString, 0, (LPARAM)lpfr);
  } else if (lpfr->Flags & FR_REPLACEALL) {
    std::string findWhat = WToUTF8(lpfr->lpstrFindWhat);
    std::string replaceWith = WToUTF8(lpfr->lpstrReplaceWith);
    bool matchCase = (lpfr->Flags & FR_MATCHCASE) != 0;
    size_t count = 0, total = 0, tempPos = 0;
    while ((tempPos = buf->Find(findWhat, tempPos, true, false, matchCase)) !=
           std::string::npos) {
      total++;
      tempPos += findWhat.length();
    }
    size_t pos = 0;
    while ((pos = buf->Find(findWhat, pos, true, false, matchCase)) !=
           std::string::npos) {
      buf->Replace(pos, pos + findWhat.length(), replaceWith);
      pos += replaceWith.length();
      count++;
      if (total > 0)
        SendMessage(g_progressHwnd, PBM_SETPOS,
                    (int)((float)count / total * 100), 0);
    }
    SendMessage(g_progressHwnd, PBM_SETPOS, 0, 0);
    if (count > 0) {
      UpdateScrollbars(hwnd);
      InvalidateRect(hwnd, NULL, FALSE);
    }
  }
  return 0;
}

static bool HasRunningProcesses() {
  const auto &buffers = g_editor->GetBuffers();
  for (size_t i = 0; i < buffers.size(); ++i) {
    if (buffers[i]->IsShell()) {
      Process *proc = buffers[i]->GetShellProcess();
      if (proc && proc->IsRunning()) return true;
    }
  }
  return false;
}

static LRESULT HandleClose(HWND hwnd) {
  const auto &buffers = g_editor->GetBuffers();
  for (size_t i = 0; i < buffers.size(); ++i) {
    if (buffers[i]->IsDirty()) {
      g_editor->SwitchToBuffer(i);
      UpdateMenu(hwnd);
      UpdateTabs(hwnd);
      InvalidateRect(hwnd, NULL, FALSE);
      UpdateWindow(hwnd);
      if (!PromptSaveBuffer(hwnd, buffers[i].get()))
        return 0;
    }
  }
  if (HasRunningProcesses()) {
    int res = MessageBoxW(hwnd,
        L"Active terminal or shell sessions are running.\nClose anyway?",
        L"Ecode", MB_YESNO | MB_ICONWARNING);
    if (res != IDYES) return 0;
  }
  DestroyWindow(hwnd);
  return 0;
}

static void HandleDestroy(HWND hwnd) {
  auto &settings = SettingsManager::Instance();
  WINDOWPLACEMENT wp = {sizeof(wp)};
  if (GetWindowPlacement(hwnd, &wp)) {
    settings.SetWindowRect(wp.rcNormalPosition);
    settings.SetWindowMaximized(wp.showCmd == SW_SHOWMAXIMIZED);
  }
  settings.SetFontFamily(g_renderer->GetFontFamily());
  settings.SetFontSize(g_renderer->GetFontSize());
  settings.SetLanguage(
      static_cast<int>(Localization::Instance().GetCurrentLanguage()));
  settings.SetWordWrap(g_renderer->IsWordWrap());

  wchar_t curDir[MAX_PATH];
  if (GetCurrentDirectoryW(MAX_PATH, curDir) > 0) {
    settings.SetProjectDirectory(curDir);
  }

  settings.Save();

  UnregisterHotKey(hwnd, HOTKEY_ID_TABSWITCHER);
  HideTabSwitcher();

  // Cleanup all app views — destroy embedded windows first to request graceful exit
  for (auto &t : g_appTabs) {
    if (t.hwnd) DestroyWindow(t.hwnd);
  }
  // Terminate embedded processes and wait; close app process handles without waiting
  for (auto &t : g_appTabs) {
    if (t.hProcess) {
      if (t.hwnd) {
        DWORD waitResult = WaitForSingleObject(t.hProcess, 3000);
        if (waitResult == WAIT_TIMEOUT) {
          TerminateProcess(t.hProcess, 0);
          WaitForSingleObject(t.hProcess, INFINITE);
        }
      }
      CloseHandle(t.hProcess);
    }
  }
  g_appTabs.clear();

  delete g_scriptEngine;
  g_scriptEngine = nullptr;
  delete g_renderer;
  g_renderer = nullptr;
  delete g_editor;
  g_editor = nullptr;
  delete g_lspClient;
  g_lspClient = nullptr;

  PostQuitMessage(0);
}
