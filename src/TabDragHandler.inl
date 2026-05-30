// =============================================================================
// TabDragHandler.inl
// Tab control subclass for drag-reordering of all tabs
// Included by main.cpp
// =============================================================================

static bool IsSameTabGroup(int a, int b, size_t visCount) {
  bool aIsBuffer = (a >= 0 && a < static_cast<int>(visCount));
  bool bIsBuffer = (b >= 0 && b < static_cast<int>(visCount));
  return aIsBuffer == bIsBuffer;
}

static bool IsValidAppTabIndex(int tabIndex, size_t visCount) {
  int appEnd = static_cast<int>(visCount + g_appTabs.size());
  return tabIndex >= static_cast<int>(visCount) && tabIndex < appEnd;
}

static LRESULT CALLBACK TabSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                        LPARAM lParam) {
  switch (msg) {
  case WM_LBUTTONDOWN: {
    TCHITTESTINFO hti = {{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, 0};
    int tabIndex = TabCtrl_HitTest(hwnd, &hti);
    int appEnd = static_cast<int>(VisibleBufferCount() + g_appTabs.size());
    if (tabIndex >= 0 && tabIndex < appEnd) {
      g_isDraggingTab = true;
      g_dragTabFrom = tabIndex;
      SetCapture(hwnd);
    }
    break;
  }
  case WM_MOUSEMOVE: {
    if (g_isDraggingTab) {
      SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
      TCHITTESTINFO hti = {{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, 0};
      int hoverTab = TabCtrl_HitTest(hwnd, &hti);
      size_t visCount = VisibleBufferCount();
      int appEnd = static_cast<int>(visCount + g_appTabs.size());
      if (hoverTab >= 0 && hoverTab < appEnd && hoverTab != g_dragTabFrom &&
          IsSameTabGroup(g_dragTabFrom, hoverTab, visCount)) {
        g_suppressTabChange = true;
        TabCtrl_SetCurSel(hwnd, hoverTab);
        g_suppressTabChange = false;
      }
    }
    break;
  }
  case WM_LBUTTONUP: {
    if (g_isDraggingTab) {
      g_isDraggingTab = false;
      ReleaseCapture();

      TCHITTESTINFO hti = {{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, 0};
      int dropTab = TabCtrl_HitTest(hwnd, &hti);
      size_t visCount = VisibleBufferCount();
      int appEnd = static_cast<int>(visCount + g_appTabs.size());

      if (dropTab >= 0 && dropTab < appEnd && dropTab != g_dragTabFrom &&
          IsSameTabGroup(g_dragTabFrom, dropTab, visCount)) {
        HWND parent = GetParent(hwnd);
        bool dragIsBuffer = (g_dragTabFrom < static_cast<int>(visCount));

        if (dragIsBuffer) {
          // Both are buffer tabs - swap buffers
          int bufFrom = TabToBufferIndex(g_dragTabFrom);
          int bufTo = TabToBufferIndex(dropTab);
          if (bufFrom >= 0 && bufTo >= 0) {
            g_editor->SwapBuffers(static_cast<size_t>(bufFrom), static_cast<size_t>(bufTo));
            if (parent) {
              UpdateTabs(parent);
              UpdateMenu(parent);
            }
          }
        } else {
          // Both are app tabs - swap app tabs
          int fromIdx = g_dragTabFrom - static_cast<int>(visCount);
          int toIdx = dropTab - static_cast<int>(visCount);
          if (fromIdx >= 0 && fromIdx < static_cast<int>(g_appTabs.size()) &&
              toIdx >= 0 && toIdx < static_cast<int>(g_appTabs.size())) {
            std::swap(g_appTabs[fromIdx], g_appTabs[toIdx]);
            if (g_activeAppTab == fromIdx)
              g_activeAppTab = toIdx;
            else if (g_activeAppTab == toIdx)
              g_activeAppTab = fromIdx;
            if (parent) UpdateMenu(parent);
          }
        }
      }
      g_dragTabFrom = -1;
    }
    break;
  }
  case WM_CAPTURECHANGED: {
    if (g_isDraggingTab) {
      g_isDraggingTab = false;
      g_dragTabFrom = -1;
    }
    break;
  }
  }
  return CallWindowProc(g_oldTabProc, hwnd, msg, wParam, lParam);
}
