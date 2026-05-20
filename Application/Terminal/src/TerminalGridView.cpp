#include "TerminalGridView.h"
#include <algorithm>
#include <cmath>
#include <windowsx.h>
#include <commctrl.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")

// ---------------------------------------------------------------------------
// TerminalTabInfo (mirrors Globals.inl)
// ---------------------------------------------------------------------------
struct TerminalTabInfo {
    TerminalView* view = nullptr;
    HWND hwnd = nullptr;
    std::wstring shell;
    std::wstring label;
    HANDLE hProcess = nullptr;
};

// ---------------------------------------------------------------------------
// Global externs (provided by main.cpp)
// ---------------------------------------------------------------------------
extern std::vector<TerminalTabInfo> g_terminalTabs;
extern int g_activeTerminalTab;
extern int g_activeAppTab;
extern HWND g_mainHwnd;
extern HWND g_tabHwnd;
extern bool g_suppressTabChange;
#include "../include/Editor.h"
extern Editor* g_editor;

extern void UpdateMenu(HWND hwnd);
extern void UpdateTabs(HWND hwnd);

struct AppTabInfo {
    HWND hwnd = nullptr;
    std::wstring label;
    int type = 0;
    void* data = nullptr;
    HANDLE hProcess = nullptr;
};
extern std::vector<AppTabInfo> g_appTabs;

#define WM_DEFERRED_FOCUS (WM_USER + 205)

// ---------------------------------------------------------------------------
// DWM thumbnail + snapshot helpers
// ---------------------------------------------------------------------------
HTHUMBNAIL TerminalGridView::RegisterChildThumb(HWND dest, HWND src) {
    LONG style    = GetWindowLong(src, GWL_STYLE);
    bool wasChild = (style & WS_CHILD) != 0;
    if (wasChild)
        SetWindowLong(src, GWL_STYLE, style & ~WS_CHILD);

    HTHUMBNAIL h  = nullptr;
    HRESULT    hr = DwmRegisterThumbnail(dest, src, &h);

    if (wasChild)
        SetWindowLong(src, GWL_STYLE, style);

    return SUCCEEDED(hr) ? h : nullptr;
}

void TerminalGridView::RegisterThumbnails() {
    UnregisterThumbnails();
    ReleaseSnapshots();

    int count = (int)g_terminalTabs.size();
    thumbnails_.resize(count, nullptr);
    snapshots_.resize(count, nullptr);
    snapshotD2D_.resize(count, nullptr);

    for (int i = 0; i < count; ++i) {
        HWND src = g_terminalTabs[i].hwnd;
        if (!src || !IsWindow(src)) continue;

        thumbnails_[i] = RegisterChildThumb(hwnd_, src);
        // Always capture a snapshot as fallback:
        //   - DWM succeeds but shows blank for hidden/GPU-rendered windows
        //   - DWM fails entirely (WS_CHILD stripping refused, etc.)
        snapshots_[i] = CaptureSnapshot(src);
    }
    UpdateThumbnailRects();
}

void TerminalGridView::UnregisterThumbnails() {
    for (auto& h : thumbnails_)
        if (h) { DwmUnregisterThumbnail(h); h = nullptr; }
    thumbnails_.clear();
}

void TerminalGridView::UpdateThumbnailRects() {
    for (int i = 0; i < (int)thumbnails_.size(); ++i) {
        if (!thumbnails_[i]) continue;
        D2D1_RECT_F cell;
        GetCellRect(i, cell);

        DWM_THUMBNAIL_PROPERTIES props = {};
        props.dwFlags               = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION
                                    | DWM_TNP_SOURCECLIENTAREAONLY | DWM_TNP_OPACITY;
        props.rcDestination         = { (LONG)(cell.left  + 4), (LONG)(cell.top    + 4),
                                        (LONG)(cell.right - 4), (LONG)(cell.bottom - 4) };
        props.fVisible              = TRUE;
        props.fSourceClientAreaOnly = TRUE;
        props.opacity               = 255;
        DwmUpdateThumbnailProperties(thumbnails_[i], &props);
    }
}

HBITMAP TerminalGridView::CaptureSnapshot(HWND src) {
    RECT rc;
    GetClientRect(src, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return nullptr;

    HDC     hdcScreen = GetDC(nullptr);
    HDC     hdcMem    = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp      = CreateCompatibleBitmap(hdcScreen, w, h);
    ReleaseDC(nullptr, hdcScreen);

    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBmp);
    // PW_RENDERFULLCONTENT captures D2D/DirectX content (Windows 8.1+).
    BOOL ok = PrintWindow(src, hdcMem, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);

    if (!ok) { DeleteObject(hBmp); return nullptr; }
    return hBmp;
}

ID2D1Bitmap* TerminalGridView::SnapshotToBitmap(HBITMAP hBmp) {
    if (!renderTarget_ || !hBmp) return nullptr;

    BITMAP bm = {};
    GetObject(hBmp, sizeof(bm), &bm);
    if (bm.bmWidth <= 0 || bm.bmHeight <= 0) return nullptr;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = bm.bmWidth;
    bmi.bmiHeader.biHeight      = -bm.bmHeight; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    int stride = bm.bmWidth * 4;
    std::vector<BYTE> pixels(stride * bm.bmHeight, 0xFF);

    HDC     hdcScreen = GetDC(nullptr);
    HDC     hdcMem    = CreateCompatibleDC(hdcScreen);
    HBITMAP hOld      = (HBITMAP)SelectObject(hdcMem, hBmp);
    GetDIBits(hdcMem, hBmp, 0, bm.bmHeight, pixels.data(), &bmi, DIB_RGB_COLORS);
    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    // GDI returns BGR with alpha=0; set opaque alpha
    for (int p = 0; p < bm.bmWidth * bm.bmHeight; ++p)
        pixels[p * 4 + 3] = 255;

    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ID2D1Bitmap* d2dBmp = nullptr;
    renderTarget_->CreateBitmap(
        D2D1::SizeU(bm.bmWidth, bm.bmHeight),
        pixels.data(), stride, props, &d2dBmp);
    return d2dBmp;
}

void TerminalGridView::ReleaseSnapshots() {
    for (auto& b : snapshotD2D_)
        if (b) { b->Release(); b = nullptr; }
    snapshotD2D_.clear();
    for (auto& b : snapshots_)
        if (b) { DeleteObject(b); b = nullptr; }
    snapshots_.clear();
}

// ---------------------------------------------------------------------------
// static registration
// ---------------------------------------------------------------------------
bool TerminalGridView::RegisterWindowClass(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = TerminalGridView::WndProcStatic;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    return RegisterClassExW(&wc) != 0;
}

// ---------------------------------------------------------------------------
// ctor / dtor
// ---------------------------------------------------------------------------
TerminalGridView::TerminalGridView() {
    bgColor_       = D2D1::ColorF(0x1E, 0x1E, 0x1E, 0.95f);
    cellBgColor_   = D2D1::ColorF(0x2D, 0x2D, 0x2D, 1.0f);
    borderColor_   = D2D1::ColorF(0x55, 0x55, 0x55, 1.0f);
    selBorderColor_= D2D1::ColorF(0x00, 0x78, 0xD4, 1.0f);
    textColor_     = D2D1::ColorF(0xCC, 0xCC, 0xCC, 1.0f);
    labelBgColor_  = D2D1::ColorF(0x22, 0x22, 0x22, 1.0f);
}

TerminalGridView::~TerminalGridView() {
    Hide();
    ReleaseRenderTarget();
    if (labelFormat_)    { labelFormat_->Release();    labelFormat_    = nullptr; }
    if (cellTextFormat_) { cellTextFormat_->Release(); cellTextFormat_ = nullptr; }
    if (dwFactory_)      { dwFactory_->Release();      dwFactory_      = nullptr; }
    if (d2dFactory_)     { d2dFactory_->Release();     d2dFactory_     = nullptr; }
}

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------
bool TerminalGridView::Create(HWND parent) {
    parent_ = parent;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kClassName, L"Terminal Grid View",
        WS_POPUP | WS_CLIPCHILDREN,
        0, 0, 100, 100,
        parent, nullptr,
        (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE),
        this);

    return hwnd_ != nullptr;
}

// ---------------------------------------------------------------------------
// Show / Hide
// ---------------------------------------------------------------------------
void TerminalGridView::Show() {
    if (!hwnd_ || visible_) return;
    if (g_terminalTabs.empty()) return;

    visible_ = true;
    selectedIndex_ = 0;
    ComputeGridLayout();

    // Position & size the window centered over the parent client area
    RECT parentRc;
    GetClientRect(parent_, &parentRc);
    MapWindowPoints(parent_, nullptr, (POINT*)&parentRc, 2);

    int winW = columns_ * kCellW + (columns_ - 1) * kPadding + 2 * kMargin;
    int winH = rows_ * (kCellH + kLabelH + kPadding) - kPadding + 2 * kMargin;
    int winX = parentRc.left + (parentRc.right - parentRc.left - winW) / 2;
    int winY = parentRc.top + (parentRc.bottom - parentRc.top - winH) / 3;

    SetWindowPos(hwnd_, HWND_TOPMOST, winX, winY, winW, winH,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetFocus(hwnd_);
    RegisterThumbnails();

    // Start refresh timer
    if (!refreshTimer_)
        refreshTimer_ = SetTimer(hwnd_, kTimerId, kTimerIntervalMs, nullptr);

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void TerminalGridView::Hide() {
    if (!visible_ || !hwnd_) return;
    visible_ = false;
    UnregisterThumbnails();
    ReleaseSnapshots();
    if (refreshTimer_) {
        KillTimer(hwnd_, refreshTimer_);
        refreshTimer_ = 0;
    }
    ShowWindow(hwnd_, SW_HIDE);
}

void TerminalGridView::Refresh() {
    if (visible_ && hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

// ---------------------------------------------------------------------------
// Compute grid layout
// ---------------------------------------------------------------------------
void TerminalGridView::ComputeGridLayout() {
    cellCount_ = (int)g_terminalTabs.size();
    if (cellCount_ <= 0) { columns_ = 1; rows_ = 1; return; }
    columns_ = std::max(1, (int)ceil(sqrt((double)cellCount_)));
    rows_ = (cellCount_ + columns_ - 1) / columns_;
}

void TerminalGridView::GetCellRect(int index, D2D1_RECT_F& outRect) const {
    if (index < 0 || index >= cellCount_) { outRect = {}; return; }
    int col = index % columns_;
    int row = index / columns_;
    float x = (float)(kMargin + col * (kCellW + kPadding));
    float y = (float)(kMargin + row * (kCellH + kLabelH + kPadding));
    outRect = D2D1::RectF(x, y, x + kCellW, y + kCellH);
}

int TerminalGridView::HitTestCell(int px, int py) const {
    for (int i = 0; i < cellCount_; ++i) {
        D2D1_RECT_F r;
        GetCellRect(i, r);
        if (px >= r.left && px <= r.right && py >= r.top && py <= r.bottom + kLabelH)
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// WndProc
// ---------------------------------------------------------------------------
LRESULT CALLBACK TerminalGridView::WndProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TerminalGridView* self = nullptr;
    if (msg == WM_CREATE) {
        auto* cs = (CREATESTRUCTW*)lp;
        self = (TerminalGridView*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = (TerminalGridView*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if (self) return self->WndProc(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT TerminalGridView::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:       return OnCreate(hwnd);
    case WM_DESTROY:      OnDestroy(); return 0;
    case WM_PAINT:        OnPaint(); return 0;
    case WM_ERASEBKGND:   return 1;
    case WM_KEYDOWN:      OnKeyDown(wp); return 0;
    case WM_LBUTTONDOWN:  OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_TIMER:
        if (wp == kTimerId) { OnTimer(); return 0; }
        break;
    case WM_KILLFOCUS:
        Dismiss(false);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// OnCreate
// ---------------------------------------------------------------------------
LRESULT TerminalGridView::OnCreate(HWND hwnd) {
    hwnd_ = hwnd;

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory_);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                        __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(&dwFactory_));

    // Label text format (terminal name below thumbnail)
    if (dwFactory_) {
        dwFactory_->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f * 96.0f / 72.0f,
            L"", &labelFormat_);
        if (labelFormat_) {
            labelFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            labelFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            labelFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }

        // Cell character text format (tiny chars for thumbnail content)
        dwFactory_->CreateTextFormat(
            L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            8.0f * 96.0f / 72.0f,
            L"", &cellTextFormat_);
        if (cellTextFormat_) {
            cellTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            cellTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            cellTextFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// OnDestroy
// ---------------------------------------------------------------------------
void TerminalGridView::OnDestroy() {
    if (refreshTimer_) { KillTimer(hwnd_, refreshTimer_); refreshTimer_ = 0; }
    UnregisterThumbnails();
    ReleaseSnapshots();
    if (labelFormat_)    { labelFormat_->Release();    labelFormat_    = nullptr; }
    if (cellTextFormat_) { cellTextFormat_->Release(); cellTextFormat_ = nullptr; }
    if (brush_)          { brush_->Release();          brush_          = nullptr; }
    ReleaseRenderTarget();
}

// ---------------------------------------------------------------------------
// EnsureRenderTarget / ReleaseRenderTarget
// ---------------------------------------------------------------------------
bool TerminalGridView::EnsureRenderTarget() {
    if (renderTarget_) return true;
    if (!d2dFactory_ || !hwnd_) return false;

    RECT rc; GetClientRect(hwnd_, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    HRESULT hr = d2dFactory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(hwnd_, size),
        &renderTarget_);
    return SUCCEEDED(hr);
}

void TerminalGridView::ReleaseRenderTarget() {
    // snapshotD2D_ bitmaps are tied to renderTarget_ — invalidate them on recreation
    for (auto& b : snapshotD2D_)
        if (b) { b->Release(); b = nullptr; }
    if (brush_)        { brush_->Release();        brush_        = nullptr; }
    if (renderTarget_) { renderTarget_->Release(); renderTarget_ = nullptr; }
}

// ---------------------------------------------------------------------------
// Brush helpers
// ---------------------------------------------------------------------------
ID2D1SolidColorBrush* TerminalGridView::GetBrush(const D2D1_COLOR_F& color) {
    if (!renderTarget_) return nullptr;
    if (brush_) { brush_->Release(); brush_ = nullptr; }
    renderTarget_->CreateSolidColorBrush(color, &brush_);
    return brush_;
}

void TerminalGridView::ReleaseBrush() {
    if (brush_) { brush_->Release(); brush_ = nullptr; }
}

// ---------------------------------------------------------------------------
// OnPaint
// ---------------------------------------------------------------------------
void TerminalGridView::OnPaint() {
    PAINTSTRUCT ps; BeginPaint(hwnd_, &ps);
    if (!EnsureRenderTarget()) { EndPaint(hwnd_, &ps); return; }

    ID2D1RenderTarget* rt = renderTarget_;
    rt->BeginDraw();

    // Background
    rt->Clear(bgColor_);

    if (!cellTextFormat_ || !labelFormat_) { rt->EndDraw(); EndPaint(hwnd_, &ps); return; }

    // Recompute grid in case terminals changed
    ComputeGridLayout();

    for (int i = 0; i < cellCount_; ++i) {
        D2D1_RECT_F cellRect;
        GetCellRect(i, cellRect);
        bool selected = (i == selectedIndex_);
        RenderThumbnail(rt, i, cellRect, selected);
    }

    HRESULT hr = rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) ReleaseRenderTarget();
    EndPaint(hwnd_, &ps);
}

// ---------------------------------------------------------------------------
// RenderThumbnail
// ---------------------------------------------------------------------------
void TerminalGridView::RenderThumbnail(ID2D1RenderTarget* rt, int termIndex,
                                        const D2D1_RECT_F& cellRect, bool selected) {
    if (termIndex >= (int)g_terminalTabs.size()) return;

    // Cell background
    if (auto* b = GetBrush(cellBgColor_))
        rt->FillRectangle(cellRect, b);

    D2D1_RECT_F thumbRect = D2D1::RectF(
        cellRect.left  + 4.0f, cellRect.top    + 4.0f,
        cellRect.right - 4.0f, cellRect.bottom - 4.0f);

    bool hasDwm  = termIndex < (int)thumbnails_.size() && thumbnails_[termIndex];
    bool hasSnap = termIndex < (int)snapshots_.size()  && snapshots_[termIndex];

    if (hasDwm) {
        // DWM composites the live thumbnail on top — paint a dark base it will cover
        if (auto* b = GetBrush(D2D1::ColorF(0.04f, 0.04f, 0.04f)))
            rt->FillRectangle(thumbRect, b);
    } else if (hasSnap) {
        // Lazy-create ID2D1Bitmap from the captured HBITMAP
        if (termIndex >= (int)snapshotD2D_.size())
            snapshotD2D_.resize(termIndex + 1, nullptr);
        if (!snapshotD2D_[termIndex])
            snapshotD2D_[termIndex] = SnapshotToBitmap(snapshots_[termIndex]);

        if (snapshotD2D_[termIndex])
            rt->DrawBitmap(snapshotD2D_[termIndex], thumbRect,
                           1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        else if (auto* b = GetBrush(cellBgColor_))
            rt->FillRectangle(thumbRect, b);
    } else {
        // No thumbnail, no snapshot — dark placeholder
        if (auto* b = GetBrush(D2D1::ColorF(0.04f, 0.04f, 0.04f)))
            rt->FillRectangle(thumbRect, b);
    }

    // Label strip below cell
    float labelTop = cellRect.bottom + 2.0f;
    D2D1_RECT_F labelRect = D2D1::RectF(
        cellRect.left - 2.0f, labelTop,
        cellRect.right + 2.0f, labelTop + kLabelH);

    if (auto* b = GetBrush(labelBgColor_))
        rt->FillRectangle(labelRect, b);

    std::wstring label = g_terminalTabs[termIndex].label;
    if (auto* b = GetBrush(textColor_)) {
        rt->DrawText(label.c_str(), (UINT32)label.size(),
                     labelFormat_, labelRect, b,
                     D2D1_DRAW_TEXT_OPTIONS_NONE,
                     DWRITE_MEASURING_MODE_NATURAL);
    }

    // Border (extends to include label area)
    D2D1_COLOR_F border = selected ? selBorderColor_ : borderColor_;
    float borderW = selected ? 2.5f : 1.0f;
    D2D1_RECT_F borderRect = D2D1::RectF(
        cellRect.left - 2.0f, cellRect.top,
        cellRect.right + 2.0f, labelTop + kLabelH);

    if (auto* b = GetBrush(border)) {
        rt->DrawRectangle(borderRect, b, borderW);
        if (selected)
            rt->DrawLine(
                D2D1::Point2F(borderRect.left,  borderRect.top),
                D2D1::Point2F(borderRect.right, borderRect.top),
                b, 3.0f);
    }
    ReleaseBrush();
}

// ---------------------------------------------------------------------------
// Keyboard navigation
// ---------------------------------------------------------------------------
void TerminalGridView::OnKeyDown(WPARAM vk) {
    switch (vk) {
    case VK_RIGHT:
        if (selectedIndex_ < cellCount_ - 1) {
            ++selectedIndex_;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        break;
    case VK_LEFT:
        if (selectedIndex_ > 0) {
            --selectedIndex_;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        break;
    case VK_DOWN:
        if (selectedIndex_ + columns_ < cellCount_) {
            selectedIndex_ += columns_;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        break;
    case VK_UP:
        if (selectedIndex_ - columns_ >= 0) {
            selectedIndex_ -= columns_;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        break;
    case VK_TAB:
        if (GetKeyState(VK_SHIFT) & 0x8000) {
            if (selectedIndex_ > 0) { --selectedIndex_; InvalidateRect(hwnd_, nullptr, FALSE); }
        } else {
            if (selectedIndex_ < cellCount_ - 1) { ++selectedIndex_; InvalidateRect(hwnd_, nullptr, FALSE); }
        }
        break;
    case VK_RETURN:
    case VK_SPACE:
        Dismiss(true);
        break;
    case VK_ESCAPE:
        Dismiss(false);
        break;
    case VK_HOME:
        selectedIndex_ = 0;
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case VK_END:
        selectedIndex_ = cellCount_ - 1;
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    }
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------
void TerminalGridView::OnLButtonDown(int px, int py) {
    int idx = HitTestCell(px, py);
    if (idx >= 0) {
        selectedIndex_ = idx;
        InvalidateRect(hwnd_, nullptr, FALSE);
        Dismiss(true);
    } else {
        Dismiss(false);
    }
}

// ---------------------------------------------------------------------------
// Timer - refresh thumbnails
// ---------------------------------------------------------------------------
void TerminalGridView::OnTimer() {
    if (visible_ && hwnd_) {
        // Update cell count in case terminals were added/removed
        if ((int)g_terminalTabs.size() != cellCount_) {
            ComputeGridLayout();
            RECT parentRc;
            GetClientRect(parent_, &parentRc);
            MapWindowPoints(parent_, nullptr, (POINT*)&parentRc, 2);
            int winW = columns_ * kCellW + (columns_ - 1) * kPadding + 2 * kMargin;
            int winH = rows_ * (kCellH + kLabelH + kPadding) - kPadding + 2 * kMargin;
            int winX = parentRc.left + (parentRc.right - parentRc.left - winW) / 2;
            int winY = parentRc.top + (parentRc.bottom - parentRc.top - winH) / 3;
            SetWindowPos(hwnd_, HWND_TOPMOST, winX, winY, winW, winH, SWP_NOACTIVATE);
            RegisterThumbnails();
        } else {
            UpdateThumbnailRects();
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

// ---------------------------------------------------------------------------
// Selection / Dismissal
// ---------------------------------------------------------------------------
void TerminalGridView::SelectTerminal(int index) {
    if (index < 0 || index >= (int)g_terminalTabs.size()) return;

    // Hide all terminal windows, show selected one
    for (size_t i = 0; i < g_terminalTabs.size(); ++i) {
        if (g_terminalTabs[i].hwnd)
            ShowWindow(g_terminalTabs[i].hwnd, (int)i == index ? SW_SHOW : SW_HIDE);
    }
    // Hide any app tabs
    for (auto& t : g_appTabs) {
        if (t.hwnd) ShowWindow(t.hwnd, SW_HIDE);
    }

    g_activeAppTab = -1;
    g_activeTerminalTab = index;
    ShowScrollBar(g_mainHwnd, SB_BOTH, FALSE);

    // Update tab selection
    size_t bufCount = g_editor ? g_editor->GetBuffers().size() : 0;

    g_suppressTabChange = true;
    if (g_tabHwnd) {
        int termStart = (int)bufCount + (int)g_appTabs.size();
        TabCtrl_SetCurSel(g_tabHwnd, termStart + index);
    }
    g_suppressTabChange = false;

    UpdateMenu(g_mainHwnd);
    UpdateTabs(g_mainHwnd);
    InvalidateRect(g_mainHwnd, nullptr, FALSE);

    // Focus the terminal
    if (g_terminalTabs[index].hwnd)
        PostMessage(g_mainHwnd, WM_DEFERRED_FOCUS,
                    (WPARAM)g_terminalTabs[index].hwnd, 0);
}

void TerminalGridView::Dismiss(bool select) {
    Hide();
    if (select) {
        SelectTerminal(selectedIndex_);
    } else {
        SetFocus(g_mainHwnd);
    }
}
