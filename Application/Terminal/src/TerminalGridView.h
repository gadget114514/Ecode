#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <dwrite.h>

#include "TerminalView.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// TerminalGridView
//
// A popup overlay window that shows all open terminals as live-rendered
// thumbnails in a grid (similar to Windows Alt+Tab task switcher).
// The user can navigate with arrow keys / mouse and select a terminal to
// switch to, or press Escape to dismiss.
// ---------------------------------------------------------------------------
class TerminalGridView {
public:
    TerminalGridView();
    ~TerminalGridView();

    bool Create(HWND parent);
    void Show();
    void Hide();
    bool IsVisible() const { return visible_; }
    HWND Hwnd() const { return hwnd_; }

    void Refresh();

    static constexpr const wchar_t* kClassName = L"EcodeTerminalGridView";
    static bool RegisterWindowClass(HINSTANCE hInst);

    // Layout constants
    static constexpr int kCellW      = 270;
    static constexpr int kCellH      = 200;
    static constexpr int kLabelH     = 28;
    static constexpr int kPadding    = 14;
    static constexpr int kMargin     = 24;
    static constexpr int kThumbCols  = 24;
    static constexpr int kThumbRows  = 10;

private:
    static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

    LRESULT OnCreate(HWND hwnd);
    void    OnDestroy();
    void    OnPaint();
    void    OnKeyDown(WPARAM vk);
    void    OnLButtonDown(int px, int py);
    void    OnTimer();

    bool EnsureRenderTarget();
    void ReleaseRenderTarget();

    void ComputeGridLayout();
    void GetCellRect(int index, D2D1_RECT_F& outRect) const;
    int  HitTestCell(int px, int py) const;

    void RenderThumbnail(ID2D1RenderTarget* rt, int termIndex,
                         const D2D1_RECT_F& cellRect, bool selected);

    // DWM thumbnail + snapshot fallback
    static HTHUMBNAIL RegisterChildThumb(HWND dest, HWND src);
    void RegisterThumbnails();
    void UnregisterThumbnails();
    void UpdateThumbnailRects();
    static HBITMAP CaptureSnapshot(HWND src);
    ID2D1Bitmap*   SnapshotToBitmap(HBITMAP hBmp);
    void           ReleaseSnapshots();

    void SelectTerminal(int index);
    void Dismiss(bool select);

    // helpers
    ID2D1SolidColorBrush* GetBrush(const D2D1_COLOR_F& color);
    void ReleaseBrush();

    // State
    HWND      hwnd_    = nullptr;
    HWND      parent_  = nullptr;
    bool      visible_ = false;
    int       selectedIndex_ = 0;

    // Grid layout (computed)
    int columns_ = 1;
    int rows_    = 1;
    int cellCount_ = 1;

    // D2D / DWrite
    ID2D1Factory*          d2dFactory_   = nullptr;
    ID2D1HwndRenderTarget* renderTarget_ = nullptr;
    IDWriteFactory*        dwFactory_    = nullptr;
    IDWriteTextFormat*     labelFormat_  = nullptr;
    IDWriteTextFormat*     cellTextFormat_ = nullptr;
    ID2D1SolidColorBrush*  brush_        = nullptr;

    // Colors
    D2D1_COLOR_F bgColor_;
    D2D1_COLOR_F cellBgColor_;
    D2D1_COLOR_F borderColor_;
    D2D1_COLOR_F selBorderColor_;
    D2D1_COLOR_F textColor_;
    D2D1_COLOR_F labelBgColor_;

    // Timer
    UINT_PTR refreshTimer_ = 0;
    static constexpr UINT_PTR kTimerId = 2;
    static constexpr int     kTimerIntervalMs = 300;

    // DWM thumbnails + snapshot fallback (parallel to g_terminalTabs)
    std::vector<HTHUMBNAIL>   thumbnails_;
    std::vector<HBITMAP>      snapshots_;
    std::vector<ID2D1Bitmap*> snapshotD2D_;
};
