// =============================================================================
// WindowHandlers_Core.inl
// Handle WM_CREATE, WM_SIZE, WM_CLOSE, WM_DESTROY, etc.
// Included by main.cpp
// =============================================================================

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
                             WS_CHILD | WS_VISIBLE | TCS_TABS, 0, 0, 0, 0, hwnd,
                             (HMENU)2002, GetModuleHandle(NULL), NULL);
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
  g_scriptEngine->Initialize();
  if (g_compileAllScripts)
    g_scriptEngine->CompileAllScripts();
  g_editor->NewFile();
  UpdateMenu(hwnd);
  SetTimer(hwnd, 1, 500, NULL);
  g_uFindMsgString = RegisterWindowMessageW(FINDMSGSTRINGW);
  DragAcceptFiles(hwnd, TRUE);

  auto &settings = SettingsManager::Instance();
  settings.Load();
  RECT rc = {0};
  settings.GetWindowRect(rc);
  if (rc.right > rc.left && rc.bottom > rc.top)
    MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
               TRUE);
  if (settings.IsWindowMaximized())
    ShowWindow(hwnd, SW_MAXIMIZE);
  ShowWindow(g_statusHwnd, settings.IsShowStatusBar() ? SW_SHOW : SW_HIDE);
  g_renderer->SetFont(settings.GetFontFamily(), settings.GetFontSize());
  g_renderer->SetWordWrap(settings.IsWordWrap());
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
  DebugLog("HandleSize: width=" + std::to_string(width) +
               " height=" + std::to_string(height) +
               " g_minibufferVisible=" + std::to_string(g_minibufferVisible),
           LOG_INFO);
  MoveWindow(g_tabHwnd, 0, 0, width, 25, TRUE);
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
  DebugLog("  Minibuffer Layout: mbTop=" + std::to_string(mbTop) +
               " mbHeight=" + std::to_string(minibufferHeight) +
               " promptWidth=" + std::to_string(promptWidth),
           LOG_INFO);
  MoveWindow(g_minibufferPromptHwnd, 0, mbTop, promptWidth, minibufferHeight,
             TRUE);
  ShowWindow(g_minibufferPromptHwnd, g_minibufferVisible ? SW_SHOW : SW_HIDE);
  MoveWindow(g_minibufferHwnd, promptWidth, mbTop, width - promptWidth,
             minibufferHeight, TRUE);
  ShowWindow(g_minibufferHwnd, g_minibufferVisible ? SW_SHOW : SW_HIDE);

  if (IsWindowVisible(g_statusHwnd)) {
    RECT rcStatus;
    SendMessage(g_statusHwnd, SB_GETRECT, 2, (LPARAM)&rcStatus);
    MoveWindow(g_progressHwnd, rcStatus.left + 2, rcStatus.top + 2,
               rcStatus.right - rcStatus.left - 4,
               rcStatus.bottom - rcStatus.top - 4, TRUE);
  }
  int safetyMargin = 50;
  g_renderer->SetTopOffset((float)tabHeight);
  g_renderer->Resize(width, height - tabHeight - statusHeight -
                                minibufferHeight - safetyMargin);
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
  settings.Save();

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
