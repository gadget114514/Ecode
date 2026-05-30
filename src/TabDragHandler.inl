// =============================================================================
// TabDragHandler.inl
// Tab control subclass for drag-reordering of all tabs via g_tabOrder
// Included by main.cpp
// =============================================================================

static LRESULT CALLBACK TabSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                        LPARAM lParam) {
  switch (msg) {
  case WM_LBUTTONDOWN: {
    TCHITTESTINFO hti = {{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, 0};
    int tabIndex = TabCtrl_HitTest(hwnd, &hti);
    if (tabIndex >= 0 && tabIndex < (int)g_tabOrder.size()) {
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
      if (hoverTab >= 0 && hoverTab < (int)g_tabOrder.size() &&
          hoverTab != g_dragTabFrom) {
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

      if (dropTab >= 0 && dropTab < (int)g_tabOrder.size() &&
          dropTab != g_dragTabFrom) {
        std::swap(g_tabOrder[g_dragTabFrom], g_tabOrder[dropTab]);
        HWND parent = GetParent(hwnd);
        if (parent) {
          UpdateTabs(parent);
          UpdateMenu(parent);
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
