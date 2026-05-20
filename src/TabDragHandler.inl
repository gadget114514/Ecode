// =============================================================================
// TabDragHandler.inl
// Tab control subclass for drag-reordering of app tabs
// Included by main.cpp
// =============================================================================

static LRESULT CALLBACK TabSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                        LPARAM lParam) {
  switch (msg) {
  case WM_LBUTTONDOWN: {
    TCHITTESTINFO hti = {{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, 0};
    int tabIndex = TabCtrl_HitTest(hwnd, &hti);
    size_t bufCount = g_editor->GetBuffers().size();
    int appEnd = static_cast<int>(bufCount + g_appTabs.size());
    if (tabIndex >= static_cast<int>(bufCount) && tabIndex < appEnd) {
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
      size_t bufCount = g_editor->GetBuffers().size();
      int appEnd = static_cast<int>(bufCount + g_appTabs.size());
      if (hoverTab >= static_cast<int>(bufCount) &&
          hoverTab < appEnd && hoverTab != g_dragTabFrom) {
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
      size_t bufCount = g_editor->GetBuffers().size();
      int appEnd = static_cast<int>(bufCount + g_appTabs.size());

      if (dropTab >= static_cast<int>(bufCount) &&
          dropTab < appEnd && dropTab != g_dragTabFrom) {
        int fromIdx = g_dragTabFrom - static_cast<int>(bufCount);
        int toIdx = dropTab - static_cast<int>(bufCount);
        if (fromIdx >= 0 && fromIdx < static_cast<int>(g_appTabs.size()) &&
            toIdx >= 0 && toIdx < static_cast<int>(g_appTabs.size())) {
          std::swap(g_appTabs[fromIdx], g_appTabs[toIdx]);
          if (g_activeAppTab == fromIdx)
            g_activeAppTab = toIdx;
          else if (g_activeAppTab == toIdx)
            g_activeAppTab = fromIdx;
          HWND parent = GetParent(hwnd);
          if (parent) UpdateMenu(parent);
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
