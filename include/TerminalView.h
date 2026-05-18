#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

#include "TerminalBuffer.h"
#include "TerminalEmulator.h"
#include "ConPtySession.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Posted by the ConPTY reader thread to deliver output on the UI thread.
#define WM_TERMINAL_OUTPUT (WM_USER + 200)

// ---------------------------------------------------------------------------
// TerminalView
//
// A self-contained Win32 child window that embeds a full terminal emulator.
// Rendering uses Direct2D / DirectWrite (matching Ecode's existing pipeline).
//
// Usage:
//   TerminalView* tv = new TerminalView();
//   HWND hwnd = tv->Create(parentHwnd);
//   tv->StartSession(L"powershell.exe", {});
// ---------------------------------------------------------------------------
class TerminalView {
public:
    TerminalView();
    ~TerminalView();

    // Create the child HWND (hidden by default).  Returns nullptr on failure.
    HWND Create(HWND parent);

    // Lazy-start the ConPTY session.  Safe to call multiple times.
    bool StartSession(
        const std::wstring& shell = L"powershell.exe",
        const std::vector<std::pair<std::wstring,std::wstring>>& extraEnv = {});

    bool IsStarted() const { return session_.IsRunning(); }
    HWND Hwnd()      const { return hwnd_; }

    // Called from parent's WM_SIZE handler.
    void MoveAndResize(int x, int y, int w, int h);

    // Send raw text/bytes to the PTY.
    void SendInput(const std::string& utf8);
    void SendInput(const std::wstring& text);

    // Window class name for RegisterClassEx
    static constexpr const wchar_t* kClassName = L"EcodeTerminalView";
    static bool RegisterWindowClass(HINSTANCE hInst);

private:
    // Win32 window procedure (dispatches to instance method)
    static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

    // Message handlers
    LRESULT OnCreate(HWND hwnd);
    void    OnDestroy();
    void    OnSize(int w, int h);
    void    OnPaint();
    void    OnKeyDown(WPARAM vk, LPARAM lParam);
    void    OnChar(wchar_t ch);
    void    OnMouseWheel(int delta);
    void    OnTerminalOutput(const char* data, size_t len);

    // Rendering
    bool EnsureRenderTarget();
    void ReleaseRenderTarget();
    void UpdateMetrics();
    void DrawCell(ID2D1RenderTarget* rt, int row, int col,
                  const TerminalCell& cell, bool isCursor, bool cursorVisible);
    ID2D1SolidColorBrush* GetBrush(const TermColor& c, float alpha = 1.0f);
    D2D1_COLOR_F ToD2DColor(const TermColor& c, float alpha = 1.0f);

    // VT key encoding
    std::string EncodeKey(WPARAM vk, bool ctrl, bool shift, bool alt);

    // Clipboard
    void PasteFromClipboard();
    void OnContextMenu(int screenX, int screenY);

    // Scrollback
    int  scrollOffset_ = 0;  // lines scrolled back (0 = bottom)
    void ScrollBy(int lines);
    int  VisibleRows() const;

    // Cursor blink
    void StartCursorTimer();
    void StopCursorTimer();

    // --- state ---
    HWND hwnd_ = nullptr;

    TerminalBuffer  buffer_;
    TerminalEmulator emulator_;
    ConPtySession   session_;

    // D2D / DWrite
    ID2D1Factory*          d2dFactory_  = nullptr;
    ID2D1HwndRenderTarget* renderTarget_= nullptr;
    IDWriteFactory*        dwFactory_   = nullptr;
    IDWriteTextFormat*     textFormat_  = nullptr;

    // cached brush (one at a time — recreated per-draw-call)
    ID2D1SolidColorBrush*  brush_       = nullptr;

    // Cell metrics
    float cellWidth_  = 8.0f;
    float cellHeight_ = 16.0f;
    float baseline_   = 12.0f;

    // Cursor blink
    bool   cursorBlink_   = false;  // current visible state
    UINT_PTR cursorTimer_ = 0;

    // Output mutex (reader thread → UI thread via PostMessage; no lock needed,
    // but we need one for the chunk pointer lifetime)
    std::mutex outputMutex_;

    // Pending hyperlink URL (for Ctrl+click)
    std::wstring pendingHyperlinkUrl_;
};
