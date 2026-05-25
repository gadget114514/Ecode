// =============================================================================
// TopBar.inl
// Custom top bar with menu labels, title text, and window controls.
// Used only when g_noTitleBar == true.
// Included by main.cpp
// =============================================================================

static HFONT CreateTopBarFont() {
  NONCLIENTMETRICSW ncm = {0};
  ncm.cbSize = sizeof(ncm);
  SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
  return CreateFontIndirectW(&ncm.lfMenuFont);
}

void DrawTopBar(HDC hdc, HWND hwnd) {
  RECT clientRect;
  GetClientRect(hwnd, &clientRect);
  RECT barRect = { clientRect.left, clientRect.top, clientRect.right, g_topBarHeight };

  int border = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

  COLORREF bgCol, textCol, btnBg;
  if (g_isActive) {
    bgCol = RGB(45, 45, 48);
    textCol = RGB(200, 200, 200);
    btnBg = RGB(45, 45, 48);
  } else {
    bgCol = RGB(35, 35, 38);
    textCol = RGB(140, 140, 140);
    btnBg = RGB(35, 35, 38);
  }

  HBRUSH bgBrush = CreateSolidBrush(bgCol);
  FillRect(hdc, &barRect, bgBrush);
  DeleteObject(bgBrush);

  bool isMaximized = IsZoomed(hwnd);
  int btnW = 34, btnH = g_topBarHeight;
  int btnTop = 0;

  g_closeButtonRect = { clientRect.right - border - btnW, btnTop, clientRect.right - border, btnTop + btnH };
  g_maxButtonRect   = { g_closeButtonRect.left - btnW, btnTop, g_closeButtonRect.left, btnTop + btnH };
  g_minButtonRect   = { g_maxButtonRect.left - btnW, btnTop, g_maxButtonRect.left, btnTop + btnH };

  HFONT hFont = CreateTopBarFont();
  HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, textCol);

  int x = 8;
  for (auto &ml : g_menuLabels) {
    if (ml.label.empty()) continue;
    SIZE sz;
    GetTextExtentPoint32W(hdc, ml.label.c_str(), (int)ml.label.size(), &sz);
    ml.rect = { x, 0, x + sz.cx + 16, g_topBarHeight };
    RECT textRect = { x + 8, 0, x + sz.cx + 8, g_topBarHeight };
    DrawTextW(hdc, ml.label.c_str(), (int)ml.label.size(), &textRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    x += sz.cx + 16;
  }

  int rightEdge = g_minButtonRect.left - 10;
  if (rightEdge > x) {
    std::wstring title = g_windowTitle;
    if (!title.empty()) {
      SIZE titleSz;
      GetTextExtentPoint32W(hdc, title.c_str(), (int)title.size(), &titleSz);
      int titleX = x + 10;
      int titleMaxW = rightEdge - titleX - 10;
      if (titleSz.cx > titleMaxW) {
        while (title.size() > 1 && titleSz.cx > titleMaxW) {
          title.pop_back();
          GetTextExtentPoint32W(hdc, title.c_str(), (int)title.size(), &titleSz);
        }
        title += L"…";
      }
      if (!title.empty()) {
        RECT titleRect = { titleX, 0, titleX + titleMaxW, g_topBarHeight };
        DrawTextW(hdc, title.c_str(), (int)title.size(), &titleRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
      }
    }
  }

  SelectObject(hdc, hOldFont);
  DeleteObject(hFont);

  int btnIdx = 0;
  for (auto *btnRect : { &g_minButtonRect, &g_maxButtonRect, &g_closeButtonRect }) {
    btnIdx++;
    RECT r = *btnRect;
    InflateRect(&r, -2, -2);
    UINT state = DFCS_CAPTIONMIN;
    if (btnRect == &g_closeButtonRect) state = DFCS_CAPTIONCLOSE;
    else if (btnRect == &g_maxButtonRect) state = isMaximized ? DFCS_CAPTIONRESTORE : DFCS_CAPTIONMAX;
    if (g_topBarButtonPushed == btnIdx) state |= DFCS_PUSHED;
    DrawFrameControl(hdc, &r, DFC_CAPTION, state);
  }
}

void HandleTopBarClick(HWND hwnd, int x, int y) {
  for (auto &ml : g_menuLabels) {
    if (x >= ml.rect.left && x <= ml.rect.right && ml.popup) {
      POINT pt = { ml.rect.left, ml.rect.bottom };
      ClientToScreen(hwnd, &pt);
      TrackPopupMenu(ml.popup, TPM_LEFTALIGN | TPM_TOPALIGN,
                     pt.x, pt.y, 0, hwnd, NULL);
      return;
    }
  }
}

void HandleTopBarDoubleClick(HWND hwnd) {
  if (IsZoomed(hwnd))
    ShowWindow(hwnd, SW_RESTORE);
  else
    ShowWindow(hwnd, SW_MAXIMIZE);
}

void HandleTopBarRightClick(HWND hwnd, int screenX, int screenY) {
  HMENU hSysMenu = GetSystemMenu(hwnd, FALSE);
  if (!hSysMenu) return;
  TrackPopupMenu(hSysMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                 screenX, screenY, 0, hwnd, NULL);
}

bool HandleTopBarButtonDown(HWND hwnd, int x, int y) {
  POINT pt = { x, y };
  int btnIdx = 0;
  for (auto *btnRect : { &g_minButtonRect, &g_maxButtonRect, &g_closeButtonRect }) {
    btnIdx++;
    if (PtInRect(btnRect, pt)) {
      g_topBarButtonPushed = btnIdx;
      RECT r = *btnRect;
      InvalidateRect(hwnd, &r, FALSE);
      return true;
    }
  }
  return false;
}

void HandleTopBarButtonUp(HWND hwnd) {
  int btnIdx = g_topBarButtonPushed;
  g_topBarButtonPushed = 0;
  POINT pt;
  GetCursorPos(&pt);
  ScreenToClient(hwnd, &pt);
  RECT *r = nullptr;
  if (btnIdx == 1) r = &g_minButtonRect;
  else if (btnIdx == 2) r = &g_maxButtonRect;
  else if (btnIdx == 3) r = &g_closeButtonRect;
  if (r) {
    InvalidateRect(hwnd, r, FALSE);
    if (PtInRect(r, pt)) {
      switch (btnIdx) {
      case 1: ShowWindow(hwnd, SW_MINIMIZE); break;
      case 2:
        if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
        else ShowWindow(hwnd, SW_MAXIMIZE);
        break;
      case 3: SendMessage(hwnd, WM_CLOSE, 0, 0); break;
      }
    }
  }
}

bool HandleTopBarSysKeyDown(HWND hwnd, WPARAM wParam) {
  if (wParam == VK_SPACE) {
    RECT rc;
    GetWindowRect(hwnd, &rc);
    HMENU hSysMenu = GetSystemMenu(hwnd, FALSE);
    if (hSysMenu) {
      TrackPopupMenu(hSysMenu, TPM_LEFTALIGN | TPM_TOPALIGN,
                     rc.left, rc.top + g_topBarHeight, 0, hwnd, NULL);
    }
    return true;
  }
  wchar_t ch = (wchar_t)towlower((wint_t)wParam);
  for (auto &ml : g_menuLabels) {
    auto pos = ml.label.find(L'&');
    if (pos == std::wstring::npos || pos + 1 >= ml.label.size())
      continue;
    wchar_t accel = towlower(ml.label[pos + 1]);
    if (accel == ch && ml.popup) {
      POINT pt = { ml.rect.left, ml.rect.bottom };
      ClientToScreen(hwnd, &pt);
      TrackPopupMenu(ml.popup, TPM_LEFTALIGN | TPM_TOPALIGN,
                     pt.x, pt.y, 0, hwnd, NULL);
      return true;
    }
  }
  return false;
}
