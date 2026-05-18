// Helper for Custom Draw
LRESULT DrawItemWithHighlight(LPNMLVCUSTOMDRAW lplvcd, HWND hDlg) {
  int iItem = (int)lplvcd->nmcd.dwItemSpec;
  int iSubItem = lplvcd->iSubItem;
  HDC hdc = lplvcd->nmcd.hdc;

  // Get Text
  wchar_t textBuf[1024];
  ListView_GetItemText(lplvcd->nmcd.hdr.hwndFrom, iItem, iSubItem, textBuf,
                       1024);
  std::wstring text = textBuf;

  // Get Query
  wchar_t queryBuf[256];
  GetDlgItemTextW(hDlg, IDC_EDIT_QUERY, queryBuf, 256);
  std::wstring query = queryBuf;

  if (query.empty() || text.empty()) {
    return CDRF_DODEFAULT;
  }

  // Prepare Colors
  bool isSelected = (lplvcd->nmcd.uItemState & CDIS_SELECTED) != 0;
  COLORREF clrText = isSelected ? GetSysColor(COLOR_HIGHLIGHTTEXT)
                                : GetSysColor(COLOR_WINDOWTEXT);
  COLORREF clrBk =
      isSelected ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_WINDOW);
  COLORREF clrMatchBk = RGB(0, 255, 255); // Cyan
  COLORREF clrMatchText = RGB(0, 0, 0);   // Black for contrast on Cyan

  // Fill Background
  RECT rc = lplvcd->nmcd.rc;
  if (iSubItem == 0)
    rc.right -= 2; // Adjust for some margin if needed
  // However, for subitem 0, ListView sometimes gives full row rect?
  // Use ListView_GetSubItemRect to be safe?
  // lplvcd->nmcd.rc IS the subitem rect in CDDS_SUBITEMPREPAINT unless subitem
  // 0 partial. Allow default fill? No, we draw over.
  FillRect(hdc, &rc,
           (HBRUSH)(isSelected ? (COLOR_HIGHLIGHT + 1) : (COLOR_WINDOW + 1)));

  // Setup DC
  int oldBkMode = SetBkMode(hdc, TRANSPARENT);

  // Searching logic (Case Insensitive)
  std::wstring textLower = text;
  std::wstring queryLower = query;
  for (auto &c : textLower)
    c = towlower(c);
  for (auto &c : queryLower)
    c = towlower(c);

  size_t pos = 0;
  size_t found = textLower.find(queryLower, pos);

  RECT rcText = rc;
  rcText.left += 6; // Padding

  // Measure and Draw Loop
  // We assume single line text

  // Font
  HFONT hFont = (HFONT)SendMessage(lplvcd->nmcd.hdr.hwndFrom, WM_GETFONT, 0, 0);
  HGDIOBJ oldFont = SelectObject(hdc, hFont);

  int currentX = rcText.left;
  int rightLimit = rc.right - 2;
  int y =
      rcText.top + (rcText.bottom - rcText.top - 14) / 2; // Vert center approx
  // Actually using DrawText with DT_VCENTER is better but requires exact rect.
  // Let's use ExtTextOut or DrawText. DrawText handles clipping better.

  auto DrawSegment = [&](const std::wstring &seg, bool highlight) {
    if (seg.empty())
      return;

    SIZE sz;
    GetTextExtentPoint32W(hdc, seg.c_str(), (int)seg.length(), &sz);

    // Background for match
    RECT segRect;
    segRect.left = currentX;
    segRect.top = rc.top;
    segRect.bottom = rc.bottom;
    segRect.right = currentX + sz.cx;

    if (highlight) {
      HBRUSH hBr = CreateSolidBrush(clrMatchBk);
      FillRect(hdc, &segRect, hBr);
      DeleteObject(hBr);
      SetTextColor(hdc, clrMatchText);
    } else {
      SetTextColor(hdc, clrText);
    }

    // Draw Text
    RECT drawRc = segRect;
    drawRc.left += 0; // already padded by start position
    // Vertical Center
    DrawTextW(hdc, seg.c_str(), (int)seg.length(), &drawRc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    currentX += sz.cx;
  };

  while (found != std::wstring::npos) {
    // Pre-match
    if (found > pos) {
      DrawSegment(text.substr(pos, found - pos), false);
    }

    // Match
    DrawSegment(text.substr(found, query.length()), true);

    pos = found + query.length();
    found = textLower.find(queryLower, pos);
  }

  // Post-match
  if (pos < text.length()) {
    DrawSegment(text.substr(pos), false);
  }

  SelectObject(hdc, oldFont);
  SetBkMode(hdc, oldBkMode);

  return CDRF_SKIPDEFAULT;
}
