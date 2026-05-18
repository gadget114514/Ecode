#include "../include/TerminalView.h"
#include <algorithm>
#include <cstring>
#include <shellapi.h>   // ShellExecuteW for hyperlink open
#include <windowsx.h>  // GET_X_LPARAM / GET_Y_LPARAM
#include <string>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shell32.lib")

// ---------------------------------------------------------------------------
// static registration
// ---------------------------------------------------------------------------
bool TerminalView::RegisterWindowClass(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = TerminalView::WndProcStatic;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_IBEAM);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    return RegisterClassExW(&wc) != 0;
}

// ---------------------------------------------------------------------------
// ctor / dtor
// ---------------------------------------------------------------------------
TerminalView::TerminalView() {
    emulator_.reset(&buffer_);

    // Response callback: PTY → emulator response → PTY
    emulator_.setResponseCallback([this](const std::wstring& resp) {
        session_.Write(resp);
    });
    // Title callback
    emulator_.setTitleCallback([this](const std::wstring& title) {
        if (hwnd_) SetWindowTextW(GetParent(hwnd_), title.c_str());
    });
    // Clipboard callback (OSC 52)
    emulator_.setClipboardCallback([this](const std::wstring& text) {
        if (!OpenClipboard(hwnd_)) return;
        EmptyClipboard();
        size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hg) {
            memcpy(GlobalLock(hg), text.c_str(), bytes);
            GlobalUnlock(hg);
            SetClipboardData(CF_UNICODETEXT, hg);
        }
        CloseClipboard();
    });
    // Hyperlink open
    emulator_.setHyperlinkOpenCallback([](const std::wstring& url) {
        ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    });
}

TerminalView::~TerminalView() {
    StopCursorTimer();
    session_.Close();
    ReleaseRenderTarget();
    if (dwFactory_)  { dwFactory_->Release();  dwFactory_  = nullptr; }
    if (d2dFactory_) { d2dFactory_->Release(); d2dFactory_ = nullptr; }
}

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------
HWND TerminalView::Create(HWND parent) {
    hwnd_ = CreateWindowExW(
        0, kClassName, L"",
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 100, 100,
        parent, nullptr,
        (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE),
        this);
    return hwnd_;
}

// ---------------------------------------------------------------------------
// StartSession
// ---------------------------------------------------------------------------
bool TerminalView::StartSession(const std::wstring& shell,
                                const std::vector<std::pair<std::wstring,std::wstring>>& extraEnv) {
    if (session_.IsRunning()) return true;

    // Initial size from window, or reasonable default
    int cols = (int)(buffer_.columns());
    int rows = (int)(buffer_.rows());
    if (hwnd_) {
        RECT rc; GetClientRect(hwnd_, &rc);
        if (cellWidth_  > 0) cols = std::max(10, (int)((rc.right  - rc.left) / cellWidth_));
        if (cellHeight_ > 0) rows = std::max(3,  (int)((rc.bottom - rc.top)  / cellHeight_));
    }
    buffer_.resize(cols, rows);

    session_.SetOutputCallback([this](const char* data, size_t len) {
        // Heap-copy the chunk so we can safely PostMessage it
        char* copy = new char[len];
        memcpy(copy, data, len);
        PostMessage(hwnd_, WM_TERMINAL_OUTPUT,
                    (WPARAM)copy, (LPARAM)len);
    });

    return session_.Start(shell, cols, rows, extraEnv);
}

// ---------------------------------------------------------------------------
// MoveAndResize
// ---------------------------------------------------------------------------
void TerminalView::MoveAndResize(int x, int y, int w, int h) {
    if (hwnd_) MoveWindow(hwnd_, x, y, w, h, TRUE);
}

// ---------------------------------------------------------------------------
// SendInput
// ---------------------------------------------------------------------------
void TerminalView::SendInput(const std::string& utf8) {
    session_.Write(utf8.data(), utf8.size());
}

void TerminalView::SendInput(const std::wstring& text) {
    session_.Write(text);
}

// ---------------------------------------------------------------------------
// WndProc (static trampoline)
// ---------------------------------------------------------------------------
LRESULT CALLBACK TerminalView::WndProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TerminalView* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lp;
        self = (TerminalView*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = (TerminalView*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if (self) return self->WndProc(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT TerminalView::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:       return OnCreate(hwnd);
    case WM_DESTROY:      OnDestroy(); return 0;
    case WM_SIZE:         OnSize(LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT:        OnPaint(); return 0;
    case WM_ERASEBKGND:   return 1; // handled in OnPaint
    case WM_KEYDOWN:      OnKeyDown(wp, lp); return 0;
    case WM_CHAR:         OnChar((wchar_t)wp); return 0;
    case WM_MOUSEWHEEL:
        ScrollBy(-(GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA) * 3);
        return 0;
    case WM_LBUTTONDOWN:
        OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONUP:
        OnLButtonUp();
        return 0;
    case WM_SETFOCUS:
        // Notify PTY of focus (if focus events enabled)
        if (buffer_.focusEventReportingEnabled())
            session_.Write("\x1b[I", 3);
        StartCursorTimer();
        return 0;
    case WM_KILLFOCUS:
        if (buffer_.focusEventReportingEnabled())
            session_.Write("\x1b[O", 3);
        StopCursorTimer();
        cursorBlink_ = true;
        return 0;
    case WM_CONTEXTMENU:
        OnContextMenu(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == 1 /* ID_TERMINAL_PASTE */) { PasteFromClipboard(); return 0; }
        if (LOWORD(wp) == 2 /* ID_TERMINAL_COPY */)  { CopySelectionToClipboard(); return 0; }
        break;
    case WM_TERMINAL_OUTPUT: {
        const char* data = (const char*)wp;
        size_t       len  = (size_t)lp;
        OnTerminalOutput(data, len);
        delete[] data;
        return 0;
    }
    case WM_TIMER:
        if (wp == cursorTimer_) {
            cursorBlink_ = !cursorBlink_;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// OnCreate
// ---------------------------------------------------------------------------
LRESULT TerminalView::OnCreate(HWND hwnd) {
    hwnd_ = hwnd;

    // D2D factory
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory_);

    // DWrite factory
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                        __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(&dwFactory_));

    // Text format — prefer Cascadia Mono, fall back to Consolas
    if (dwFactory_) {
        HRESULT hr = dwFactory_->CreateTextFormat(
            L"Cascadia Mono", nullptr,
            DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f * 96.0f / 72.0f,   // pt → dip
            L"", &textFormat_);
        if (FAILED(hr)) {
            dwFactory_->CreateTextFormat(
                L"Consolas", nullptr,
                DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                11.0f * 96.0f / 72.0f,
                L"", &textFormat_);
        }
        if (textFormat_) {
            textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    }

    UpdateMetrics();
    return 0;
}

// ---------------------------------------------------------------------------
// OnDestroy
// ---------------------------------------------------------------------------
void TerminalView::OnDestroy() {
    StopCursorTimer();
    if (textFormat_) { textFormat_->Release(); textFormat_ = nullptr; }
    if (brush_)      { brush_->Release();      brush_      = nullptr; }
    ReleaseRenderTarget();
}

// ---------------------------------------------------------------------------
// UpdateMetrics — measure one character cell
// ---------------------------------------------------------------------------
void TerminalView::UpdateMetrics() {
    if (!dwFactory_ || !textFormat_) return;

    IDWriteTextLayout* layout = nullptr;
    dwFactory_->CreateTextLayout(L"M", 1, textFormat_, 1000, 1000, &layout);
    if (layout) {
        DWRITE_TEXT_METRICS m{};
        layout->GetMetrics(&m);
        cellWidth_  = m.width;
        cellHeight_ = m.height;

        DWRITE_LINE_METRICS lm{};
        UINT32 lc = 1;
        layout->GetLineMetrics(&lm, 1, &lc);
        baseline_ = lm.baseline;

        layout->Release();
    }
    buffer_.setCellSize((int)cellWidth_, (int)cellHeight_);
}

// ---------------------------------------------------------------------------
// OnSize
// ---------------------------------------------------------------------------
void TerminalView::OnSize(int w, int h) {
    ReleaseRenderTarget(); // recreate at new size
    if (w <= 0 || h <= 0 || cellWidth_ <= 0 || cellHeight_ <= 0) return;

    int cols = std::max(10, (int)(w / cellWidth_));
    int rows = std::max(3,  (int)(h / cellHeight_));

    if (cols != buffer_.columns() || rows != buffer_.rows()) {
        buffer_.resize(cols, rows);
        if (session_.IsRunning())
            session_.Resize(cols, rows);
    }
    scrollOffset_ = 0;
}

// ---------------------------------------------------------------------------
// OnTerminalOutput — called on UI thread via PostMessage
// ---------------------------------------------------------------------------
void TerminalView::OnTerminalOutput(const char* data, size_t len) {
    // Decode UTF-8 → UTF-16
    int needed = MultiByteToWideChar(CP_UTF8, 0, data, (int)len, nullptr, 0);
    if (needed <= 0) return;
    std::wstring ws(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, data, (int)len, ws.data(), needed);

    emulator_.process(ws);
    scrollOffset_ = 0; // snap to bottom on new output
    InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// EnsureRenderTarget / ReleaseRenderTarget
// ---------------------------------------------------------------------------
bool TerminalView::EnsureRenderTarget() {
    if (renderTarget_) return true;
    if (!d2dFactory_ || !hwnd_) return false;

    RECT rc; GetClientRect(hwnd_, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    HRESULT hr = d2dFactory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, size),
        &renderTarget_);
    return SUCCEEDED(hr);
}

void TerminalView::ReleaseRenderTarget() {
    if (brush_)        { brush_->Release();        brush_       = nullptr; }
    if (renderTarget_) { renderTarget_->Release(); renderTarget_= nullptr; }
}

// ---------------------------------------------------------------------------
// Brush helper
// ---------------------------------------------------------------------------
D2D1_COLOR_F TerminalView::ToD2DColor(const TermColor& c, float alpha) {
    return D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, alpha);
}

ID2D1SolidColorBrush* TerminalView::GetBrush(const TermColor& c, float alpha) {
    if (brush_) { brush_->Release(); brush_ = nullptr; }
    if (renderTarget_)
        renderTarget_->CreateSolidColorBrush(ToD2DColor(c, alpha), &brush_);
    return brush_;
}

// ---------------------------------------------------------------------------
// OnPaint
// ---------------------------------------------------------------------------
void TerminalView::OnPaint() {
    PAINTSTRUCT ps; BeginPaint(hwnd_, &ps);

    if (!EnsureRenderTarget()) { EndPaint(hwnd_, &ps); return; }

    ID2D1RenderTarget* rt = renderTarget_;
    rt->BeginDraw();

    // Background
    rt->Clear(ToD2DColor(DefaultBg()));

    if (!textFormat_) { rt->EndDraw(); EndPaint(hwnd_, &ps); return; }

    RECT clientRc; GetClientRect(hwnd_, &clientRc);
    const int visRows  = (int)((clientRc.bottom - clientRc.top) / cellHeight_);
    const int totalLog = buffer_.totalLineCount();
    const int histLines= buffer_.historyLineCount();

    // Determine first logical row to display
    // scrollOffset_ == 0 means bottom (newest)
    const int firstRow = std::max(0, totalLog - visRows - scrollOffset_);

    const int curRow = buffer_.cursorRow() + histLines;  // logical row of cursor

    for (int screenRow = 0; screenRow < visRows; ++screenRow) {
        int logRow = firstRow + screenRow;
        if (logRow >= totalLog) break;
        const auto& line = buffer_.lineAt(logRow);

        for (int col = 0; col < buffer_.columns(); ++col) {
            if (col >= (int)line.size()) break;
            const TerminalCell& cell = line[col];
            if (cell.wideContinuation) continue;

            bool isCursor = (logRow == curRow && col == buffer_.cursorColumn()
                             && scrollOffset_ == 0);
            bool isSelected = IsSelected(logRow, col);
            DrawCell(rt, screenRow, col, cell, isCursor, cursorBlink_, isSelected);
        }
    }

    HRESULT hr = rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) ReleaseRenderTarget();
    EndPaint(hwnd_, &ps);
}

void TerminalView::DrawCell(ID2D1RenderTarget* rt, int row, int col,
                            const TerminalCell& cell, bool isCursor, bool cursorVisible,
                            bool isSelected) {
    const float x = col * cellWidth_;
    const float y = row * cellHeight_;
    const float w = cellWidth_ * (cell.wide ? 2.0f : 1.0f);
    const float h = cellHeight_;

    TermColor fg = cell.foreground.isDefault ? DefaultFg() : cell.foreground;
    TermColor bg = cell.background.isDefault ? DefaultBg() : cell.background;
    TermColor dc = cell.decorColor.isDefault ? fg : cell.decorColor;

    // Apply inverse (reverse video) at render time — swap fg/bg
    if (cell.inverse) std::swap(fg, bg);

    // Draw background first
    if (!bg.isDefault || isCursor) {
        TermColor drawBg = isCursor ? fg : bg;
        if (auto* b = GetBrush(drawBg))
            rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), b);
    }

    // Selection overlay (drawn after background so it's visible on any cell bg)
    if (isSelected) {
        TermColor selColor;
        selColor.r = 80; selColor.g = 130; selColor.b = 220; selColor.isDefault = false;
        if (auto* b = GetBrush(selColor, 0.4f))
            rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), b);
        // Lighten foreground for readability on selection
        fg.r = (uint8_t)std::min(255, (int)fg.r + 60);
        fg.g = (uint8_t)std::min(255, (int)fg.g + 60);
        fg.b = (uint8_t)std::min(255, (int)fg.b + 60);
    }

    // Draw text
    if (!cell.text.empty() && cell.text != L" ") {
        TermColor drawFg = isCursor ? bg : fg;
        if (auto* b = GetBrush(drawFg)) {
            D2D1_RECT_F rect = D2D1::RectF(x, y, x + w, y + h);
            rt->DrawText(cell.text.c_str(), (UINT32)cell.text.size(),
                         textFormat_, rect, b,
                         D2D1_DRAW_TEXT_OPTIONS_NONE,
                         DWRITE_MEASURING_MODE_NATURAL);
        }
    }

    // Underline
    if (cell.underline) {
        if (auto* b = GetBrush(dc)) {
            float uy = y + baseline_ + 1.5f;
            rt->DrawLine({x, uy}, {x + w, uy}, b, 1.0f);
        }
    }

    // Strikethrough
    if (cell.strikethrough) {
        if (auto* b = GetBrush(dc)) {
            float sy = y + baseline_ * 0.5f;
            rt->DrawLine({x, sy}, {x + w, sy}, b, 1.0f);
        }
    }

    // Cursor outline (when not blinking-invisible)
    if (isCursor && cursorVisible) {
        if (buffer_.cursorShape() == TerminalBuffer::CursorShape::Underline) {
            if (auto* b = GetBrush(fg))
                rt->DrawLine({x, y + h - 2}, {x + cellWidth_, y + h - 2}, b, 2.0f);
        } else if (buffer_.cursorShape() == TerminalBuffer::CursorShape::Bar) {
            if (auto* b = GetBrush(fg))
                rt->DrawLine({x, y}, {x, y + h}, b, 2.0f);
        }
    }
}

// ---------------------------------------------------------------------------
// Mouse selection
// ---------------------------------------------------------------------------
void TerminalView::BufferCoordFromPoint(int px, int py, int& outRow, int& outCol) const {
    RECT clientRc; GetClientRect(hwnd_, &clientRc);
    const int visRows  = (int)((clientRc.bottom - clientRc.top) / cellHeight_);
    const int totalLog = buffer_.totalLineCount();
    const int histLines= buffer_.historyLineCount();
    const int firstRow = std::max(0, totalLog - visRows - scrollOffset_);

    outCol = (int)(px / cellWidth_);
    outCol = std::max(0, std::min(outCol, buffer_.columns() - 1));

    int screenRow = (int)(py / cellHeight_);
    screenRow = std::max(0, std::min(screenRow, visRows - 1));
    outRow = firstRow + screenRow;
    if (outRow >= totalLog) outRow = totalLog - 1;
}

bool TerminalView::IsSelected(int logRow, int col) const {
    if (!selecting_ && selAnchorRow_ < 0) return false;
    int ar = selAnchorRow_, ac = selAnchorCol_;
    int er = selEndRow_,    ec = selEndCol_;
    if (!selecting_) { er = ar; ec = ac; }
    int r1 = std::min(ar, er), r2 = std::max(ar, er);
    int c1 = std::min(ac, ec), c2 = std::max(ac, ec);
    return logRow >= r1 && logRow <= r2 && col >= c1 && col <= c2;
}

void TerminalView::OnLButtonDown(int px, int py) {
    // Click clears existing selection
    selAnchorRow_ = -1;
    selAnchorCol_ = -1;
    selEndRow_ = -1;
    selEndCol_ = -1;

    SetCapture(hwnd_);
    selecting_ = true;
    BufferCoordFromPoint(px, py, selAnchorRow_, selAnchorCol_);
    selEndRow_ = selAnchorRow_;
    selEndCol_ = selAnchorCol_;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void TerminalView::OnMouseMove(int px, int py) {
    if (!selecting_) return;
    BufferCoordFromPoint(px, py, selEndRow_, selEndCol_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void TerminalView::OnLButtonUp() {
    selecting_ = false;
    ReleaseCapture();
    // selection remains active until next click or copy
}

void TerminalView::CopySelectionToClipboard() const {
    if (selAnchorRow_ < 0) return;
    int ar = selAnchorRow_, ac = selAnchorCol_;
    int er = selEndRow_,    ec = selEndCol_;
    int r1 = std::min(ar, er), r2 = std::max(ar, er);
    int c1 = std::min(ac, ec), c2 = std::max(ac, ec);

    std::wstring wtext;
    for (int r = r1; r <= r2; ++r) {
        const auto& line = buffer_.lineAt(r);
        int startCol = (r == r1) ? c1 : 0;
        int endCol   = (r == r2) ? c2 : (buffer_.columns() - 1);
        for (int c = startCol; c <= endCol && c < (int)line.size(); ++c) {
            if (line[c].wideContinuation) continue;
            wtext += line[c].text;
        }
        if (r < r2) wtext += L"\r\n";
    }

    if (wtext.empty()) return;
    if (!OpenClipboard(const_cast<HWND>(hwnd_))) return;
    EmptyClipboard();
    size_t bytes = (wtext.size() + 1) * sizeof(wchar_t);
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hg) {
        memcpy(GlobalLock(hg), wtext.c_str(), bytes);
        GlobalUnlock(hg);
        SetClipboardData(CF_UNICODETEXT, hg);
    }
    CloseClipboard();
}

// ---------------------------------------------------------------------------
// Scrollback
// ---------------------------------------------------------------------------
int TerminalView::VisibleRows() const {
    if (cellHeight_ <= 0 || !hwnd_) return buffer_.rows();
    RECT rc; GetClientRect(hwnd_, &rc);
    return std::max(1, (int)((rc.bottom - rc.top) / cellHeight_));
}

void TerminalView::ScrollBy(int lines) {
    const int maxScroll = std::max(0, buffer_.historyLineCount());
    scrollOffset_ = std::max(0, std::min(scrollOffset_ + lines, maxScroll));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// Cursor blink timer
// ---------------------------------------------------------------------------
void TerminalView::StartCursorTimer() {
    if (!cursorTimer_)
        cursorTimer_ = SetTimer(hwnd_, 1, 530, nullptr);
}

void TerminalView::StopCursorTimer() {
    if (cursorTimer_) { KillTimer(hwnd_, cursorTimer_); cursorTimer_ = 0; }
}

// ---------------------------------------------------------------------------
// Clipboard
// ---------------------------------------------------------------------------
void TerminalView::PasteFromClipboard() {
    if (!OpenClipboard(hwnd_)) return;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        const wchar_t* text = (const wchar_t*)GlobalLock(hData);
        if (text) {
            // Convert UTF-16 → UTF-8 and send to PTY
            int need = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (need > 1) { // need includes null terminator
                std::string utf8(need - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), need - 1, nullptr, nullptr);
                // Replace bare \n with \r (terminal convention)
                std::string out;
                out.reserve(utf8.size());
                for (size_t i = 0; i < utf8.size(); ++i) {
                    if (utf8[i] == '\n' && (i == 0 || utf8[i-1] != '\r'))
                        out += '\r';
                    else
                        out += utf8[i];
                }
                scrollOffset_ = 0;
                session_.Write(out.data(), out.size());
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
}

void TerminalView::OnContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    bool hasSel = selAnchorRow_ >= 0;
    AppendMenuW(hMenu, MF_STRING | (hasSel ? 0 : MF_GRAYED), 2, L"Copy");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    bool canPaste = IsClipboardFormatAvailable(CF_UNICODETEXT);
    AppendMenuW(hMenu, MF_STRING | (canPaste ? 0 : MF_GRAYED), 1, L"Paste");

    UINT cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_RETURNCMD,
                              screenX, screenY, 0, hwnd_, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 1) { PasteFromClipboard(); }
    else if (cmd == 2) { CopySelectionToClipboard(); }
}

// ---------------------------------------------------------------------------
// Keyboard input
// ---------------------------------------------------------------------------
void TerminalView::OnKeyDown(WPARAM vk, LPARAM /*lParam*/) {
    bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
    bool alt   = (GetKeyState(VK_MENU)    & 0x8000) != 0;

    // Ctrl+C / Ctrl+Insert: copy selection to clipboard
    if ((ctrl && vk == 'C') || (ctrl && vk == VK_INSERT)) {
        if (selAnchorRow_ >= 0) {
            CopySelectionToClipboard();
            return;
        }
    }
    // Ctrl+Shift+C: also copy
    if (ctrl && shift && vk == 'C') {
        if (selAnchorRow_ >= 0) {
            CopySelectionToClipboard();
            return;
        }
    }

    // Clear selection on any typing
    bool clearSel = false;
    if (vk >= 0x20 && vk <= 0xFE) clearSel = true;
    else if (vk == VK_RETURN || vk == VK_BACK || vk == VK_TAB || vk == VK_ESCAPE) clearSel = true;
    else if (vk == VK_DELETE) clearSel = true;

    std::string seq = EncodeKey(vk, ctrl, shift, alt);
    if (!seq.empty()) {
        if (clearSel && selAnchorRow_ >= 0) {
            selAnchorRow_ = -1; selAnchorCol_ = -1;
            selEndRow_ = -1; selEndCol_ = -1;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        scrollOffset_ = 0; // snap to bottom on any keystroke
        session_.Write(seq.data(), seq.size());
    }
}

void TerminalView::OnChar(wchar_t ch) {
    // Ignore control characters already handled by OnKeyDown
    if (ch < 0x20 || ch == 0x7f) return;

    // Clear selection on character input
    if (selAnchorRow_ >= 0) {
        selAnchorRow_ = -1; selAnchorCol_ = -1;
        selEndRow_ = -1; selEndCol_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    scrollOffset_ = 0;

    // UTF-16 → UTF-8
    wchar_t ws[3] = { ch, 0, 0 };
    int need = WideCharToMultiByte(CP_UTF8, 0, ws, 1, nullptr, 0, nullptr, nullptr);
    if (need > 0) {
        std::string utf8(need, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws, 1, utf8.data(), need, nullptr, nullptr);
        session_.Write(utf8.data(), utf8.size());
    }
}

// ---------------------------------------------------------------------------
// EncodeKey — WM_KEYDOWN → VT escape sequence
// Application cursor mode is respected via buffer_.applicationCursorMode()
// ---------------------------------------------------------------------------
std::string TerminalView::EncodeKey(WPARAM vk, bool ctrl, bool shift, bool alt) {
    const bool appCursor = buffer_.applicationCursorMode();

    // Ctrl + letter
    if (ctrl && !alt) {
        if (vk >= 'A' && vk <= 'Z') {
            char c = (char)(vk - 'A' + 1);
            return std::string(1, c);
        }
        switch (vk) {
        case VK_SPACE:  return "\x00"; // Ctrl+Space = NUL
        case VK_OEM_4:  return "\x1b"; // Ctrl+[ = ESC
        case VK_OEM_5:  return "\x1c"; // Ctrl+\ = FS
        case VK_OEM_6:  return "\x1d"; // Ctrl+] = GS
        case VK_OEM_3:  return "\x1e"; // Ctrl+` = RS (approx)
        case VK_DELETE: return "\x1b[3;5~";
        }
    }

    // Alt prefix
    std::string altPfx = alt ? "\x1b" : "";

    switch (vk) {
    // --- cursor keys ---
    case VK_UP:     return altPfx + (appCursor ? "\x1bOA" : "\x1b[A");
    case VK_DOWN:   return altPfx + (appCursor ? "\x1bOB" : "\x1b[B");
    case VK_RIGHT:  return altPfx + (appCursor ? "\x1bOC" : "\x1b[C");
    case VK_LEFT:   return altPfx + (appCursor ? "\x1bOD" : "\x1b[D");
    // --- special keys ---
    case VK_HOME:   return shift ? "\x1b[1;2H" : "\x1b[H";
    case VK_END:    return shift ? "\x1b[1;2F" : "\x1b[F";
    case VK_INSERT: return "\x1b[2~";
    case VK_DELETE: return "\x1b[3~";
    case VK_PRIOR:  return shift ? "\x1b[5;2~" : "\x1b[5~"; // Page Up
    case VK_NEXT:   return shift ? "\x1b[6;2~" : "\x1b[6~"; // Page Down
    // --- function keys ---
    case VK_F1:  return "\x1bOP";
    case VK_F2:  return "\x1bOQ";
    case VK_F3:  return "\x1bOR";
    case VK_F4:  return "\x1bOS";
    case VK_F5:  return "\x1b[15~";
    case VK_F6:  return "\x1b[17~";
    case VK_F7:  return "\x1b[18~";
    case VK_F8:  return "\x1b[19~";
    case VK_F9:  return "\x1b[20~";
    case VK_F10: return "\x1b[21~";
    case VK_F11: return "\x1b[23~";
    case VK_F12: return "\x1b[24~";
    // --- control chars ---
    case VK_BACK:   return "\x7f";      // Backspace
    case VK_RETURN: return "\r";        // Enter
    case VK_TAB:    return shift ? "\x1b[Z" : "\x09"; // Tab / Shift+Tab
    case VK_ESCAPE: return "\x1b";
    default:        return "";
    }
}
