<<<<<<< HEAD
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include "../include/TerminalView.h"
#include <string>

static TerminalView* g_view  = nullptr;
static std::wstring  g_shell = L"powershell.exe";

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_view = new TerminalView();
        if (!g_view->Create(hwnd)) {
            delete g_view;
            g_view = nullptr;
            return -1;
        }
        return 0;

    case WM_SIZE:
        if (g_view)
            g_view->MoveAndResize(0, 0, LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_SETFOCUS:
        if (g_view && g_view->Hwnd())
            SetFocus(g_view->Hwnd());
        return 0;

    case WM_DESTROY:
        if (g_view) { delete g_view; g_view = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // Check for --embedded flag (host requests hidden window for embedding)
    bool embedded = false;
    int argIdx = 1;
    if (argv && argc >= 2 && wcscmp(argv[1], L"--embedded") == 0) {
        embedded = true;
        argIdx = 2;
    }

    if (argv && argc > argIdx) {
        g_shell = argv[argIdx];
        if (g_shell.find(L' ') != std::wstring::npos)
            g_shell = L"\"" + g_shell + L"\"";
        for (int i = argIdx + 1; i < argc; ++i) {
            g_shell += L" ";
            std::wstring arg = argv[i];
            if (arg.find(L' ') != std::wstring::npos)
                arg = L"\"" + arg + L"\"";
            g_shell += arg;
        }
    }
    if (argv) LocalFree(argv);

    // Derive window title: "powershell.exe" -> "powershell"
    std::wstring title = g_shell;
    size_t slash = title.find_last_of(L"\\/");
    if (slash != std::wstring::npos) title = title.substr(slash + 1);
    size_t dot = title.rfind(L'.');
    if (dot != std::wstring::npos) title = title.substr(0, dot);

    TerminalView::RegisterWindowClass(hInstance);

    const wchar_t CLASS_NAME[] = L"EcodeTerminalWindow";
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (!RegisterClassExW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 500,
        NULL, NULL, hInstance, NULL);
    if (!hwnd) return 1;

    // When embedded, stay hidden until host sends WM_EMBED_APP
    ShowWindow(hwnd, embedded ? SW_HIDE : nCmdShow);

    if (g_view && !g_view->StartSession(g_shell)) {
        std::wstring msg = L"Failed to start shell: " + g_shell + L"\n\n" + g_view->LastError();
        MessageBoxW(hwnd, msg.c_str(), L"Terminal", MB_OK | MB_ICONERROR);
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
=======
// =============================================================================
// Terminal — ConPTY terminal emulator plugin for Ecode
// Single-file: ConPTY session + VT/ANSI parser + Direct2D renderer
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Custom messages
// ---------------------------------------------------------------------------
#define WM_PTY_OUTPUT  (WM_USER + 10)   // lParam = new std::string*
#define TIMER_CURSOR   1

// ---------------------------------------------------------------------------
// ConPTY function pointers (dynamically loaded from kernel32/conpty)
// ---------------------------------------------------------------------------
typedef HRESULT (WINAPI *PFN_CreatePseudoConsole)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
typedef HRESULT (WINAPI *PFN_ResizePseudoConsole)(HPCON, COORD);
typedef void    (WINAPI *PFN_ClosePseudoConsole)(HPCON);

static PFN_CreatePseudoConsole g_CreatePseudoConsole = nullptr;
static PFN_ResizePseudoConsole g_ResizePseudoConsole = nullptr;
static PFN_ClosePseudoConsole  g_ClosePseudoConsole  = nullptr;

// ---------------------------------------------------------------------------
// Terminal cell
// ---------------------------------------------------------------------------
struct Cell {
    wchar_t  ch   = L' ';
    COLORREF fg   = 0xC0C0C0;
    COLORREF bg   = 0x000000;
    bool     bold = false;
};

// ---------------------------------------------------------------------------
// Terminal state
// ---------------------------------------------------------------------------
enum class ParseState { Ground, Escape, CSI, OSC };

struct TermState {
    std::vector<std::vector<Cell>> screen;  // [row][col]
    int cols       = 80;
    int rows       = 24;
    int curRow     = 0;
    int curCol     = 0;
    bool cursorVisible = true;
    bool cursorBlink   = true;   // toggled by TIMER_CURSOR

    COLORREF fgColor = 0xC0C0C0;
    COLORREF bgColor = 0x000000;
    bool bold        = false;

    int scrollTop = 0;
    int scrollBot = 23;          // updated when rows change

    ParseState ps = ParseState::Ground;
    std::string escBuf;           // accumulates CSI/OSC bytes

    void Resize(int newCols, int newRows) {
        cols = newCols;
        rows = newRows;
        scrollTop = 0;
        scrollBot = rows - 1;
        screen.assign(rows, std::vector<Cell>(cols));
        curRow = std::min(curRow, rows - 1);
        curCol = std::min(curCol, cols - 1);
    }

    void EraseCell(int row, int col) {
        if (row < 0 || row >= rows || col < 0 || col >= cols) return;
        screen[row][col] = Cell{};
    }

    void EraseLine(int row, int from, int to) {
        to = std::min(to, cols - 1);
        for (int c = from; c <= to; ++c) EraseCell(row, c);
    }

    void EraseDisplay(int mode) {
        if (mode == 0) {          // below cursor
            EraseLine(curRow, curCol, cols - 1);
            for (int r = curRow + 1; r < rows; ++r) EraseLine(r, 0, cols - 1);
        } else if (mode == 1) {   // above cursor
            for (int r = 0; r < curRow; ++r) EraseLine(r, 0, cols - 1);
            EraseLine(curRow, 0, curCol);
        } else if (mode == 2 || mode == 3) {
            for (int r = 0; r < rows; ++r) EraseLine(r, 0, cols - 1);
        }
    }

    void ScrollUp() {
        // scroll content up by 1 within [scrollTop, scrollBot]
        screen.erase(screen.begin() + scrollTop);
        screen.insert(screen.begin() + scrollBot, std::vector<Cell>(cols));
    }

    void NewLine() {
        if (curRow >= scrollBot) {
            ScrollUp();
        } else {
            curRow++;
        }
    }

    void PutChar(wchar_t ch) {
        if (curCol >= cols) {
            curCol = 0;
            NewLine();
        }
        if (curRow < rows && curCol < cols) {
            Cell &c = screen[curRow][curCol];
            c.ch   = ch;
            c.fg   = fgColor;
            c.bg   = bgColor;
            c.bold = bold;
        }
        curCol++;
    }
};

// ---------------------------------------------------------------------------
// ANSI 16-color palette
// ---------------------------------------------------------------------------
static const COLORREF k16Colors[16] = {
    0x000000, 0x800000, 0x008000, 0x808000,
    0x000080, 0x800080, 0x008080, 0xC0C0C0,
    0x808080, 0xFF0000, 0x00FF00, 0xFFFF00,
    0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF,
};

static COLORREF Xterm256Color(int n) {
    if (n < 16) return k16Colors[n];
    if (n < 232) {
        n -= 16;
        int b = n % 6; n /= 6;
        int g = n % 6; n /= 6;
        int r = n;
        auto v = [](int x) -> COLORREF { return x ? 55 + x * 40 : 0; };
        return RGB(v(r), v(g), v(b));
    }
    // grayscale 232-255
    int gray = 8 + (n - 232) * 10;
    return RGB(gray, gray, gray);
}

// ---------------------------------------------------------------------------
// PTY session
// ---------------------------------------------------------------------------
struct PtySession {
    HPCON  hPcon    = nullptr;
    HANDLE hInWrite = nullptr;
    HANDLE hOutRead = nullptr;
    HANDLE hProcess = nullptr;
    HANDLE hJob     = nullptr;
    HANDLE hReaderThread = nullptr;
    LPPROC_THREAD_ATTRIBUTE_LIST attrList = nullptr;
};

static HWND       g_hwnd = nullptr;
static TermState  g_term;
static PtySession g_pty;

// ---------------------------------------------------------------------------
// Direct2D resources
// ---------------------------------------------------------------------------
static ID2D1Factory*          g_d2d   = nullptr;
static IDWriteFactory*        g_dw    = nullptr;
static ID2D1HwndRenderTarget* g_rt    = nullptr;
static IDWriteTextFormat*     g_tf    = nullptr;
static float                  g_cellW = 8.0f;
static float                  g_cellH = 16.0f;
static std::unordered_map<COLORREF, ID2D1SolidColorBrush*> g_brushes;

static ID2D1SolidColorBrush* GetBrush(COLORREF color) {
    auto it = g_brushes.find(color);
    if (it != g_brushes.end()) return it->second;
    ID2D1SolidColorBrush* brush = nullptr;
    float r = GetRValue(color) / 255.0f;
    float gf = GetGValue(color) / 255.0f;
    float b = GetBValue(color) / 255.0f;
    if (g_rt) g_rt->CreateSolidColorBrush(D2D1::ColorF(r, gf, b), &brush);
    if (brush) g_brushes[color] = brush;
    return brush;
}

static void DiscardBrushes() {
    for (auto &kv : g_brushes) { if (kv.second) kv.second->Release(); }
    g_brushes.clear();
}

static void DiscardRT() {
    DiscardBrushes();
    if (g_rt) { g_rt->Release(); g_rt = nullptr; }
}

static bool EnsureRT() {
    if (g_rt) return true;
    if (!g_d2d || !g_hwnd) return false;
    RECT rc; GetClientRect(g_hwnd, &rc);
    D2D1_SIZE_U sz = { (UINT32)(rc.right - rc.left), (UINT32)(rc.bottom - rc.top) };
    HRESULT hr = g_d2d->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(g_hwnd, sz),
        &g_rt);
    return SUCCEEDED(hr);
}

// Measure cell dimensions from the text format
static void MeasureCellSize() {
    if (!g_dw || !g_tf) return;
    IDWriteTextLayout* layout = nullptr;
    HRESULT hr = g_dw->CreateTextLayout(L"M", 1, g_tf, 1000.0f, 1000.0f, &layout);
    if (SUCCEEDED(hr) && layout) {
        DWRITE_TEXT_METRICS m = {};
        layout->GetMetrics(&m);
        g_cellW = m.width;
        g_cellH = m.height;
        layout->Release();
    }
}

// ---------------------------------------------------------------------------
// VT/ANSI parser helpers
// ---------------------------------------------------------------------------

// Parse semicolon-separated integers from escBuf params
static std::vector<int> ParseParams(const std::string &buf) {
    std::vector<int> params;
    int cur = 0;
    bool hasDigit = false;
    for (char c : buf) {
        if (c >= '0' && c <= '9') { cur = cur * 10 + (c - '0'); hasDigit = true; }
        else if (c == ';') { params.push_back(hasDigit ? cur : 0); cur = 0; hasDigit = false; }
    }
    if (hasDigit || !buf.empty()) params.push_back(cur);
    return params;
}

static int Param(const std::vector<int> &p, int idx, int def) {
    if (idx < (int)p.size() && p[idx] != 0) return p[idx];
    return def;
}

// Process SGR (Select Graphic Rendition) — ESC [ ... m
static void ApplySGR(const std::vector<int> &params) {
    if (params.empty()) { // reset
        g_term.fgColor = 0xC0C0C0;
        g_term.bgColor = 0x000000;
        g_term.bold    = false;
        return;
    }
    for (int i = 0; i < (int)params.size(); ++i) {
        int p = params[i];
        if (p == 0) { g_term.fgColor = 0xC0C0C0; g_term.bgColor = 0x000000; g_term.bold = false; }
        else if (p == 1)  { g_term.bold = true; }
        else if (p == 22) { g_term.bold = false; }
        else if (p >= 30 && p <= 37) { g_term.fgColor = k16Colors[p - 30 + (g_term.bold ? 8 : 0)]; }
        else if (p == 39) { g_term.fgColor = 0xC0C0C0; }
        else if (p >= 40 && p <= 47) { g_term.bgColor = k16Colors[p - 40]; }
        else if (p == 49) { g_term.bgColor = 0x000000; }
        else if (p >= 90 && p <= 97) { g_term.fgColor = k16Colors[p - 90 + 8]; }
        else if (p >= 100 && p <= 107) { g_term.bgColor = k16Colors[p - 100 + 8]; }
        else if (p == 38 && i + 2 < (int)params.size() && params[i+1] == 5) {
            g_term.fgColor = Xterm256Color(params[i+2]); i += 2;
        } else if (p == 38 && i + 4 < (int)params.size() && params[i+1] == 2) {
            g_term.fgColor = RGB(params[i+2], params[i+3], params[i+4]); i += 4;
        } else if (p == 48 && i + 2 < (int)params.size() && params[i+1] == 5) {
            g_term.bgColor = Xterm256Color(params[i+2]); i += 2;
        } else if (p == 48 && i + 4 < (int)params.size() && params[i+1] == 2) {
            g_term.bgColor = RGB(params[i+2], params[i+3], params[i+4]); i += 4;
        }
    }
}

// Process a complete CSI sequence: params in escBuf, final byte = finalCh
static void HandleCSI(char finalCh) {
    std::string paramStr = g_term.escBuf;
    bool isDEC = (!paramStr.empty() && paramStr[0] == '?');
    if (isDEC) paramStr = paramStr.substr(1);

    std::vector<int> params = ParseParams(paramStr);
    int &row = g_term.curRow;
    int &col = g_term.curCol;

    switch (finalCh) {
    case 'A': row = std::max(g_term.scrollTop, row - Param(params, 0, 1)); break;
    case 'B': row = std::min(g_term.scrollBot, row + Param(params, 0, 1)); break;
    case 'C': col = std::min(g_term.cols - 1, col + Param(params, 0, 1)); break;
    case 'D': col = std::max(0, col - Param(params, 0, 1)); break;
    case 'E': row = std::min(g_term.scrollBot, row + Param(params, 0, 1)); col = 0; break;
    case 'F': row = std::max(g_term.scrollTop, row - Param(params, 0, 1)); col = 0; break;
    case 'G': col = std::min(g_term.cols - 1, Param(params, 0, 1) - 1); break;
    case 'H': case 'f':
        row = std::max(0, std::min(g_term.rows - 1, Param(params, 0, 1) - 1));
        col = std::max(0, std::min(g_term.cols - 1, Param(params, 1, 1) - 1));
        break;
    case 'J': g_term.EraseDisplay(Param(params, 0, 0)); break;
    case 'K': {
        int mode = Param(params, 0, 0);
        if (mode == 0)      g_term.EraseLine(row, col, g_term.cols - 1);
        else if (mode == 1) g_term.EraseLine(row, 0, col);
        else if (mode == 2) g_term.EraseLine(row, 0, g_term.cols - 1);
        break;
    }
    case 'L': {   // insert lines
        int n = Param(params, 0, 1);
        for (int i = 0; i < n && row + i < g_term.rows; ++i) {
            g_term.screen.insert(g_term.screen.begin() + row, std::vector<Cell>(g_term.cols));
            if ((int)g_term.screen.size() > g_term.rows)
                g_term.screen.erase(g_term.screen.begin() + g_term.rows);
        }
        break;
    }
    case 'M': {   // delete lines
        int n = Param(params, 0, 1);
        for (int i = 0; i < n && row < g_term.rows; ++i) {
            g_term.screen.erase(g_term.screen.begin() + row);
            g_term.screen.insert(g_term.screen.begin() + g_term.scrollBot, std::vector<Cell>(g_term.cols));
        }
        break;
    }
    case 'P': {   // delete chars
        int n = Param(params, 0, 1);
        auto &line = g_term.screen[row];
        line.erase(line.begin() + col, line.begin() + std::min(col + n, g_term.cols));
        while ((int)line.size() < g_term.cols) line.push_back(Cell{});
        break;
    }
    case 'S': {   // scroll up
        int n = Param(params, 0, 1);
        for (int i = 0; i < n; ++i) g_term.ScrollUp();
        break;
    }
    case 'T': {   // scroll down
        int n = Param(params, 0, 1);
        for (int i = 0; i < n; ++i) {
            g_term.screen.erase(g_term.screen.begin() + g_term.scrollBot);
            g_term.screen.insert(g_term.screen.begin() + g_term.scrollTop, std::vector<Cell>(g_term.cols));
        }
        break;
    }
    case 'm': ApplySGR(params); break;
    case 'r':
        g_term.scrollTop = std::max(0, Param(params, 0, 1) - 1);
        g_term.scrollBot = std::min(g_term.rows - 1, Param(params, 1, g_term.rows) - 1);
        break;
    case 'h':
        if (isDEC && Param(params, 0, 0) == 25) g_term.cursorVisible = true;
        break;
    case 'l':
        if (isDEC && Param(params, 0, 0) == 25) g_term.cursorVisible = false;
        break;
    // ignore unrecognized sequences
    default: break;
    }
}

// UTF-8 → wide char, returns 0 for incomplete/invalid
static wchar_t Utf8Step(const unsigned char *data, size_t len, size_t &i) {
    unsigned char c = data[i];
    if (c < 0x80) { i++; return (wchar_t)c; }
    int extra = 0; uint32_t cp = 0;
    if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
    else { i++; return L'?'; }
    if (i + extra >= len) return 0;  // incomplete
    for (int k = 0; k < extra; ++k) {
        ++i;
        if ((data[i] & 0xC0) != 0x80) return L'?';
        cp = (cp << 6) | (data[i] & 0x3F);
    }
    i++;
    if (cp > 0xFFFF) return L'?';  // skip surrogates/beyond BMP for simplicity
    return (wchar_t)cp;
}

// Main VT/ANSI processor
static void ProcessOutput(const char *data, size_t len) {
    const unsigned char *u = (const unsigned char *)data;
    size_t i = 0;
    while (i < len) {
        unsigned char byte = u[i];

        switch (g_term.ps) {
        case ParseState::Ground: {
            if (byte == 0x1B) { g_term.ps = ParseState::Escape; g_term.escBuf.clear(); i++; break; }
            if (byte == '\r') { g_term.curCol = 0; i++; break; }
            if (byte == '\n') { g_term.NewLine(); i++; break; }
            if (byte == '\b') { if (g_term.curCol > 0) g_term.curCol--; i++; break; }
            if (byte == '\t') {
                int next = ((g_term.curCol / 8) + 1) * 8;
                g_term.curCol = std::min(next, g_term.cols - 1);
                i++; break;
            }
            if (byte < 0x20) { i++; break; }  // ignore other control chars
            wchar_t wch = Utf8Step(u, len, i);
            if (wch == 0) goto done;           // incomplete UTF-8 at end of buffer
            g_term.PutChar(wch);
            break;
        }
        case ParseState::Escape:
            i++;
            if (byte == '[') { g_term.ps = ParseState::CSI; g_term.escBuf.clear(); }
            else if (byte == ']') { g_term.ps = ParseState::OSC; g_term.escBuf.clear(); }
            else if (byte == 'M') {
                // reverse index
                if (g_term.curRow > g_term.scrollTop) g_term.curRow--;
                else {
                    g_term.screen.insert(g_term.screen.begin() + g_term.scrollTop, std::vector<Cell>(g_term.cols));
                    if ((int)g_term.screen.size() > g_term.rows)
                        g_term.screen.erase(g_term.screen.begin() + g_term.rows);
                }
                g_term.ps = ParseState::Ground;
            }
            else if (byte == 'c') {   // full reset
                g_term.Resize(g_term.cols, g_term.rows);
                g_term.fgColor = 0xC0C0C0; g_term.bgColor = 0x000000; g_term.bold = false;
                g_term.ps = ParseState::Ground;
            }
            else { g_term.ps = ParseState::Ground; }
            break;

        case ParseState::CSI:
            i++;
            if ((byte >= 0x40 && byte <= 0x7E)) {
                HandleCSI((char)byte);
                g_term.ps = ParseState::Ground;
            } else {
                g_term.escBuf += (char)byte;
            }
            break;

        case ParseState::OSC:
            i++;
            if (byte == 0x07 || byte == 0x1B) {  // BEL or ST
                if (byte == 0x1B && i < len && u[i] == '\\') i++;  // skip trailing backslash of ST
                g_term.ps = ParseState::Ground;
            }
            // else accumulate & ignore OSC content (title, hyperlink, etc.)
            break;
        }
    }
done:;
}

// ---------------------------------------------------------------------------
// ConPTY setup / teardown
// ---------------------------------------------------------------------------
static DWORD WINAPI ReaderThreadProc(LPVOID) {
    char buf[4096];
    DWORD n;
    while (ReadFile(g_pty.hOutRead, buf, sizeof(buf), &n, nullptr) && n > 0) {
        auto *s = new std::string(buf, n);
        PostMessage(g_hwnd, WM_PTY_OUTPUT, 0, (LPARAM)s);
    }
    return 0;
}

static bool LoadConPTY() {
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel) return false;
    g_CreatePseudoConsole = (PFN_CreatePseudoConsole)GetProcAddress(hKernel, "CreatePseudoConsole");
    g_ResizePseudoConsole = (PFN_ResizePseudoConsole)GetProcAddress(hKernel, "ResizePseudoConsole");
    g_ClosePseudoConsole  = (PFN_ClosePseudoConsole) GetProcAddress(hKernel, "ClosePseudoConsole");
    return g_CreatePseudoConsole && g_ResizePseudoConsole && g_ClosePseudoConsole;
}

static bool StartPty(int cols, int rows) {
    if (!g_CreatePseudoConsole) return false;

    HANDLE hInRead = nullptr, hOutWrite = nullptr;
    if (!CreatePipe(&hInRead,       &g_pty.hInWrite, nullptr, 0)) return false;
    if (!CreatePipe(&g_pty.hOutRead, &hOutWrite,     nullptr, 0)) {
        CloseHandle(hInRead); CloseHandle(g_pty.hInWrite);
        return false;
    }

    COORD size = { (SHORT)cols, (SHORT)rows };
    HRESULT hr = g_CreatePseudoConsole(size, hInRead, hOutWrite, 0, &g_pty.hPcon);
    CloseHandle(hInRead);
    CloseHandle(hOutWrite);
    if (FAILED(hr)) return false;

    // Build STARTUPINFOEX with PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    g_pty.attrList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrSize);
    if (!g_pty.attrList) return false;
    InitializeProcThreadAttributeList(g_pty.attrList, 1, 0, &attrSize);
    UpdateProcThreadAttribute(g_pty.attrList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, g_pty.hPcon, sizeof(HPCON), nullptr, nullptr);

    STARTUPINFOEXW si = {};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.lpAttributeList = g_pty.attrList;

    // Try PowerShell, fall back to cmd
    wchar_t shell[MAX_PATH] = L"powershell.exe";
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, shell, nullptr, nullptr, FALSE,
            EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
            &si.StartupInfo, &pi)) {
        wcscpy_s(shell, L"cmd.exe");
        if (!CreateProcessW(nullptr, shell, nullptr, nullptr, FALSE,
                EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                &si.StartupInfo, &pi))
            return false;
    }

    g_pty.hProcess = pi.hProcess;
    CloseHandle(pi.hThread);

    // Job object: kill process tree when plugin is closed
    g_pty.hJob = CreateJobObjectW(nullptr, nullptr);
    if (g_pty.hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(g_pty.hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        AssignProcessToJobObject(g_pty.hJob, g_pty.hProcess);
    }

    g_pty.hReaderThread = CreateThread(nullptr, 0, ReaderThreadProc, nullptr, 0, nullptr);
    return true;
}

static void ResizePty(int cols, int rows) {
    if (g_pty.hPcon && g_ResizePseudoConsole) {
        COORD size = { (SHORT)cols, (SHORT)rows };
        g_ResizePseudoConsole(g_pty.hPcon, size);
    }
}

static void StopPty() {
    if (g_pty.hPcon && g_ClosePseudoConsole) {
        g_ClosePseudoConsole(g_pty.hPcon);
        g_pty.hPcon = nullptr;
    }
    if (g_pty.hReaderThread) {
        WaitForSingleObject(g_pty.hReaderThread, 3000);
        CloseHandle(g_pty.hReaderThread);
        g_pty.hReaderThread = nullptr;
    }
    if (g_pty.hProcess) { CloseHandle(g_pty.hProcess); g_pty.hProcess = nullptr; }
    if (g_pty.hInWrite)  { CloseHandle(g_pty.hInWrite);  g_pty.hInWrite  = nullptr; }
    if (g_pty.hOutRead)  { CloseHandle(g_pty.hOutRead);  g_pty.hOutRead  = nullptr; }
    if (g_pty.hJob)      { CloseHandle(g_pty.hJob);      g_pty.hJob      = nullptr; }
    if (g_pty.attrList) {
        DeleteProcThreadAttributeList(g_pty.attrList);
        HeapFree(GetProcessHeap(), 0, g_pty.attrList);
        g_pty.attrList = nullptr;
    }
}

static void SendInput(const char *data, size_t len) {
    if (g_pty.hInWrite && data && len) {
        DWORD written;
        WriteFile(g_pty.hInWrite, data, (DWORD)len, &written, nullptr);
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
static void Render() {
    if (!EnsureRT()) return;

    g_rt->BeginDraw();
    g_rt->Clear(D2D1::ColorF(0, 0, 0));  // black background

    for (int row = 0; row < g_term.rows; ++row) {
        for (int col = 0; col < g_term.cols; ++col) {
            const Cell &cell = g_term.screen[row][col];
            float x = col * g_cellW;
            float y = row * g_cellH;
            D2D1_RECT_F rc = D2D1::RectF(x, y, x + g_cellW, y + g_cellH);

            // Background
            COLORREF bg = cell.bg;
            // Invert cursor cell
            bool isCursor = (row == g_term.curRow && col == g_term.curCol
                             && g_term.cursorVisible && g_term.cursorBlink);
            if (isCursor) bg = cell.fg;
            if (bg != 0x000000) {
                auto *br = GetBrush(bg);
                if (br) g_rt->FillRectangle(rc, br);
            }

            // Character
            if (cell.ch != L' ' && cell.ch != L'\0') {
                COLORREF fg = isCursor ? cell.bg : cell.fg;
                auto *br = GetBrush(fg);
                if (br && g_tf) {
                    wchar_t wch[2] = { cell.ch, 0 };
                    g_rt->DrawTextW(wch, 1, g_tf, rc, br);
                }
            }
        }
    }

    HRESULT hr = g_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) DiscardRT();
}

// ---------------------------------------------------------------------------
// Window size → terminal grid size
// ---------------------------------------------------------------------------
static void UpdateGridSize(int clientW, int clientH) {
    if (g_cellW < 1.0f || g_cellH < 1.0f) return;
    int newCols = std::max(1, (int)(clientW / g_cellW));
    int newRows = std::max(1, (int)(clientH / g_cellH));
    if (newCols == g_term.cols && newRows == g_term.rows) return;
    g_term.Resize(newCols, newRows);
    ResizePty(newCols, newRows);
}

// ---------------------------------------------------------------------------
// Key → VT sequence
// ---------------------------------------------------------------------------
static void HandleKeyDown(WPARAM vk, bool ctrl) {
    char seq[8] = {};
    const char *s = nullptr;
    size_t slen = 0;

    if (ctrl) {
        if (vk >= 'A' && vk <= 'Z') {
            seq[0] = (char)(vk - 'A' + 1);
            SendInput(seq, 1);
            return;
        }
    }

    switch (vk) {
    case VK_RETURN:  s = "\r";        slen = 1; break;
    case VK_BACK:    s = "\x7f";      slen = 1; break;
    case VK_ESCAPE:  s = "\x1b";      slen = 1; break;
    case VK_UP:      s = "\x1b[A";    slen = 3; break;
    case VK_DOWN:    s = "\x1b[B";    slen = 3; break;
    case VK_RIGHT:   s = "\x1b[C";    slen = 3; break;
    case VK_LEFT:    s = "\x1b[D";    slen = 3; break;
    case VK_HOME:    s = "\x1b[H";    slen = 3; break;
    case VK_END:     s = "\x1b[F";    slen = 3; break;
    case VK_DELETE:  s = "\x1b[3~";   slen = 4; break;
    case VK_INSERT:  s = "\x1b[2~";   slen = 4; break;
    case VK_PRIOR:   s = "\x1b[5~";   slen = 4; break;
    case VK_NEXT:    s = "\x1b[6~";   slen = 4; break;
    case VK_F1:      s = "\x1bOP";    slen = 3; break;
    case VK_F2:      s = "\x1bOQ";    slen = 3; break;
    case VK_F3:      s = "\x1bOR";    slen = 3; break;
    case VK_F4:      s = "\x1bOS";    slen = 3; break;
    case VK_F5:      s = "\x1b[15~";  slen = 5; break;
    case VK_F6:      s = "\x1b[17~";  slen = 5; break;
    case VK_F7:      s = "\x1b[18~";  slen = 5; break;
    case VK_F8:      s = "\x1b[19~";  slen = 5; break;
    case VK_F9:      s = "\x1b[20~";  slen = 5; break;
    case VK_F10:     s = "\x1b[21~";  slen = 5; break;
    case VK_F11:     s = "\x1b[23~";  slen = 5; break;
    case VK_F12:     s = "\x1b[24~";  slen = 5; break;
    default: return;
    }
    if (s && slen) SendInput(s, slen);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        g_term.Resize(80, 24);

        // Start ConPTY
        if (!LoadConPTY()) {
            MessageBoxW(hwnd, L"ConPTY not available (requires Windows 10 1809+)",
                        L"Terminal", MB_ICONERROR);
        } else if (!StartPty(80, 24)) {
            MessageBoxW(hwnd, L"Failed to start terminal session.", L"Terminal", MB_ICONERROR);
        }

        SetTimer(hwnd, TIMER_CURSOR, 500, nullptr);
        return 0;
    }

    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        if (g_rt) g_rt->Resize(D2D1::SizeU(w, h));
        UpdateGridSize(w, h);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        Render();
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_PTY_OUTPUT: {
        auto *s = (std::string*)lp;
        if (s) {
            ProcessOutput(s->data(), s->size());
            delete s;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_TIMER:
        if (wp == TIMER_CURSOR) {
            g_term.cursorBlink = !g_term.cursorBlink;
            // Only repaint cursor area
            if (g_term.cursorVisible) {
                RECT rc = {
                    (LONG)(g_term.curCol * g_cellW),
                    (LONG)(g_term.curRow * g_cellH),
                    (LONG)((g_term.curCol + 1) * g_cellW),
                    (LONG)((g_term.curRow + 1) * g_cellH),
                };
                InvalidateRect(hwnd, &rc, FALSE);
            }
        }
        return 0;

    case WM_CHAR: {
        // Printable characters — convert to UTF-8
        wchar_t wch = (wchar_t)wp;
        if (wch < 0x20 || wch == 0x7F) {
            // Handled in WM_KEYDOWN; let WM_CHAR ignore control chars
            // Exception: tab
            if (wch == L'\t') SendInput("\t", 1);
            return 0;
        }
        char utf8[5] = {};
        int n = WideCharToMultiByte(CP_UTF8, 0, &wch, 1, utf8, 4, nullptr, nullptr);
        if (n > 0) SendInput(utf8, (size_t)n);
        return 0;
    }

    case WM_KEYDOWN: {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        HandleKeyDown(wp, ctrl);
        return 0;
    }

    case WM_SETFOCUS:
        // Ensure keystrokes reach this window even when embedded
        return 0;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        int lines = (delta > 0) ? -3 : 3;
        // Scroll by sending cursor-up/down sequences
        const char *seq = (lines < 0) ? "\x1b[A\x1b[A\x1b[A" : "\x1b[B\x1b[B\x1b[B";
        SendInput(seq, 9);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_CURSOR);
        StopPty();
        DiscardRT();
        if (g_tf)  { g_tf->Release();  g_tf  = nullptr; }
        if (g_dw)  { g_dw->Release();  g_dw  = nullptr; }
        if (g_d2d) { g_d2d->Release(); g_d2d = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    // Initialize Direct2D / DirectWrite
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2d);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_dw);

    if (g_dw) {
        // Try Cascadia Mono first, fall back to Consolas
        HRESULT hr = g_dw->CreateTextFormat(L"Cascadia Mono", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &g_tf);
        if (FAILED(hr) || !g_tf) {
            g_dw->CreateTextFormat(L"Consolas", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &g_tf);
        }
        if (g_tf) {
            g_tf->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            g_tf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            g_tf->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    }

    MeasureCellSize();

    // Register window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_IBEAM);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"EcodeTerminalPlugin";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"EcodeTerminalPlugin", L"Terminal",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
>>>>>>> feature/terminalapp
