// =============================================================================
// AppMain.inl
// WindowProc skeleton and wWinMain entry point
// Included by main.cpp
// =============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  if (uMsg == g_uFindMsgString)
    return HandleFindReplace(hwnd, lParam);

  if (uMsg == WM_NCCALCSIZE && g_noTitleBar) {
    return 0;
  }
  if (uMsg == WM_NCHITTEST && g_noTitleBar) {
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    ScreenToClient(hwnd, &pt);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int border = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
    if (pt.y < border && pt.x < border) return HTTOPLEFT;
    if (pt.y < border && pt.x > rc.right - border) return HTTOPRIGHT;
    if (pt.y > rc.bottom - border && pt.x < border) return HTBOTTOMLEFT;
    if (pt.y > rc.bottom - border && pt.x > rc.right - border) return HTBOTTOMRIGHT;
    if (pt.x < border) return HTLEFT;
    if (pt.x > rc.right - border) return HTRIGHT;
    if (pt.y < border) return HTTOP;
    if (pt.y > rc.bottom - border) return HTBOTTOM;
    if (pt.y < g_topBarHeight) {
      for (auto &ml : g_menuLabels) {
        if (pt.x >= ml.rect.left && pt.x <= ml.rect.right)
          return HTCLIENT;
      }
      POINT clientPt = pt;
      if (PtInRect(&g_minButtonRect, clientPt) ||
          PtInRect(&g_maxButtonRect, clientPt) ||
          PtInRect(&g_closeButtonRect, clientPt))
        return HTCLIENT;
      return HTCAPTION;
    }
    return HTCLIENT;
  }
  if (uMsg == WM_NCACTIVATE && g_noTitleBar) {
    if (!wParam && g_menuTracking)
      return TRUE; // suppress deactivation flicker while our own menu is open
    g_isActive = (wParam != FALSE);
    InvalidateRect(hwnd, NULL, FALSE);
    return TRUE;
  }
  if (uMsg == WM_NCPAINT && g_noTitleBar) {
    return 0;
  }
  if (uMsg == WM_ERASEBKGND) {
    return TRUE;
  }
  if (uMsg == WM_GETMINMAXINFO && g_noTitleBar) {
    MINMAXINFO *mmi = (MINMAXINFO *)lParam;
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int border = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
    mmi->ptMaxPosition.x = workArea.left - border;
    mmi->ptMaxPosition.y = workArea.top - border;
    mmi->ptMaxSize.x = (workArea.right - workArea.left) + border * 2;
    mmi->ptMaxSize.y = (workArea.bottom - workArea.top) + border * 2;
    return 0;
  }

  switch (uMsg) {
  case WM_CREATE:
    return HandleCreate(hwnd);
  case WM_COPYDATA: {
    PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lParam;
    if (pcds && pcds->dwData == 0x5654) {
      const char* logMsg = (const char*)pcds->lpData;
      if (logMsg && strncmp(logMsg, "[VT]", 4) == 0) {
        if (g_editor) {
          g_editor->LogMessage(logMsg);
        }
      }
    }
    return 0;
  }
  case WM_SETFOCUS:
    if (g_activeAppTab >= 0 && (size_t)g_activeAppTab < g_appTabs.size())
      SetFocus(g_appTabs[g_activeAppTab].hwnd);
    return 0;
  case WM_DEFERRED_FOCUS:
    if (IsWindow((HWND)wParam))
      SetFocus((HWND)wParam);
    return 0;
  case WM_SIZE:
    return HandleSize(hwnd, lParam);
  case WM_PAINT:
    return HandlePaint(hwnd);
  case WM_COMMAND:
    return HandleCommand(hwnd, wParam, lParam);
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if (g_noTitleBar && HandleTopBarSysKeyDown(hwnd, wParam))
      return 0;
    if (HandleKeyDown(hwnd, wParam, lParam) == 0)
      return 0;
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  case WM_CHAR:
    return HandleChar(hwnd, wParam);
  case WM_LBUTTONDOWN: {
    if (g_noTitleBar) {
      int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
      if (y >= 0 && y < g_topBarHeight) {
        if (!HandleTopBarButtonDown(hwnd, x, y))
          HandleTopBarClick(hwnd, x, y);
        return 0;
      }
    }
    return HandleMouseDown(hwnd, lParam);
  }
  case WM_LBUTTONUP: {
    if (g_noTitleBar && g_topBarButtonPushed) {
      HandleTopBarButtonUp(hwnd);
      return 0;
    }
    g_isDragging = false;
    ReleaseCapture();
    return 0;
  }
  case WM_LBUTTONDBLCLK: {
    if (g_noTitleBar) {
      int y = GET_Y_LPARAM(lParam);
      if (y >= 0 && y < g_topBarHeight) {
        HandleTopBarDoubleClick(hwnd);
        return 0;
      }
    }
    break;
  }
  case WM_MOUSEMOVE:
    return HandleMouseMove(hwnd, lParam);
  case WM_VSCROLL:
    return HandleVScroll(hwnd, wParam);
  case WM_HSCROLL:
    return HandleHScroll(hwnd, wParam);
  case WM_MOUSEWHEEL: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (!buf)
      return 0;
    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    int dir = SettingsManager::Instance().IsReverseScrollDirection() ? 1 : -1;
    int lines = (int)buf->GetScrollLine() + dir * (delta / WHEEL_DELTA * 3);
    buf->SetScrollLine(
        (std::max)(0, (std::min)(lines, (int)buf->GetTotalLines() - 1)));
    UpdateScrollbars(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_CONTEXTMENU: {
    if ((HWND)wParam == g_tabHwnd) {
      return 0; // Handled by NM_RCLICK in WM_NOTIFY
    }
    if (g_noTitleBar) {
      int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
      POINT pt = { x, y };
      ScreenToClient(hwnd, &pt);
      if (pt.y >= 0 && pt.y < g_topBarHeight) {
        HandleTopBarRightClick(hwnd, x, y);
        return 0;
      }
    }
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_UNDO, L10N("menu_edit_undo"));
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_REDO, L10N("menu_edit_redo"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_CUT, L10N("menu_edit_cut"));
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_COPY, L10N("menu_edit_copy"));
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_PASTE, L10N("menu_edit_paste"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_SELECT_ALL,
               L10N("menu_edit_select_all"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_TAG_JUMP, L"Tag Jump");
    int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
    if (x == -1 && y == -1) {
      RECT rc;
      GetWindowRect(hwnd, &rc);
      x = rc.left + 50;
      y = rc.top + 50;
    }
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, x, y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
    return 0;
  }
  case WM_TIMER:
    if (wParam == 1) {
      if (g_renderer->GetCaretBlinking()) {
        static bool caretVisible = true;
        caretVisible = !caretVisible;
        g_renderer->SetCaretVisible(caretVisible);
        InvalidateRect(hwnd, NULL, FALSE);
      } else {
        // Ensure it is visible if blinking is off
        g_renderer->SetCaretVisible(true);
        // We might want to InvalidateRect once if we just switched modes, but
        // for now this ensures it stays visible.
      }
    }
    return 0;
  case WM_SHELL_OUTPUT: {
    ShellOutput *output = (ShellOutput *)wParam;
    if (output) {
      if (!output->callback.empty()) {
        if (g_scriptEngine) {
          g_scriptEngine->CallGlobalFunction(output->callback, output->text);
        }
      } else if (g_editor && g_editor->IsValidBuffer(output->buffer)) {
        output->buffer->Insert(output->buffer->GetTotalLength(), output->text);
        output->buffer->SetInputStart(output->buffer->GetTotalLength());
        if (g_editor->GetActiveBuffer() == output->buffer) {
          output->buffer->SetCaretPos(output->buffer->GetTotalLength());
          output->buffer->SetSelectionAnchor(output->buffer->GetCaretPos());
          EnsureCaretVisible(hwnd);
          InvalidateRect(hwnd, NULL, FALSE);
        }
      }
      delete output;
    }
    return 0;
  }
  case WM_SET_PROCESS_HANDLE: {
    int appIdx = (int)wParam;
    HANDLE hProcess = (HANDLE)lParam;
    if (appIdx >= 0 && (size_t)appIdx < g_appTabs.size()) {
      g_appTabs[appIdx].hProcess = hProcess;
    } else {
      CloseHandle(hProcess);
    }
    return 0;
  }
  case WM_TERMINAL_PROGRESS: {
    int scaled = (int)lParam; // 0–10000 or -1 to clear
    if (g_progressHwnd) {
      if (scaled < 0) {
        SendMessage(g_progressHwnd, PBM_SETPOS, 0, 0);
      } else {
        SendMessage(g_progressHwnd, PBM_SETPOS, scaled / 100, 0);
      }
      UpdateWindow(g_statusHwnd);
    }
    return 0;
  }
  case WM_EMBED_APP: {
    HWND childHwnd = (HWND)wParam;
    int appIdx = (int)lParam;
    if (appIdx >= 0 && (size_t)appIdx < g_appTabs.size()) {
      if (g_appTabs[appIdx].hwnd) {
        DestroyWindow(g_appTabs[appIdx].hwnd);
      }

      // Sync thread input queues for robust cross-process keyboard and focus interaction
      DWORD childThreadId = GetWindowThreadProcessId(childHwnd, NULL);
      DWORD parentThreadId = GetCurrentThreadId();
      AttachThreadInput(childThreadId, parentThreadId, TRUE);

      SetParent(childHwnd, hwnd);
      LONG style = GetWindowLong(childHwnd, GWL_STYLE);
      style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
      style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
      SetWindowLong(childHwnd, GWL_STYLE, style);
      SetMenu(childHwnd, NULL);
      SetWindowPos(childHwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
      g_appTabs[appIdx].hwnd = childHwnd;
      RECT rc;
      GetClientRect(hwnd, &rc);
      SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
      if (g_activeAppTab == appIdx) {
        ShowWindow(childHwnd, SW_SHOW);
        PostMessageW(hwnd, WM_DEFERRED_FOCUS, (WPARAM)childHwnd, 0);
      } else {
        ShowWindow(childHwnd, SW_HIDE);
      }
      UpdateMenu(hwnd);
      InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
  }
  case WM_GREP_RESULT: {
    std::string *text = (std::string*)lParam;
    if (text) {
      Buffer *buf = g_editor ? g_editor->GetBufferByName(L"*Find Results*") : nullptr;
      if (buf) {
        buf->Insert(buf->GetTotalLength(), *text);
        if (g_editor->GetActiveBuffer() == buf) {
          buf->SetCaretPos(buf->GetTotalLength());
          EnsureCaretVisible(hwnd);
          InvalidateRect(hwnd, NULL, FALSE);
        }
      }
      delete text;
    }
    return 0;
  }
  case WM_GREP_PROGRESS: {
    int filesProcessed = (int)lParam;
    SendMessage(g_progressHwnd, PBM_SETPOS, filesProcessed % 100, 0);
    return 0;
  }
  case WM_GREP_COMPLETE: {
    int totalMatches = (int)wParam;
    g_grepSearchActive = false;
    SendMessage(g_progressHwnd, PBM_SETPOS, 0, 0);
    Buffer *buf = g_editor ? g_editor->GetBufferByName(L"*Find Results*") : nullptr;
    if (buf) {
      std::string done = "\n--- Done. " + std::to_string(totalMatches) + " matches found. ---\n";
      buf->Insert(buf->GetTotalLength(), done);
      if (g_editor->GetActiveBuffer() == buf) {
        buf->SetCaretPos(buf->GetTotalLength());
        EnsureCaretVisible(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
      }
    }
    return 0;
  }
  case WM_HOTKEY:
    if ((int)wParam == HOTKEY_ID_TABSWITCHER)
      ShowTabSwitcher(hwnd);
    return 0;
  case WM_DROPFILES: {
    HDROP hDrop = (HDROP)wParam;
    UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
    for (UINT i = 0; i < count; i++) {
      wchar_t path[MAX_PATH];
      DragQueryFile(hDrop, i, path, MAX_PATH);
      g_editor->OpenFile(path);
    }
    DragFinish(hDrop);
    UpdateMenu(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_NOTIFY: {
    NMHDR *pnm = (NMHDR *)lParam;
    if (pnm->hwndFrom == g_tabHwnd && pnm->code == TCN_SELCHANGE) {
      if (g_suppressTabChange) break;
      int sel = TabCtrl_GetCurSel(g_tabHwnd);
      size_t bufCount = g_editor->GetBuffers().size();
      int appStart = static_cast<int>(bufCount);
      int appEnd = appStart + static_cast<int>(g_appTabs.size());

      if (sel >= appStart && sel < appEnd) {
        int appIdx = sel - appStart;
        for (auto &t : g_appTabs) if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
        g_activeAppTab = appIdx;
        ShowScrollBar(hwnd, SB_BOTH, FALSE);
        if (g_appTabs[appIdx].hwnd) {
          ShowWindow(g_appTabs[appIdx].hwnd, SW_SHOW);
          PostMessage(hwnd, WM_DEFERRED_FOCUS, (WPARAM)g_appTabs[appIdx].hwnd, 0);
        }
        UpdateMenu(hwnd);
      } else if (sel >= 0 && sel < static_cast<int>(bufCount)) {
        for (auto &t : g_appTabs) if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
        g_activeAppTab = -1;
        ShowScrollBar(hwnd, SB_BOTH, TRUE);
        g_editor->SwitchToBuffer(static_cast<size_t>(sel));
        SetFocus(hwnd);
        UpdateMenu(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
      }
    } else if (pnm->hwndFrom == g_treeHwnd && pnm->code == NM_DBLCLK) {
      HTREEITEM hItem = TreeView_GetSelection(g_treeHwnd);
      if (hItem) {
        TVITEMW tvi = {0};
        tvi.mask = TVIF_PARAM;
        tvi.hItem = hItem;
        if (TreeView_GetItem(g_treeHwnd, &tvi) && tvi.lParam) {
          TreeItemData* data = (TreeItemData*)tvi.lParam;
          if (!data->isDirectory) {
            size_t idx = g_editor->OpenFile(data->path);
            if (idx != static_cast<size_t>(-1)) {
              g_editor->SwitchToBuffer(idx);
              UpdateMenu(hwnd);
              InvalidateRect(hwnd, NULL, FALSE);
            }
          }
        }
      }
    } else if (pnm->code == TTN_GETDISPINFOW) {
      NMTTDISPINFOW *pdi = (NMTTDISPINFOW *)lParam;
      if (pdi->hdr.hwndFrom == TabCtrl_GetToolTips(g_tabHwnd)) {
        int tabIndex = (int)pdi->hdr.idFrom;
        auto &buffers = g_editor->GetBuffers();
        if (tabIndex >= 0 && tabIndex < (int)buffers.size()) {
          std::wstring path = buffers[tabIndex]->GetPath();
          if (path.empty()) {
            path = buffers[tabIndex]->IsScratch() ? L"Scratch" : L"Untitled";
          }
          wcsncpy_s(pdi->szText, path.c_str(), _countof(pdi->szText));
          pdi->szText[_countof(pdi->szText) - 1] = L'\0';
        }
      }
    } else if (pnm->hwndFrom == g_tabHwnd && pnm->code == NM_RCLICK) {
      TCHITTESTINFO hti;
      GetCursorPos(&hti.pt);
      POINT ptScreen = hti.pt;
      ScreenToClient(g_tabHwnd, &hti.pt);
      int tabIndex = TabCtrl_HitTest(g_tabHwnd, &hti);
      if (tabIndex != -1) {
        size_t bufCount = g_editor->GetBuffers().size();
        int appStart = static_cast<int>(bufCount);
        int appEnd = appStart + static_cast<int>(g_appTabs.size());
        bool isAppTab = (tabIndex >= appStart && tabIndex < appEnd);
        HMENU hMenu = CreatePopupMenu();
        if (isAppTab) {
          AppendMenu(hMenu, MF_STRING, IDM_TAB_CLOSE_TERMINAL, L"Close");
          AppendMenu(hMenu, MF_STRING, IDM_TAB_CLOSE_TERMINAL + 1, L"Kill Process");
        } else if (tabIndex >= 0 && tabIndex < static_cast<int>(bufCount)) {
          AppendMenu(hMenu, MF_STRING, IDM_TAB_COPY_PATH, L"Copy Full Path");
        }
        int res = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, ptScreen.x, ptScreen.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);

        if (res == IDM_TAB_COPY_PATH) {
          auto &buffers = g_editor->GetBuffers();
          if (tabIndex >= 0 && tabIndex < (int)buffers.size()) {
            std::wstring path = buffers[tabIndex]->GetPath();
            if (path.empty()) {
              path = buffers[tabIndex]->IsScratch() ? L"Scratch" : L"Untitled";
            }
            if (OpenClipboard(hwnd)) {
              EmptyClipboard();
              size_t cbStr = (path.length() + 1) * sizeof(wchar_t);
              HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cbStr);
              if (hMem) {
                memcpy(GlobalLock(hMem), path.c_str(), cbStr);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
              }
              CloseClipboard();
            }
          }
        } else if (res == IDM_TAB_CLOSE_TERMINAL + 1) {
          // Kill Process on app tab
          int appIdx = tabIndex - appStart;
          if (appIdx >= 0 && appIdx < static_cast<int>(g_appTabs.size())) {
            if (g_appTabs[appIdx].hProcess) {
              TerminateProcess(g_appTabs[appIdx].hProcess, 0);
              CloseHandle(g_appTabs[appIdx].hProcess);
            }
            if (g_appTabs[appIdx].hwnd) DestroyWindow(g_appTabs[appIdx].hwnd);
            g_appTabs.erase(g_appTabs.begin() + appIdx);
            if (g_activeAppTab == appIdx) g_activeAppTab = -1;
            else if (g_activeAppTab > appIdx) g_activeAppTab--;
            UpdateMenu(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
          }
        } else if (res == IDM_TAB_CLOSE_TERMINAL) {
          int appStart = static_cast<int>(bufCount);
          int appIdx = tabIndex - appStart;
          if (appIdx >= 0 && appIdx < static_cast<int>(g_appTabs.size())) {
            if (g_appTabs[appIdx].hProcess) {
              TerminateProcess(g_appTabs[appIdx].hProcess, 0);
              CloseHandle(g_appTabs[appIdx].hProcess);
            }
            if (g_appTabs[appIdx].hwnd) DestroyWindow(g_appTabs[appIdx].hwnd);
            g_appTabs.erase(g_appTabs.begin() + appIdx);
            if (g_activeAppTab == appIdx) g_activeAppTab = -1;
            else if (g_activeAppTab > appIdx) g_activeAppTab--;
            UpdateMenu(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
          }
        }
      }
    }
    return 0;
  }
  case WM_IME_SETCONTEXT:
    lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  case WM_IME_STARTCOMPOSITION: {
    g_imeComposing = true;
    g_imeComposition.clear();
    g_imeCompAttr.clear();
    HIMC himc = ImmGetContext(hwnd);
    if (himc) {
      POINT ptScreen = g_renderer->GetCaretScreenPoint();
      POINT ptClient = ptScreen;
      ScreenToClient(hwnd, &ptClient);
      COMPOSITIONFORM cf = {CFS_POINT, ptClient};
      ImmSetCompositionWindow(himc, &cf);
      CANDIDATEFORM ccf = {0, CFS_CANDIDATEPOS, ptScreen};
      ImmSetCandidateWindow(himc, &ccf);
      ImmReleaseContext(hwnd, himc);
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_IME_COMPOSITION: {
    HIMC himc = ImmGetContext(hwnd);
    if (himc) {
      // Intermediate composition string (visual overlay, not in buffer)
      if (lParam & GCS_COMPSTR) {
        LONG size = ImmGetCompositionStringW(himc, GCS_COMPSTR, NULL, 0);
        if (size > 0) {
          g_imeComposition.resize(size / sizeof(wchar_t));
          ImmGetCompositionStringW(himc, GCS_COMPSTR, g_imeComposition.data(), size);
          LONG attrSize = ImmGetCompositionStringW(himc, GCS_COMPATTR, NULL, 0);
          if (attrSize > 0) {
            g_imeCompAttr.resize(attrSize);
            ImmGetCompositionStringW(himc, GCS_COMPATTR, g_imeCompAttr.data(), attrSize);
          } else {
            g_imeCompAttr.assign(g_imeComposition.size(), ATTR_INPUT);
          }
        } else {
          g_imeComposition.clear();
          g_imeCompAttr.clear();
        }
        InvalidateRect(hwnd, NULL, FALSE);
      }
      // Finalized result string -> insert directly into buffer (not via WM_CHAR)
      if (lParam & GCS_RESULTSTR) {
        Buffer *buf = g_editor ? g_editor->GetActiveBuffer() : nullptr;
        LONG size = ImmGetCompositionStringW(himc, GCS_RESULTSTR, NULL, 0);
        if (size > 0 && buf) {
          std::vector<wchar_t> wbuf(size / sizeof(wchar_t) + 1);
          ImmGetCompositionStringW(himc, GCS_RESULTSTR, wbuf.data(), size);
          int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), (int)(size / sizeof(wchar_t)), NULL, 0, NULL, NULL);
          std::string utf8(utf8Len, '\0');
          WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), (int)(size / sizeof(wchar_t)), utf8.data(), utf8Len, NULL, NULL);
          buf->Insert(buf->GetCaretPos(), utf8);
          buf->MoveCaret((int)utf8.length());
          buf->SetSelectionAnchor(buf->GetCaretPos());
          EnsureCaretVisible(hwnd);
          UpdateScrollbars(hwnd);
        }
        g_imeComposition.clear();
        g_imeCompAttr.clear();
        g_imeComposing = false;
        InvalidateRect(hwnd, NULL, FALSE);
      }
      ImmReleaseContext(hwnd, himc);
    }
    return 0;
  }
  case WM_IME_ENDCOMPOSITION:
    g_imeComposition.clear();
    g_imeCompAttr.clear();
    g_imeComposing = false;
    InvalidateRect(hwnd, NULL, FALSE);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  case WM_IME_NOTIFY: {
    if (wParam == IMN_OPENCANDIDATE || wParam == IMN_CHANGECANDIDATE) {
      HIMC himc = ImmGetContext(hwnd);
      if (himc) {
        CANDIDATEFORM cf = {0, CFS_CANDIDATEPOS, {0, 0}};
        cf.ptCurrentPos = g_renderer->GetCaretScreenPoint(); // screen coords
        ImmSetCandidateWindow(himc, &cf);
        ImmReleaseContext(hwnd, himc);
      }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  case WM_CLOSE:
    return HandleClose(hwnd);
  case WM_DESTROY:
    HandleDestroy(hwnd);
    return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
  const wchar_t CLASS_NAME[] = L"EcodeWindowClass";
  WNDCLASS wc = {0};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
  wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
  if (!RegisterClass(&wc))
    return 0;

  int argc;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  bool headless = false;
  if (argv) {
    for (int i = 1; i < argc; ++i) {
      if (wcscmp(argv[i], L"-headless") == 0)
        headless = true;
      else if (wcscmp(argv[i], L"-no-cache") == 0)
        g_bypassCache = true;
      else if (wcscmp(argv[i], L"-compile-all") == 0)
        g_compileAllScripts = true;
      else if (wcscmp(argv[i], L"-e") == 0 && i + 1 < argc) {
        // -e handled later after ScriptEngine initialized
      }
    }
  }

  HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"Ecode",
                             WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL |
                                 WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
  if (!hwnd)
    return 0;

  SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(hInstance, MAKEINTRESOURCE(101)));
  SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(hInstance, MAKEINTRESOURCE(101)));

  if (argv) {
    for (int i = 1; i < argc; ++i) {
      if (wcscmp(argv[i], L"-headless") == 0 ||
          wcscmp(argv[i], L"-no-cache") == 0 ||
          wcscmp(argv[i], L"-compile-all") == 0)
        continue;
      else if (wcscmp(argv[i], L"-e") == 0 && i + 1 < argc) {
        std::wstring code = argv[++i];
        int len = WideCharToMultiByte(CP_UTF8, 0, code.c_str(), -1, NULL, 0,
                                      NULL, NULL);
        if (len > 0) {
          std::string codeA(len - 1, 0);
          WideCharToMultiByte(CP_UTF8, 0, code.c_str(), -1, &codeA[0], len,
                              NULL, NULL);
          g_scriptEngine->Evaluate(codeA);
        }
      } else {
        g_editor->OpenFile(argv[i]);
      }
    }
    LocalFree(argv);
    UpdateMenu(hwnd);
  }

  // Ensure at least one buffer exists
  if (g_editor->GetBuffers().empty()) {
    g_editor->NewFile("Untitled");
    UpdateMenu(hwnd);
  }

  if (headless)
    return 0;
  ShowWindow(hwnd, nCmdShow);
  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (g_hDlgFind == NULL || !IsDialogMessage(g_hDlgFind, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  return 0;
}

// Entry point for Console subsystem (for testing/debug)
#ifdef ECODE_CONSOLE_BUILD
int main() {
  return wWinMain(GetModuleHandle(NULL), NULL, GetCommandLineW(), SW_SHOW);
}
#endif
