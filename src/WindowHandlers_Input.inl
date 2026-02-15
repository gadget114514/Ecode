// =============================================================================
// WindowHandlers_Input.inl
// Handle WM_PAINT, WM_KEYDOWN, WM_CHAR, Mouse, and Scroll messages
// Included by main.cpp
// =============================================================================

static LRESULT HandlePaint(HWND hwnd) {
  PAINTSTRUCT ps;
  BeginPaint(hwnd, &ps);
  Buffer *activeBuffer = g_editor->GetActiveBuffer();
  if (activeBuffer) {
    size_t scrollLine = activeBuffer->GetScrollLine();
    size_t viewportLineCount = g_renderer->CalculateVisibleLineCount();
    size_t actualLines = 0;
    std::string content = activeBuffer->GetViewportText(
        scrollLine, viewportLineCount, actualLines);

    auto selectionRanges = activeBuffer->GetSelectionRanges();
    size_t logicalCaret = activeBuffer->GetCaretPos();
    size_t visualCaret = activeBuffer->LogicalToVisualOffset(logicalCaret);

    size_t viewportStartPhysicalLine =
        (scrollLine < activeBuffer->GetVisibleLineCount())
            ? activeBuffer->GetPhysicalLine(scrollLine)
            : 0;
    size_t viewportStartLogical =
        activeBuffer->GetLineOffset(viewportStartPhysicalLine);
    size_t viewportStartVisual =
        activeBuffer->LogicalToVisualOffset(viewportStartLogical);
    size_t viewportRelativeCaret = (visualCaret >= viewportStartVisual)
                                       ? (visualCaret - viewportStartVisual)
                                       : 0;

    std::vector<size_t> physicalLineNumbers;
    physicalLineNumbers.reserve(actualLines);
    for (size_t i = 0; i < actualLines; ++i) {
      physicalLineNumbers.push_back(
          activeBuffer->GetPhysicalLine(scrollLine + i));
    }

    auto highlights = activeBuffer->GetHighlights();
    std::vector<Buffer::HighlightRange> viewportHighlights;
    for (const auto &h : highlights) {
      size_t hStartVisual = activeBuffer->LogicalToVisualOffset(h.start);
      size_t hEndVisual =
          activeBuffer->LogicalToVisualOffset(h.start + h.length);
      size_t viewportEndVisual = viewportStartVisual + content.length();

      if (hEndVisual > viewportStartVisual &&
          hStartVisual < viewportEndVisual) {
        Buffer::HighlightRange adjusted = h;
        adjusted.start = (hStartVisual > viewportStartVisual)
                             ? (hStartVisual - viewportStartVisual)
                             : 0;
        size_t endRel = (hEndVisual < viewportEndVisual)
                            ? (hEndVisual - viewportStartVisual)
                            : content.length();
        adjusted.length =
            (endRel > adjusted.start) ? (endRel - adjusted.start) : 0;
        if (adjusted.length > 0)
          viewportHighlights.push_back(adjusted);
      }
    }

    std::vector<Buffer::SelectionRange> viewportSelections;
    for (const auto &s : selectionRanges) {
      size_t sStartVisual = activeBuffer->LogicalToVisualOffset(s.start);
      size_t sEndVisual = activeBuffer->LogicalToVisualOffset(s.end);
      size_t viewportEndVisual = viewportStartVisual + content.length();

      if (sEndVisual > viewportStartVisual &&
          sStartVisual < viewportEndVisual) {
        size_t relStart = (sStartVisual > viewportStartVisual)
                              ? (sStartVisual - viewportStartVisual)
                              : 0;
        size_t relEnd = (sEndVisual < viewportEndVisual)
                            ? (sEndVisual - viewportStartVisual)
                            : content.length();
        if (relEnd > relStart) {
          viewportSelections.push_back({relStart, relEnd});
        }
      }
    }

    g_renderer->DrawEditorLines(
        content, viewportRelativeCaret, &viewportSelections,
        &viewportHighlights, scrollLine + 1, activeBuffer->GetScrollX(),
        &physicalLineNumbers, activeBuffer->GetTotalLines());
  }
  EndPaint(hwnd, &ps);
  return 0;
}

static LRESULT HandleChar(HWND hwnd, WPARAM wParam) {
  if (g_scriptEngine->IsKeyboardCaptured()) {
    wchar_t wc = static_cast<wchar_t>(wParam);
    std::string s;
    int len = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, NULL, 0, NULL, NULL);
    if (len > 0) {
      s.resize(len);
      WideCharToMultiByte(CP_UTF8, 0, &wc, 1, &s[0], len, NULL, NULL);
    }
    if (g_scriptEngine->HandleKeyEvent(s, true)) {
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
  }

  Buffer *activeBuffer = g_editor->GetActiveBuffer();
  if (activeBuffer) {
    if (wParam >= 32 || wParam == VK_RETURN || wParam == VK_TAB) {
      if (activeBuffer->HasSelection())
        activeBuffer->DeleteSelection();
      std::string s;
      if (wParam == VK_RETURN)
        s = "\n";
      else if (wParam == VK_TAB)
        s = "\t";
      else {
        wchar_t wc = static_cast<wchar_t>(wParam);
        int len = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, NULL, 0, NULL, NULL);
        if (len > 0) {
          s.resize(len);
          WideCharToMultiByte(CP_UTF8, 0, &wc, 1, &s[0], len, NULL, NULL);
        }
      }
      if (!s.empty()) {
        activeBuffer->Insert(activeBuffer->GetCaretPos(), s);
        activeBuffer->MoveCaret(static_cast<int>(s.length()));
        activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
        EnsureCaretVisible(hwnd);
        UpdateScrollbars(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
      }
    } else if (wParam == VK_BACK) {
      if (activeBuffer->HasSelection())
        activeBuffer->DeleteSelection();
      else {
        size_t pos = activeBuffer->GetCaretPos();
        if (pos > 0) {
          size_t fetchSize = (pos > 8) ? 8 : pos;
          std::string text = activeBuffer->GetText(pos - fetchSize, fetchSize);
          size_t relPos = text.length();
          size_t prevRelPos = relPos - 1;
          while (prevRelPos > 0 && (text[prevRelPos] & 0xC0) == 0x80)
            prevRelPos--;
          size_t bytesToDelete = relPos - prevRelPos;
          size_t deleteAt = pos - bytesToDelete;
          activeBuffer->Delete(deleteAt, bytesToDelete);
          activeBuffer->SetCaretPos(deleteAt);
          activeBuffer->SetSelectionAnchor(deleteAt);
          EnsureCaretVisible(hwnd);
          UpdateScrollbars(hwnd);
        }
      }
      InvalidateRect(hwnd, NULL, FALSE);
    }
  }
  return 0;
}

static LRESULT HandleKeyDown(HWND hwnd, WPARAM wParam, LPARAM lParam) {
  static bool s_inEscapeSequence = false;

  // 1. Unified mapping from VK_ code to string name
  std::string keyName;
  if (wParam == VK_ESCAPE)
    keyName = "Esc";
  else if (wParam == VK_RETURN)
    keyName = "Enter";
  else if (wParam == VK_BACK)
    keyName = "Backspace";
  else if (wParam == VK_DELETE)
    keyName = "Delete";
  else if (wParam == VK_UP)
    keyName = "Up";
  else if (wParam == VK_DOWN)
    keyName = "Down";
  else if (wParam == VK_LEFT)
    keyName = "Left";
  else if (wParam == VK_RIGHT)
    keyName = "Right";
  else if (wParam == VK_PRIOR)
    keyName = "PageUp";
  else if (wParam == VK_NEXT)
    keyName = "PageDown";
  else if (wParam == VK_HOME)
    keyName = "Home";
  else if (wParam == VK_END)
    keyName = "End";
  else if (wParam == VK_TAB)
    keyName = "Tab";
  else if (wParam >= 'A' && wParam <= 'Z')
    keyName = (char)wParam;
  else if (wParam >= '0' && wParam <= '9') {
    if (GetKeyState(VK_SHIFT) & 0x8000) {
      const char *shiftDigits = ")!@#$%^&*(";
      keyName = shiftDigits[wParam - '0'];
    } else {
      keyName = (char)wParam;
    }
  } else if (wParam == VK_OEM_COMMA)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? "<" : ",";
  else if (wParam == VK_OEM_PERIOD)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? ">" : ".";
  else if (wParam == VK_OEM_1)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? ":" : ";";
  else if (wParam == VK_OEM_2)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? "?" : "/";
  else if (wParam == VK_OEM_3)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? "~" : "`";
  else if (wParam == VK_OEM_4)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? "{" : "[";
  else if (wParam == VK_OEM_5)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? "|" : "\\";
  else if (wParam == VK_OEM_6)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? "}" : "]";
  else if (wParam == VK_OEM_7)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? "\"" : "'";
  else if (wParam == VK_OEM_PLUS)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? "+" : "=";
  else if (wParam == VK_OEM_MINUS)
    keyName = (GetKeyState(VK_SHIFT) & 0x8000) ? "_" : "-";
  else if (wParam >= VK_F1 && wParam <= VK_F12)
    keyName = "F" + std::to_string(wParam - VK_F1 + 1);

  // 2. Global Quit (Ctrl+G) - Checked early to allow canceling prefixes
  if (GetKeyState(VK_CONTROL) & 0x8000 && wParam == 'G') {
    if (g_scriptEngine->IsKeyboardCaptured()) {
      g_scriptEngine->SetCaptureKeyboard(false);
      g_scriptEngine->SetKeyHandler("");
    }
    if (g_minibufferVisible)
      HideMinibuffer();
    if (g_hDlgFind) {
      DestroyWindow(g_hDlgFind);
      g_hDlgFind = NULL;
    }
    s_inEscapeSequence = false;
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf)
      buf->SetSelectionAnchor(buf->GetCaretPos());
    SendMessage(g_statusHwnd, SB_SETTEXT, 0, (LPARAM)L"Quit");
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }

  // 3. Handle Escape sequence (Esc followed by key = Alt+key)
  if (s_inEscapeSequence) {
    s_inEscapeSequence = false;
    if (wParam != VK_ESCAPE) {
      std::string chord = "Alt+";
      if (GetKeyState(VK_CONTROL) & 0x8000)
        chord = "Ctrl+" + chord;
      if (GetKeyState(VK_SHIFT) & 0x8000)
        chord = "Shift+" + chord;

      if (!keyName.empty() && g_scriptEngine->HandleBinding(chord + keyName)) {
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
      return 0; // Swallow key after Esc even if unhandled
    }
  }

  // 4. Normal chord construction (Ctrl+, Shift+, Alt+)
  std::string chord;
  if (GetKeyState(VK_CONTROL) & 0x8000)
    chord += "Ctrl+";
  if (GetKeyState(VK_SHIFT) & 0x8000)
    chord += "Shift+";
  if (GetKeyState(VK_MENU) & 0x8000)
    chord += "Alt+";

  // 5. Special Escape handling: start sequence or quit minibuffer
  if (wParam == VK_ESCAPE) {
    if (g_scriptEngine->IsKeyboardCaptured() &&
        g_scriptEngine->HandleKeyEvent("Escape", false)) {
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    if (g_minibufferVisible) {
      HideMinibuffer();
      return 0;
    }
    s_inEscapeSequence = true;
    return 0;
  }

  Buffer *activeBuffer = g_editor->GetActiveBuffer();
  if (activeBuffer && chord.empty()) {
    bool movement = true;
    if (wParam == VK_LEFT)
      activeBuffer->MoveCaret(-1);
    else if (wParam == VK_RIGHT)
      activeBuffer->MoveCaret(1);
    else if (wParam == VK_UP)
      activeBuffer->MoveCaretUp();
    else if (wParam == VK_DOWN)
      activeBuffer->MoveCaretDown();
    else if (wParam == VK_HOME)
      activeBuffer->MoveCaretHome();
    else if (wParam == VK_END)
      activeBuffer->MoveCaretEnd();
    else if (wParam == VK_PRIOR)
      activeBuffer->MoveCaretPageUp(20);
    else if (wParam == VK_NEXT)
      activeBuffer->MoveCaretPageDown(20);
    else
      movement = false;

    if (movement) {
      if (!(GetKeyState(VK_SHIFT) & 0x8000))
        activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
      InvalidateRect(hwnd, NULL, FALSE);
      EnsureCaretVisible(hwnd);
      return 0;
    }

    if (wParam == VK_DELETE) {
      if (activeBuffer->HasSelection())
        activeBuffer->DeleteSelection();
      else if (activeBuffer->GetCaretPos() < activeBuffer->GetTotalLength()) {
        size_t pos = activeBuffer->GetCaretPos();
        size_t nextPos = pos;
        std::string text = activeBuffer->GetText(pos, 4);
        if (!text.empty()) {
          unsigned char c = (unsigned char)text[0];
          if (c < 0x80)
            nextPos = pos + 1;
          else if ((c & 0xE0) == 0xC0)
            nextPos = pos + 2;
          else if ((c & 0xF0) == 0xE0)
            nextPos = pos + 3;
          else if ((c & 0xF8) == 0xF0)
            nextPos = pos + 4;
          else
            nextPos = pos + 1;
        }
        activeBuffer->Delete(
            pos,
            (std::min)(nextPos - pos, activeBuffer->GetTotalLength() - pos));
      }
      InvalidateRect(hwnd, NULL, FALSE);
      EnsureCaretVisible(hwnd);
      return 0;
    }
  }

  // 6. Hook for scripts (Captured input or key bindings)
  if (g_scriptEngine->IsKeyboardCaptured()) {
    if (!keyName.empty() || !chord.empty()) {
      if (!keyName.empty() &&
          g_scriptEngine->HandleKeyEvent(chord + keyName, false)) {
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
    }
  }

  if (!chord.empty() && !keyName.empty()) {
    DebugLog("HandleKeyDown: final chord [" + chord + keyName + "]", LOG_INFO);
    if (g_scriptEngine->HandleBinding(chord + keyName)) {
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
  }

  // 7. Special Buffer Handlers (Shell, Scratch)
  if (wParam == VK_RETURN && activeBuffer && activeBuffer->IsShell()) {
    size_t caretPos = activeBuffer->GetCaretPos();
    size_t lineIdx = activeBuffer->GetLineAtOffset(caretPos);
    size_t lineStart = activeBuffer->GetLineOffset(lineIdx);
    size_t lineEnd = activeBuffer->GetLineOffset(lineIdx + 1);
    if (lineEnd == 0)
      lineEnd = activeBuffer->GetTotalLength();
    std::string lineText =
        activeBuffer->GetText(lineStart, lineEnd - lineStart);
    while (!lineText.empty() &&
           (lineText.back() == '\n' || lineText.back() == '\r'))
      lineText.pop_back();
    activeBuffer->SendToShell(lineText + "\n");
    size_t endPos = activeBuffer->GetTotalLength();
    activeBuffer->SetCaretPos(endPos);
    activeBuffer->Insert(endPos, "\n");
    activeBuffer->SetCaretPos(activeBuffer->GetTotalLength());
    activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
    EnsureCaretVisible(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }

  if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000) &&
      activeBuffer && activeBuffer->IsScratch()) {
    std::string code = activeBuffer->GetText(0, activeBuffer->GetTotalLength());
    std::string result = g_scriptEngine->Evaluate(code);
    activeBuffer->Insert(activeBuffer->GetTotalLength(),
                         "\n// Result: " + result + "\n");
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }

  return -1; // Unhandled
}

LRESULT HandleMouseDown(HWND hwnd, LPARAM lParam) {
  g_isDragging = true;
  SetCapture(hwnd);
  int x = LOWORD(lParam), y = HIWORD(lParam);
  Buffer *activeBuffer = g_editor->GetActiveBuffer();
  if (activeBuffer) {
    size_t visualLineIndex;
    size_t totalLines = activeBuffer->GetTotalLines();
    if (g_renderer->HitTestGutter((float)x, (float)y, (size_t)totalLines,
                                  visualLineIndex)) {
      size_t physicalLine = activeBuffer->GetPhysicalLine(
          visualLineIndex + activeBuffer->GetScrollLine());
      int digits =
          (totalLines > 0) ? (int)std::to_string(totalLines).length() : 1;
      float gutterWidth = (digits * 8.0f) + 15.0f;
      if (x > gutterWidth - 15.0f)
        activeBuffer->ToggleFold(physicalLine);
      else
        activeBuffer->SelectLine(physicalLine);
    } else {
      size_t scrollLine = activeBuffer->GetScrollLine();
      size_t viewportLineCount = g_renderer->CalculateVisibleLineCount();
      size_t actualLines = 0;
      std::string viewportText = activeBuffer->GetViewportText(
          scrollLine, viewportLineCount, actualLines);
      size_t viewportRelVisualPos = g_renderer->GetPositionFromPoint(
          viewportText, (float)x, (float)y, totalLines);

      size_t viewportStartPhysical = activeBuffer->GetPhysicalLine(scrollLine);
      size_t viewportStartLogical =
          activeBuffer->GetLineOffset(viewportStartPhysical);
      size_t viewportStartVisual =
          activeBuffer->LogicalToVisualOffset(viewportStartLogical);

      size_t totalVisualPos = viewportStartVisual + viewportRelVisualPos;
      size_t pos = activeBuffer->VisualToLogicalOffset(totalVisualPos);

      activeBuffer->SetCaretPos(pos);
      if (!(GetKeyState(VK_SHIFT) & 0x8000))
        activeBuffer->SetSelectionAnchor(pos);
      EnsureCaretVisible(hwnd);
    }
    InvalidateRect(hwnd, NULL, FALSE);
  }
  return 0;
}

LRESULT HandleMouseMove(HWND hwnd, LPARAM lParam) {
  if (g_isDragging) {
    int x = LOWORD(lParam), y = HIWORD(lParam);
    Buffer *activeBuffer = g_editor->GetActiveBuffer();
    if (activeBuffer) {
      if (GetKeyState(VK_MENU) & 0x8000)
        activeBuffer->SetSelectionMode(SelectionMode::Box);
      else
        activeBuffer->SetSelectionMode(SelectionMode::Normal);
      size_t scrollLine = activeBuffer->GetScrollLine();
      size_t viewportLineCount = g_renderer->CalculateVisibleLineCount();
      size_t actualLines = 0;
      std::string viewportText = activeBuffer->GetViewportText(
          scrollLine, viewportLineCount, actualLines);
      size_t totalLines = activeBuffer->GetTotalLines();
      size_t viewportRelVisualPos = g_renderer->GetPositionFromPoint(
          viewportText, (float)x, (float)y, totalLines);
      size_t viewportStartPhysical = activeBuffer->GetPhysicalLine(scrollLine);
      size_t viewportOffset =
          activeBuffer->GetLineOffset(viewportStartPhysical);
      size_t pos = viewportOffset + viewportRelVisualPos;
      activeBuffer->SetCaretPos(pos);
      EnsureCaretVisible(hwnd);
    }
    InvalidateRect(hwnd, NULL, FALSE);
  }
  return 0;
}

static LRESULT HandleVScroll(HWND hwnd, WPARAM wParam) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (!buf)
    return 0;
  SCROLLINFO si = {sizeof(si), SIF_ALL};
  GetScrollInfo(hwnd, SB_VERT, &si);
  int oldPos = si.nPos;
  switch (LOWORD(wParam)) {
  case SB_TOP:
    si.nPos = si.nMin;
    break;
  case SB_BOTTOM:
    si.nPos = si.nMax;
    break;
  case SB_LINEUP:
    si.nPos -= 1;
    break;
  case SB_LINEDOWN:
    si.nPos += 1;
    break;
  case SB_PAGEUP:
    si.nPos -= si.nPage;
    break;
  case SB_PAGEDOWN:
    si.nPos += si.nPage;
    break;
  case SB_THUMBTRACK:
    si.nPos = si.nTrackPos;
    break;
  }
  si.fMask = SIF_POS;
  SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
  GetScrollInfo(hwnd, SB_VERT, &si);
  if (si.nPos != oldPos) {
    buf->SetScrollLine(si.nPos);
    InvalidateRect(hwnd, NULL, FALSE);
  }
  return 0;
}

static LRESULT HandleHScroll(HWND hwnd, WPARAM wParam) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (!buf)
    return 0;
  SCROLLINFO si = {sizeof(si), SIF_ALL};
  GetScrollInfo(hwnd, SB_HORZ, &si);
  int oldPos = si.nPos;
  switch (LOWORD(wParam)) {
  case SB_LEFT:
    si.nPos = si.nMin;
    break;
  case SB_RIGHT:
    si.nPos = si.nMax;
    break;
  case SB_LINELEFT:
    si.nPos -= 10;
    break;
  case SB_LINERIGHT:
    si.nPos += 10;
    break;
  case SB_PAGELEFT:
    si.nPos -= si.nPage;
    break;
  case SB_PAGERIGHT:
    si.nPos += si.nPage;
    break;
  case SB_THUMBTRACK:
    si.nPos = si.nTrackPos;
    break;
  }
  si.fMask = SIF_POS;
  SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
  GetScrollInfo(hwnd, SB_HORZ, &si);
  if (si.nPos != oldPos) {
    buf->SetScrollX((float)si.nPos);
    InvalidateRect(hwnd, NULL, FALSE);
  }
  return 0;
}
