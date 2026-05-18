void SaveConfig() {
  std::wofstream file("FastFileSearch.cfg");
  if (file.is_open()) {
    file << currentLang << std::endl;
    for (const auto &t : searchTargets) {
      file << t << std::endl;
    }
  }
}

void LoadConfig(HWND hDlg) {
  std::wifstream file("FastFileSearch.cfg");
  if (file.is_open()) {
    file >> currentLang;
    if (currentLang < 0 || currentLang > 1)
      currentLang = 0;

    std::wstring line;
    std::getline(file, line); // consume newline

    searchTargets.clear();
    SendMessage(hTargetList, LB_RESETCONTENT, 0, 0);

    while (std::getline(file, line)) {
      if (!line.empty() && line.back() == L'\r')
        line.pop_back();
      if (!line.empty()) {
        searchTargets.push_back(line);
        SendMessage(hTargetList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
      }
    }
  }
  SendDlgItemMessage(hDlg, IDC_COMBO_LANG, CB_SETCURSEL, currentLang, 0);
}

void ResizeLayout(HWND hDlg, int cx, int cy) {
  // Basic anchoring logic
  // Margins
  int margin = 10;
  int btnWidth = 80;
  int btnHeight = 25;

  // Top Row
  // IDC_EDIT_QUERY (155, 7) -> Width dynamic
  // IDC_BTN_SEARCH (385, 7) -> Anchor Right

  HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_QUERY);
  HWND hBtnSearch = GetDlgItem(hDlg, IDC_BTN_SEARCH);

  RECT rcEdit, rcBtn;
  GetWindowRect(hEdit, &rcEdit);
  GetWindowRect(hBtnSearch, &rcBtn);
  int editH = rcEdit.bottom - rcEdit.top;
  int btnW = rcBtn.right - rcBtn.left;
  int btnH = rcBtn.bottom - rcBtn.top;

  // Calculate new width for edit
  // Right anchor for button: cx - margin - btnW
  int btnX = cx - margin - btnW;
  SetWindowPos(hBtnSearch, NULL, btnX, 10, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

  // Edit width: btnX - margin - editX (keep EditX)
  // Need original EditX. defined in RC as 155 dlg units ~ 230 px?
  // Let's get current pos mapmode independent if possible.
  // Just map from window rect.
  POINT ptEdit = {0, 0};
  MapWindowPoints(hEdit, hDlg, &ptEdit, 1);
  int editX = ptEdit.x; // Current X
  // Actually EditX is relative to dialog.
  // Use GetWindowRect and ScreenToClient.

  RECT rcDlg;
  GetWindowRect(hDlg, &rcDlg);

  POINT pt = {rcEdit.left, rcEdit.top};
  ScreenToClient(hDlg, &pt);
  int editLeft = pt.x;

  SetWindowPos(hEdit, NULL, 0, 0, btnX - margin - editLeft, editH,
               SWP_NOMOVE | SWP_NOZORDER);

  // List Targets (IDC_LIST_TARGETS)
  HWND hListTargets = GetDlgItem(hDlg, IDC_LIST_TARGETS);
  HWND hBtnAdd = GetDlgItem(hDlg, IDC_BTN_ADD);
  HWND hBtnRem = GetDlgItem(hDlg, IDC_BTN_REMOVE);

  // Buttons anchor right
  SetWindowPos(hBtnAdd, NULL, btnX, 55, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER); // Y approx
  SetWindowPos(hBtnRem, NULL, btnX, 85, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

  // Target List Width
  // Get List Pos
  RECT rcListT;
  GetWindowRect(hListTargets, &rcListT);
  pt.x = rcListT.left;
  pt.y = rcListT.top;
  ScreenToClient(hDlg, &pt);
  int listTx = pt.x;
  int listTy = pt.y;
  int listTh = rcListT.bottom - rcListT.top;

  SetWindowPos(hListTargets, NULL, 0, 0, btnX - margin - listTx, listTh,
               SWP_NOMOVE | SWP_NOZORDER);

  // Main Result List (IDC_LIST_RESULTS)
  // Anchored ALL
  // Top = below targets ~ 140px?
  // Bottom = cy - margin - bottom_row_height
  int bottomRowH = 40;

  HWND hListRes = GetDlgItem(hDlg, IDC_LIST_RESULTS);
  GetWindowRect(hListRes, &rcListRes);
  pt.x = rcListRes.left;
  pt.y = rcListRes.top;
  ScreenToClient(hDlg, &pt);
  int listRy = pt.y;

  SetWindowPos(hListRes, NULL, 0, 0, cx - margin - margin,
               cy - bottomRowH - listRy, SWP_NOMOVE | SWP_NOZORDER);

  // Bottom Row: Save Button, Status
  // Anchor Bottom
  int bottomY = cy - 35;

  HWND hBtnSave = GetDlgItem(hDlg, IDC_BTN_SAVE);
  SetWindowPos(hBtnSave, NULL, margin, bottomY, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER);

  HWND hStatus = GetDlgItem(hDlg, IDC_STATUS);
  // Status x = after save button
  GetWindowRect(hBtnSave, &rcBtn); // Width
  int statusX = margin + (rcBtn.right - rcBtn.left) + margin;
  SetWindowPos(hStatus, NULL, statusX, bottomY + 3, cx - statusX - margin, 20,
               SWP_NOZORDER);
}

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
  // lParam1/2 are indices into searchResults
  if (lParam1 >= searchResults.size() || lParam2 >= searchResults.size())
    return 0;

  const FileResult &a = searchResults[lParam1];
  const FileResult &b = searchResults[lParam2];

  int cmp = 0;
  switch (sortColumn) {
  case 0: // Name
    cmp = lstrcmpiW(a.name.c_str(), b.name.c_str());
    break;
  case 1: // Path
    cmp = lstrcmpiW(a.fullPath.c_str(), b.fullPath.c_str());
    break;
  case 2: // Date
    if (a.fileTime < b.fileTime)
      cmp = -1;
    else if (a.fileTime > b.fileTime)
      cmp = 1;
    break;
  case 3: // Size
    if (a.fileSize < b.fileSize)
      cmp = -1;
    else if (a.fileSize > b.fileSize)
      cmp = 1;
    break;
  }

  return sortAscending ? cmp : -cmp;
}
