// =============================================================================
// TabSwitcherView.inl
// Alt-Tab style realtime tab grid view.
//
// Column count is derived dynamically from the client width on every WM_SIZE.
// Vertical overflow is handled with a native scrollbar + mouse-wheel.
// Each grid cell registers its own DWM thumbnail that maps its source HWND
// (AppTab's own HWND if available, else g_mainHwnd) into the cell rectangle.
// =============================================================================

#include <dwmapi.h>
#include <windowsx.h>

static const wchar_t *kTabSwitcherClass = L"EcodeTabSwitcher";

struct SwitcherItem {
    enum Kind { BUFFER = 0, APPTAB = 1 };
    Kind         kind;
    int          index;
    std::wstring label;
    bool         isTerminal;
};

static std::vector<HTHUMBNAIL>   g_switcherThumbs;
static std::vector<HBITMAP>      g_switcherBitmaps; // per-item; non-null for Buffer cells
static HWND                      g_switcherHwnd     = nullptr;
static std::vector<SwitcherItem> g_switcherItems;
static int                       g_switcherSel      = 0;
static bool                      g_switcherTermOnly = false;
static int                       g_switcherCols     = 4;   // updated on WM_SIZE
static int                       g_switcherScrollY  = 0;   // vertical scroll in px

// ---------------------------------------------------------------------------
// Fixed layout constants
// ---------------------------------------------------------------------------
static const int kSW_PAD      = 10;
static const int kSW_LABEL_H  = 28;
static const int kSW_BORDER   = 2;
static const int kSW_HEADER_H = 36;
static const int kSW_FOOTER_H = 28;

// Cell size is read from settings at runtime.
static inline int SwCellW() { return SettingsManager::Instance().GetTabGridCellW(); }
static inline int SwCellH() { return SettingsManager::Instance().GetTabGridCellH(); }
// Bitmap interior size (excludes border and label strip).
static inline int SwBmpW()  { return SwCellW() - 2 * kSW_BORDER; }
static inline int SwBmpH()  { return SwCellH() - kSW_LABEL_H - 2 * kSW_BORDER; }

// ---------------------------------------------------------------------------
// Dynamic layout helpers
// ---------------------------------------------------------------------------
static void SwRecalcCols(int clientWidth) {
    int cols = (clientWidth - kSW_PAD) / (SwCellW() + kSW_PAD);
    g_switcherCols = (std::max)(1, cols);
}

// Total height of all cell rows (does not include scroll offset).
static int SwContentHeight() {
    int rows = g_switcherItems.empty()
        ? 1
        : ((int)g_switcherItems.size() + g_switcherCols - 1) / g_switcherCols;
    return kSW_HEADER_H
         + kSW_PAD + rows * (SwCellH() + kSW_PAD)
         + kSW_FOOTER_H;
}

// Cell rect in client coords, accounting for current scroll offset.
static RECT SwCellRect(int idx) {
    int col = idx % g_switcherCols;
    int row = idx / g_switcherCols;
    int cw  = SwCellW(), ch = SwCellH();
    RECT r;
    r.left   = kSW_PAD + col * (cw + kSW_PAD);
    r.top    = kSW_HEADER_H + kSW_PAD
               + row * (ch + kSW_PAD) - g_switcherScrollY;
    r.right  = r.left + cw;
    r.bottom = r.top  + ch;
    return r;
}

static RECT SwThumbRect(RECT cell) {
    return { cell.left  + kSW_BORDER,
             cell.top   + kSW_BORDER,
             cell.right - kSW_BORDER,
             cell.bottom - kSW_LABEL_H - kSW_BORDER };
}

static RECT SwLabelRect(RECT cell) {
    return { cell.left, cell.bottom - kSW_LABEL_H,
             cell.right, cell.bottom };
}

// ---------------------------------------------------------------------------
// Scrollbar
// ---------------------------------------------------------------------------
static void SwUpdateScrollbar(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    int contentH = SwContentHeight();
    int pageH    = rc.bottom - rc.top;
    int maxScroll = (std::max)(0, contentH - pageH);

    if (g_switcherScrollY > maxScroll) g_switcherScrollY = maxScroll;
    if (g_switcherScrollY < 0)         g_switcherScrollY = 0;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = contentH - 1;
    si.nPage  = (UINT)pageH;
    si.nPos   = g_switcherScrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

// ---------------------------------------------------------------------------
// Item list
// ---------------------------------------------------------------------------
static void SwRebuildItems() {
    g_switcherItems.clear();

    if (g_editor) {
        const auto &bufs = g_editor->GetBuffers();
        for (int i = 0; i < (int)bufs.size(); ++i) {
            SwitcherItem it;
            it.kind       = SwitcherItem::BUFFER;
            it.index      = i;
            it.isTerminal = bufs[i]->IsShell();
            std::wstring path = bufs[i]->GetPath();
            if (path.empty()) {
                it.label = L"[Untitled]";
            } else {
                size_t sep = path.find_last_of(L"\\/");
                it.label = (sep != std::wstring::npos) ? path.substr(sep + 1) : path;
            }
            if (!g_switcherTermOnly || it.isTerminal)
                g_switcherItems.push_back(it);
        }
    }

    for (int i = 0; i < (int)g_appTabs.size(); ++i) {
        SwitcherItem it;
        it.kind       = SwitcherItem::APPTAB;
        it.index      = i;
        it.isTerminal = (g_appTabs[i].type == TAB_TYPE_TERMINAL);
        it.label      = g_appTabs[i].label;
        if (!g_switcherTermOnly || it.isTerminal)
            g_switcherItems.push_back(it);
    }

    if (g_switcherSel >= (int)g_switcherItems.size())
        g_switcherSel = (std::max)(0, (int)g_switcherItems.size() - 1);
}

// ---------------------------------------------------------------------------
// DWM thumbnail management
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Buffer bitmap rendering
// ---------------------------------------------------------------------------

static HBITMAP SwRenderBufferBitmap(Buffer *buf) {
    int bmpW = SwBmpW(), bmpH = SwBmpH();
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp  = CreateCompatibleBitmap(hdcScreen, bmpW, bmpH);
    ReleaseDC(nullptr, hdcScreen);

    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBmp);

    // Background
    RECT rc = { 0, 0, bmpW, bmpH };
    HBRUSH bgBr = CreateSolidBrush(RGB(18, 20, 34));
    FillRect(hdcMem, &rc, bgBr);
    DeleteObject(bgBr);

    // Small monospace font
    HFONT hFont = CreateFontW(
        11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);

    SetBkMode(hdcMem, TRANSPARENT);

    TEXTMETRIC tm = {};
    GetTextMetrics(hdcMem, &tm);
    int lineH    = tm.tmHeight + 1;
    int margin   = 4;
    int maxLines = (bmpH - margin) / lineH;

    size_t totalLines = buf->GetTotalLines();
    size_t totalLen   = buf->GetTotalLength();

    for (int i = 0; i < (std::min)((int)totalLines, maxLines); ++i) {
        size_t lineStart = buf->GetLineOffset((size_t)i);
        size_t lineEnd   = ((size_t)(i + 1) < totalLines)
                         ? buf->GetLineOffset((size_t)(i + 1))
                         : totalLen;
        size_t lineLen = (lineEnd > lineStart) ? lineEnd - lineStart : 0;
        lineLen = (std::min)(lineLen, (size_t)300);

        std::string raw = buf->GetText(lineStart, lineLen);
        // Strip trailing CR/LF
        while (!raw.empty() && (raw.back() == '\n' || raw.back() == '\r'))
            raw.pop_back();
        if (raw.empty()) continue;

        // UTF-8 → wchar
        int wlen = MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), -1, nullptr, 0);
        std::wstring wline(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), -1, &wline[0], wlen);

        bool isComment = !raw.empty() && (raw.find("//") == 0 || raw.front() == '#');
        SetTextColor(hdcMem, isComment ? RGB(90, 110, 80) : RGB(160, 170, 205));

        RECT lr = { margin, margin + i * lineH,
                    bmpW - margin, margin + (i + 1) * lineH };
        DrawTextW(hdcMem, wline.c_str(), -1, &lr,
                  DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS | DT_NOCLIP);
    }

    SelectObject(hdcMem, hOldFont);
    SelectObject(hdcMem, hOld);
    DeleteObject(hFont);
    DeleteDC(hdcMem);
    return hBmp;
}

static void SwDestroyBitmaps() {
    for (HBITMAP h : g_switcherBitmaps)
        if (h) DeleteObject(h);
    g_switcherBitmaps.clear();
}

// Called after SwRebuildItems: discard old bitmaps and size the vector.
// Actual generation happens lazily in SwPaint.
static void SwResetBitmaps() {
    SwDestroyBitmaps();
    g_switcherBitmaps.resize(g_switcherItems.size(), nullptr);
}

static void SwUnregisterAllThumbs() {
    for (HTHUMBNAIL h : g_switcherThumbs)
        if (h) DwmUnregisterThumbnail(h);
    g_switcherThumbs.clear();
}

// DwmRegisterThumbnail requires a top-level (non-WS_CHILD) source window.
// AppTab windows are embedded children (SetParent + WS_CHILD).
// We briefly strip WS_CHILD so DWM accepts the registration, then restore
// the style immediately.  Once registered the relationship persists by HWND.
static HTHUMBNAIL SwRegisterChildThumb(HWND dest, HWND src) {
    LONG style    = GetWindowLong(src, GWL_STYLE);
    bool wasChild = (style & WS_CHILD) != 0;
    if (wasChild)
        SetWindowLong(src, GWL_STYLE, style & ~WS_CHILD);

    HTHUMBNAIL h = nullptr;
    HRESULT hr   = DwmRegisterThumbnail(dest, src, &h);

    if (wasChild)
        SetWindowLong(src, GWL_STYLE, style); // restore immediately

    return SUCCEEDED(hr) ? h : nullptr;
}

// Register thumbnails only for AppTab items with their own HWND.
// Editor buffers have no separate HWND so they get no thumbnail.
// Items where registration fails (or would duplicate g_mainHwnd) are left null.
static void SwRegisterThumbs(HWND switcherHwnd) {
    SwUnregisterAllThumbs();
    g_switcherThumbs.resize(g_switcherItems.size(), nullptr);

    for (int i = 0; i < (int)g_switcherItems.size(); ++i) {
        const auto &it = g_switcherItems[i];
        if (it.kind != SwitcherItem::APPTAB) continue;

        int idx = it.index;
        if (idx < 0 || idx >= (int)g_appTabs.size()) continue;
        HWND src = g_appTabs[idx].hwnd;
        if (!src || !IsWindow(src)) continue;

        // Skip if it would be the same as the main window (no useful preview).
        if (src == g_mainHwnd) continue;

        g_switcherThumbs[i] = SwRegisterChildThumb(switcherHwnd, src);
    }
}

// Push updated position for every thumbnail.
// Cells scrolled outside [HEADER_H, clientBottom - FOOTER_H] are hidden.
static void SwUpdateAllThumbs(HWND switcherHwnd) {
    RECT rcClient;
    GetClientRect(switcherHwnd, &rcClient);
    int visTop    = kSW_HEADER_H;
    int visBottom = rcClient.bottom - kSW_FOOTER_H;

    for (int i = 0; i < (int)g_switcherThumbs.size(); ++i) {
        HTHUMBNAIL h = g_switcherThumbs[i];
        if (!h) continue;

        RECT cell  = SwCellRect(i);
        RECT thumb = SwThumbRect(cell);

        bool visible = (thumb.bottom > visTop && thumb.top < visBottom);

        DWM_THUMBNAIL_PROPERTIES props = {};
        props.dwFlags =
            DWM_TNP_VISIBLE              |
            DWM_TNP_RECTDESTINATION      |
            DWM_TNP_SOURCECLIENTAREAONLY |
            DWM_TNP_OPACITY;
        props.fVisible              = visible ? TRUE : FALSE;
        props.fSourceClientAreaOnly = TRUE;
        props.rcDestination         = thumb;
        props.opacity               = 255;
        DwmUpdateThumbnailProperties(h, &props);
    }
}

// ---------------------------------------------------------------------------
// Activate selected item in main window
// ---------------------------------------------------------------------------
static void SwActivateItem(HWND parentHwnd) {
    if (g_switcherSel < 0 || g_switcherSel >= (int)g_switcherItems.size())
        return;
    const auto &it = g_switcherItems[g_switcherSel];

    if (it.kind == SwitcherItem::BUFFER) {
        g_editor->SwitchToBuffer(it.index);
        for (auto &t : g_appTabs) if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
        g_activeAppTab = -1;
        ShowScrollBar(parentHwnd, SB_BOTH, TRUE);
    } else {
        for (auto &t : g_appTabs) if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
        g_activeAppTab = it.index;
        if (g_appTabs[it.index].hwnd)
            ShowWindow(g_appTabs[it.index].hwnd, SW_SHOW);
        g_suppressTabChange = true;
        TabCtrl_SetCurSel(g_tabHwnd,
            (int)g_editor->GetBuffers().size() + it.index);
        g_suppressTabChange = false;
        ShowScrollBar(parentHwnd, SB_BOTH, FALSE);
    }

    RECT rc; GetClientRect(parentHwnd, &rc);
    SendMessage(parentHwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
    UpdateMenu(parentHwnd);
    UpdateTabs(parentHwnd);
    InvalidateRect(parentHwnd, NULL, FALSE);
}

// ---------------------------------------------------------------------------
// Scroll helper
// ---------------------------------------------------------------------------
static void SwScrollBy(HWND hwnd, int delta) {
    RECT rc; GetClientRect(hwnd, &rc);
    int contentH  = SwContentHeight();
    int pageH     = rc.bottom - rc.top;
    int maxScroll = (std::max)(0, contentH - pageH);

    g_switcherScrollY += delta;
    if (g_switcherScrollY < 0)         g_switcherScrollY = 0;
    if (g_switcherScrollY > maxScroll) g_switcherScrollY = maxScroll;

    SwUpdateScrollbar(hwnd);
    SwUpdateAllThumbs(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

// Ensure the selected cell is visible (scroll into view).
static void SwEnsureSelVisible(HWND hwnd) {
    if (g_switcherSel < 0 || g_switcherSel >= (int)g_switcherItems.size())
        return;
    RECT rc; GetClientRect(hwnd, &rc);
    int visTop    = kSW_HEADER_H;
    int visBottom = rc.bottom - kSW_FOOTER_H;
    RECT cell = SwCellRect(g_switcherSel);

    if (cell.top < visTop)
        SwScrollBy(hwnd, cell.top - visTop);
    else if (cell.bottom > visBottom)
        SwScrollBy(hwnd, cell.bottom - visBottom);
}

// ---------------------------------------------------------------------------
// GDI painting  (borders, labels, header/footer)
// ---------------------------------------------------------------------------
static HFONT SwFont(int size, bool bold) {
    return CreateFontW(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                       FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

static void SwPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // Background
    {
        HBRUSH br = CreateSolidBrush(RGB(22, 22, 34));
        FillRect(hdc, &rcClient, br);
        DeleteObject(br);
    }

    SetBkMode(hdc, TRANSPARENT);
    HFONT hBold   = SwFont(15, true);
    HFONT hNormal = SwFont(13, false);
    HFONT hSmall  = SwFont(11, false);

    // --- Header (fixed, not scrolled) ---
    {
        HFONT old = (HFONT)SelectObject(hdc, hBold);
        SetTextColor(hdc, RGB(210, 220, 255));
        std::wstring title = g_switcherTermOnly
            ? L"Tab Grid View  [Terminal Only]"
            : L"Tab Grid View  [All]";
        TextOutW(hdc, kSW_PAD, 8, title.c_str(), (int)title.size());

        SelectObject(hdc, hSmall);
        SetTextColor(hdc, RGB(90, 100, 140));
        const wchar_t *hint =
            L"T: toggle filter   ↑↓←→: move   Enter: open   Esc: close";
        RECT hr2 = { kSW_PAD + 320, 10, rcClient.right - kSW_PAD, kSW_HEADER_H };
        DrawTextW(hdc, hint, -1, &hr2,
                  DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
        SelectObject(hdc, old);
    }
    {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(40, 44, 68));
        HPEN old = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, 0, kSW_HEADER_H - 1, nullptr);
        LineTo(hdc, rcClient.right, kSW_HEADER_H - 1);
        SelectObject(hdc, old); DeleteObject(pen);
    }

    // --- Grid cells ---
    // Clip cell drawing to the scrollable area between header and footer.
    RECT clipArea = { 0, kSW_HEADER_H, rcClient.right,
                      rcClient.bottom - kSW_FOOTER_H };
    HRGN hClip = CreateRectRgnIndirect(&clipArea);
    SelectClipRgn(hdc, hClip);
    DeleteObject(hClip);

    for (int i = 0; i < (int)g_switcherItems.size(); ++i) {
        const auto &item = g_switcherItems[i];
        RECT cell  = SwCellRect(i);

        // Skip cells entirely outside the visible area.
        if (cell.bottom <= kSW_HEADER_H ||
            cell.top    >= rcClient.bottom - kSW_FOOTER_H)
            continue;

        RECT thumb = SwThumbRect(cell);
        RECT label = SwLabelRect(cell);
        bool sel   = (i == g_switcherSel);

        // Selection border fill (the kSW_BORDER gap around thumb)
        {
            COLORREF bc = sel ? RGB(80, 180, 255) : RGB(50, 55, 80);
            HBRUSH br = CreateSolidBrush(bc);
            FillRect(hdc, &cell, br);
            DeleteObject(br);
        }
        // Thumb area: buffer bitmap or dark placeholder (DWM composites over placeholder for AppTabs)
        if (item.kind == SwitcherItem::BUFFER &&
            i < (int)g_switcherBitmaps.size()) {
            // Lazy generation: create the bitmap the first time the cell is painted.
            if (!g_switcherBitmaps[i] && g_editor) {
                const auto &bufs = g_editor->GetBuffers();
                if (item.index >= 0 && item.index < (int)bufs.size())
                    g_switcherBitmaps[i] = SwRenderBufferBitmap(bufs[item.index].get());
            }
            if (g_switcherBitmaps[i]) {
                HDC hdcMem = CreateCompatibleDC(hdc);
                HBITMAP hOld2 = (HBITMAP)SelectObject(hdcMem, g_switcherBitmaps[i]);
                StretchBlt(hdc,
                           thumb.left, thumb.top,
                           thumb.right - thumb.left, thumb.bottom - thumb.top,
                           hdcMem, 0, 0, SwBmpW(), SwBmpH(), SRCCOPY);
                SelectObject(hdcMem, hOld2);
                DeleteDC(hdcMem);
            } else {
                HBRUSH br = CreateSolidBrush(RGB(10, 10, 18));
                FillRect(hdc, &thumb, br);
                DeleteObject(br);
            }
        } else {
            HBRUSH br = CreateSolidBrush(RGB(10, 10, 18));
            FillRect(hdc, &thumb, br);
            DeleteObject(br);
        }
        // Label strip background
        {
            COLORREF lbg = item.isTerminal ? RGB(18, 44, 22)
                         : (item.kind == SwitcherItem::APPTAB) ? RGB(22, 36, 60)
                         : RGB(28, 28, 46);
            HBRUSH br = CreateSolidBrush(lbg);
            FillRect(hdc, &label, br);
            DeleteObject(br);
        }
        // Badge
        {
            HFONT old = (HFONT)SelectObject(hdc, hSmall);
            const wchar_t *badge = item.isTerminal ? L"TERM"
                                 : (item.kind == SwitcherItem::APPTAB) ? L"APP"
                                 : L"BUF";
            COLORREF bc = item.isTerminal ? RGB(80, 200, 100)
                        : (item.kind == SwitcherItem::APPTAB) ? RGB(80, 140, 220)
                        : RGB(120, 120, 170);
            SetTextColor(hdc, bc);
            RECT br2 = { label.right - 44, label.top + 2,
                         label.right - 4,  label.bottom };
            DrawTextW(hdc, badge, -1, &br2,
                      DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
            SelectObject(hdc, old);
        }
        // Label text
        {
            HFONT old = (HFONT)SelectObject(hdc, hNormal);
            SetTextColor(hdc, sel ? RGB(255, 255, 255) : RGB(190, 200, 230));
            RECT lr = { label.left + 6, label.top,
                        label.right - 50, label.bottom };
            DrawTextW(hdc, item.label.c_str(), -1, &lr,
                      DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(hdc, old);
        }
    }

    // Empty state
    if (g_switcherItems.empty()) {
        SelectClipRgn(hdc, nullptr);
        HFONT old = (HFONT)SelectObject(hdc, hNormal);
        SetTextColor(hdc, RGB(80, 90, 120));
        RECT er = { kSW_PAD, kSW_HEADER_H + 40,
                    rcClient.right - kSW_PAD, kSW_HEADER_H + 100 };
        DrawTextW(hdc, L"No tabs to show.", -1, &er,
                  DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, old);
    }

    // Remove clip before drawing footer
    SelectClipRgn(hdc, nullptr);

    // --- Footer (fixed) ---
    {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(40, 44, 68));
        HPEN old = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, 0, rcClient.bottom - kSW_FOOTER_H, nullptr);
        LineTo(hdc, rcClient.right, rcClient.bottom - kSW_FOOTER_H);
        SelectObject(hdc, old); DeleteObject(pen);

        // Fill footer background
        RECT fr = { 0, rcClient.bottom - kSW_FOOTER_H,
                    rcClient.right, rcClient.bottom };
        HBRUSH br = CreateSolidBrush(RGB(22, 22, 34));
        FillRect(hdc, &fr, br);
        DeleteObject(br);

        HFONT old2 = (HFONT)SelectObject(hdc, hSmall);
        SetTextColor(hdc, RGB(70, 80, 110));
        RECT tr = { kSW_PAD, rcClient.bottom - kSW_FOOTER_H + 6,
                    rcClient.right - kSW_PAD, rcClient.bottom };
        std::wstring cnt = std::to_wstring(g_switcherItems.size())
                         + L" tab(s)   Ctrl+Shift+T";
        DrawTextW(hdc, cnt.c_str(), -1, &tr,
                  DT_SINGLELINE | DT_RIGHT | DT_NOPREFIX);
        SelectObject(hdc, old2);
    }

    DeleteObject(hBold);
    DeleteObject(hNormal);
    DeleteObject(hSmall);
    EndPaint(hwnd, &ps);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK TabSwitcherWndProc(HWND hwnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        RECT rc; GetClientRect(hwnd, &rc);
        SwRecalcCols(rc.right);
        SwRegisterThumbs(hwnd);
        SwUpdateAllThumbs(hwnd);
        SwUpdateScrollbar(hwnd);
        return 0;
    }

    case WM_SIZE: {
        int clientW = LOWORD(lParam);
        SwRecalcCols(clientW);
        SwUpdateScrollbar(hwnd);
        SwUpdateAllThumbs(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_VSCROLL: {
        SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        int newPos = si.nPos;
        switch (LOWORD(wParam)) {
        case SB_LINEUP:        newPos -= SwCellH() + kSW_PAD; break;
        case SB_LINEDOWN:      newPos += SwCellH() + kSW_PAD; break;
        case SB_PAGEUP:        newPos -= (int)si.nPage; break;
        case SB_PAGEDOWN:      newPos += (int)si.nPage; break;
        case SB_THUMBTRACK:    newPos = si.nTrackPos; break;
        case SB_THUMBPOSITION: newPos = HIWORD(wParam); break;
        }
        g_switcherScrollY = newPos;
        SwUpdateScrollbar(hwnd);
        SwUpdateAllThumbs(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        SwScrollBy(hwnd, -delta / WHEEL_DELTA * (SwCellH() + kSW_PAD));
        return 0;
    }

    case WM_PAINT:
        SwPaint(hwnd);
        return 0;

    case WM_KEYDOWN: {
        bool moved = false;
        switch (wParam) {
        case VK_ESCAPE:
            DestroyWindow(hwnd);
            return 0;
        case VK_RETURN: {
            HWND parent = GetParent(hwnd);
            SwUnregisterAllThumbs();
            SwActivateItem(parent);
            DestroyWindow(hwnd);
            return 0;
        }
        case VK_LEFT:
            if (g_switcherSel > 0) { --g_switcherSel; moved = true; }
            break;
        case VK_RIGHT:
            if (g_switcherSel < (int)g_switcherItems.size() - 1) {
                ++g_switcherSel; moved = true;
            }
            break;
        case VK_UP:
            if (g_switcherSel - g_switcherCols >= 0) {
                g_switcherSel -= g_switcherCols; moved = true;
            }
            break;
        case VK_DOWN:
            if (g_switcherSel + g_switcherCols < (int)g_switcherItems.size()) {
                g_switcherSel += g_switcherCols; moved = true;
            }
            break;
        case 'T':
            g_switcherTermOnly = !g_switcherTermOnly;
            g_switcherSel = 0;
            g_switcherScrollY = 0;
            SwRebuildItems();
            SwResetBitmaps();
            SwRegisterThumbs(hwnd);
            SwUpdateScrollbar(hwnd);
            SwUpdateAllThumbs(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (moved) {
            SwEnsureSelVisible(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        for (int i = 0; i < (int)g_switcherItems.size(); ++i) {
            RECT r = SwCellRect(i);
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                g_switcherSel = i;
                HWND parent = GetParent(hwnd);
                SwUnregisterAllThumbs();
                SwActivateItem(parent);
                DestroyWindow(hwnd);
                return 0;
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        for (int i = 0; i < (int)g_switcherItems.size(); ++i) {
            RECT r = SwCellRect(i);
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                if (g_switcherSel != i) {
                    g_switcherSel = i;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }
        }
        return 0;
    }

    case WM_DESTROY:
        SwUnregisterAllThumbs();
        SwDestroyBitmaps();
        g_switcherHwnd = nullptr;
        if (g_mainHwnd && IsWindow(g_mainHwnd))
            SetFocus(g_mainHwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool SwRegisterClass(HINSTANCE hInst) {
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = TabSwitcherWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = kTabSwitcherClass;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    return RegisterClassExW(&wc) != 0;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void ShowTabSwitcher(HWND parentHwnd) {
    if (g_switcherHwnd && IsWindow(g_switcherHwnd)) {
        SetForegroundWindow(g_switcherHwnd);
        return;
    }

    g_switcherTermOnly = false;
    g_switcherSel      = 0;
    g_switcherScrollY  = 0;
    g_switcherCols     = 4;
    SwRebuildItems();
    SwResetBitmaps();

    // Initial window size: 4 cols × up to 3 rows, then scroll.
    int initCols = 4;
    int initRows = (std::min)(3,
        (int)((g_switcherItems.size() + initCols - 1) / initCols));
    initRows = (std::max)(initRows, 1);

    int clientW = kSW_PAD + initCols * (SwCellW() + kSW_PAD) + kSW_PAD;
    int clientH = kSW_HEADER_H
                + kSW_PAD + initRows * (SwCellH() + kSW_PAD)
                + kSW_FOOTER_H;

    RECT adj = { 0, 0, clientW, clientH };
    AdjustWindowRect(&adj, WS_OVERLAPPEDWINDOW | WS_VSCROLL, FALSE);
    int winW = adj.right - adj.left;
    int winH = adj.bottom - adj.top;

    RECT pr = {};
    GetWindowRect(parentHwnd, &pr);
    int px = pr.left + (pr.right  - pr.left - winW) / 2;
    int py = pr.top  + (pr.bottom - pr.top  - winH) / 2;

    static bool classReady = false;
    if (!classReady) {
        SwRegisterClass(GetModuleHandleW(nullptr));
        classReady = true;
    }

    g_switcherHwnd = CreateWindowExW(
        0,
        kTabSwitcherClass,
        L"Tab Grid View",
        WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_VISIBLE,
        px, py, winW, winH,
        parentHwnd, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (g_switcherHwnd)
        SetFocus(g_switcherHwnd);
}

void HideTabSwitcher() {
    if (g_switcherHwnd && IsWindow(g_switcherHwnd))
        DestroyWindow(g_switcherHwnd);
}
