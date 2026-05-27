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
#include "ImageManager.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Posted by the ConPTY reader thread to deliver output on the UI thread.
#define WM_TERMINAL_OUTPUT (WM_USER + 200)

// Sent to parent window to report taskbar progress (lParam = 0–10000, or -1 to clear).
#define WM_TERMINAL_PROGRESS (WM_USER + 299)

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
        UINT codePage = CP_UTF8,
        const std::vector<std::pair<std::wstring,std::wstring>>& extraEnv = {});

    bool IsStarted() const { return session_.IsRunning(); }
    HWND Hwnd()      const { return hwnd_; }
    HANDLE GetProcessHandle() const { return session_.GetProcessHandle(); }
    std::wstring LastError()  const { return session_.LastError(); }

    // Taskbar progress via ITaskbarList3 (requires CoInitializeEx).
    void setProgressCallback(std::function<void(float)> cb) {
        onProgress_ = std::move(cb);
        emulator_.setProgressCallback(onProgress_);
    }

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
                  const TerminalCell& cell, bool isCursor, bool cursorVisible,
                  bool isSelected = false);
    ID2D1SolidColorBrush* GetBrush(const TermColor& c, float alpha = 1.0f);
    D2D1_COLOR_F ToD2DColor(const TermColor& c, float alpha = 1.0f);

    // VT key encoding
    std::string EncodeKey(WPARAM vk, bool ctrl, bool shift, bool alt);

    // Clipboard
    void PasteFromClipboard();
    void OnContextMenu(int screenX, int screenY);

    // Mouse selection
    void OnLButtonDown(int px, int py);
    void OnMouseMove(int px, int py);
    void OnLButtonUp();
    void BufferCoordFromPoint(int px, int py, int& outRow, int& outCol) const;
    bool IsSelected(int logRow, int col) const;
    void CopySelectionToClipboard() const;

    // Mouse
    void SendMouseWheelToPty(int direction, int px, int py); // direction: +1=up -1=down
    void SendMouseClickToPty(int button, int px, int py, bool release);

    // Scrollback
    static constexpr int kScrollBarWidth = 8;
    int  scrollOffset_ = 0;  // lines scrolled back (0 = bottom)
    int  maxScrollOffset_ = 0; // cached historyLineCount at last paint
    bool scrolledBack_ = false; // true when scrollOffset_ > 0
    void ScrollBy(int lines);
    void ScrollToBottom();
    int  VisibleRows() const;
    void DrawScrollBar(ID2D1RenderTarget* rt);
    void DrawScrollIndicator(ID2D1RenderTarget* rt, const RECT& clientRc);

    // Cursor blink
    void StartCursorTimer();
    void StopCursorTimer();

    // --- OSC 1337 image handlers ---
    void OnImageData(const std::vector<uint8_t>& data,
                     int widthPx, int heightPx,
                     bool preserveAspectRatio,
                     const std::wstring& name);
    void OnFileDownload(const std::vector<uint8_t>& data,
                        const std::wstring& name);

    // --- Sixel handler ---
    void OnSixelData(const std::vector<uint8_t>& data);

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
    bool   cursorBlink_   = true;   // current visible state (true = visible; starts visible so cursor shows at startup)
    UINT_PTR cursorTimer_ = 0;

    // Output mutex (reader thread → UI thread via PostMessage; no lock needed,
    // but we need one for the chunk pointer lifetime)
    std::mutex outputMutex_;

    // Code page for MultiByteToWideChar/WideCharToMultiByte (from settings.ini / --codepage)
    UINT codePage_ = CP_UTF8;

    // Hyperlink config (from settings.ini [Terminal] section)
    bool clickToOpenHyperlink_ = true;
    int  hyperlinkModifier_    = 3;     // 0=Ctrl, 1=Alt, 2=Shift, 3=no modifier
    std::wstring hyperlinkOpenCommand_; // empty = ShellExecuteW, else {url} template

    // Tooltip for hyperlinks
    HWND tooltip_ = nullptr;
    bool tooltipActive_ = false;
    std::wstring tooltipText_;

    // Is there a hyperlink at the given client-area point?
    bool IsHyperlinkAt(int px, int py) const;

    // Selection state
    bool selecting_    = false;
    int  selAnchorRow_ = -1;
    int  selAnchorCol_ = -1;
    int  selEndRow_    = -1;
    int  selEndCol_    = -1;

    // IME インライン入力
    std::wstring     imeComposition_;   // 変換中の文字列（空 = 非アクティブ）
    std::vector<BYTE> imeCompAttr_;     // 各文字の属性（ATTR_INPUT/ATTR_CONVERTED）
    bool             imeActive_ = false;
    int              imeShift_ = 0;     // 編集中の右シフト量（セル単位）

    // OSC 1337 image manager
    ImageManager* imageManager_ = nullptr;

    // Progress callback (OSC 9;4)
    std::function<void(float)> onProgress_;

    // Partial multi-byte sequence buffer (跨ぐチャンク間の不完全なマルチバイトシーケンス)
    std::string pendingPartial_;

};
