// =============================================================================
// FastDired — Two-pane directory browser with Direct2D rendering
// Incremental scanning: results appear as the scan progresses.
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
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <atomic>

#define WM_NEW_ENTRIES (WM_USER + 10)
#define WM_SCAN_DONE   (WM_USER + 11)

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
struct FileEntry {
    std::wstring name;
    std::wstring ext;
    int64_t      size;
    FILETIME     lastWrite;
    DWORD        attributes;
    bool IsDir() const { return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
};

struct Pane {
    std::wstring           currentPath;
    std::vector<FileEntry> entries;
    int                    selectedIndex = 0;
    int                    scrollOffset  = 0; // first visible row
    int                    visibleRows   = 0;
    HANDLE                 scanThread    = nullptr;
    CRITICAL_SECTION       cs;
    bool                   scanning      = false;

    void InitCS() { InitializeCriticalSection(&cs); }
    void DoneCS() { DeleteCriticalSection(&cs); }
};

static HINSTANCE             g_hInst       = nullptr;
static HWND                  g_hwnd        = nullptr;
static Pane                  g_panes[2];
static int                   g_dividerPos  = -1;
static bool                  g_draggingDivider = false;
static int                   g_activePane  = 0; // keyboard focus: 0=left, 1=right

// Direct2D / DirectWrite
static ID2D1Factory*         g_d2d         = nullptr;
static IDWriteFactory*       g_dw          = nullptr;
static ID2D1HwndRenderTarget*g_rt          = nullptr;
static IDWriteTextFormat*    g_tf          = nullptr;
static ID2D1SolidColorBrush* g_brushText   = nullptr;
static ID2D1SolidColorBrush* g_brushBg     = nullptr;
static ID2D1SolidColorBrush* g_brushSel    = nullptr;
static ID2D1SolidColorBrush* g_brushHeader = nullptr;
static ID2D1SolidColorBrush* g_brushDivider= nullptr;
static D2D1_SIZE_F           g_clientSize  = {};

static const float g_rowH     = 20.0f;
static const float g_headerH  = 22.0f;
static const float g_pathH    = 24.0f;
static const float g_colW[]   = { 200.0f, 70.0f, 60.0f, 130.0f };
static const int   g_colCount = 4;

static std::atomic<bool> g_cancelScan{false};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void NavigateTo(Pane *pane, const std::wstring &path);
static void StartScan(Pane *pane);
static void CancelScan(Pane *pane);

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
static void NormalizePath(std::wstring &path) {
    wchar_t full[MAX_PATH];
    if (GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr)) {
        path = full;
    }
    if (path.size() == 2 && path[1] == L':') path += L'\\';
}

// ---------------------------------------------------------------------------
// Direct2D helpers
// ---------------------------------------------------------------------------
static bool EnsureRT() {
    if (g_rt) return true;
    if (!g_d2d) return false;
    RECT rc; GetClientRect(g_hwnd, &rc);
    D2D1_SIZE_U sz = { (UINT32)(rc.right - rc.left), (UINT32)(rc.bottom - rc.top) };
    HRESULT hr = g_d2d->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(g_hwnd, sz), &g_rt);
    if (FAILED(hr)) return false;
    g_rt->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &g_brushText);
    g_rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &g_brushBg);
    g_rt->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.85f, 0.95f), &g_brushSel);
    g_rt->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.3f, 0.3f), &g_brushHeader);
    g_rt->CreateSolidColorBrush(D2D1::ColorF(0.6f, 0.6f, 0.6f), &g_brushDivider);
    return true;
}

static void DiscardRT() {
    if (g_rt) g_rt->Release(); g_rt = nullptr;
    if (g_brushText)   g_brushText->Release();   g_brushText   = nullptr;
    if (g_brushBg)     g_brushBg->Release();     g_brushBg     = nullptr;
    if (g_brushSel)    g_brushSel->Release();    g_brushSel    = nullptr;
    if (g_brushHeader) g_brushHeader->Release(); g_brushHeader = nullptr;
    if (g_brushDivider)g_brushDivider->Release();g_brushDivider= nullptr;
}

// ---------------------------------------------------------------------------
// Scan thread (incremental: posts batches via WM_NEW_ENTRIES)
// ---------------------------------------------------------------------------
struct ScanParam {
    Pane *pane;
    std::wstring path;
};

static DWORD WINAPI ScanThreadProc(LPVOID lp) {
    ScanParam *sp = (ScanParam*)lp;
    Pane *pane = sp->pane;
    std::wstring path = sp->path;
    delete sp;

    std::wstring searchPath = path + L"\\*";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        PostMessage(g_hwnd, WM_SCAN_DONE, 0, (LPARAM)pane);
        return 0;
    }

    std::vector<FileEntry> batch;
    batch.reserve(100);
    int total = 0;
    do {
        if (g_cancelScan.load()) { FindClose(hFind); PostMessage(g_hwnd, WM_SCAN_DONE, 0, (LPARAM)pane); return 0; }
        if (wcscmp(ffd.cFileName, L".") == 0) continue;
        FileEntry e;
        e.name = ffd.cFileName;
        e.size = ((int64_t)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow;
        e.lastWrite = ffd.ftLastWriteTime;
        e.attributes = ffd.dwFileAttributes;
        size_t dot = e.name.rfind(L'.');
        e.ext = (dot != std::wstring::npos) ? e.name.substr(dot) : L"";
        batch.push_back(std::move(e));
        total++;
        if (batch.size() >= 100) {
            // Send batch to UI thread
            auto *b = new std::vector<FileEntry>(std::move(batch));
            PostMessage(g_hwnd, WM_NEW_ENTRIES, (WPARAM)pane, (LPARAM)b);
            batch.clear();
            batch.reserve(100);
        }
    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);

    // Send remaining batch
    if (!batch.empty()) {
        auto *b = new std::vector<FileEntry>(std::move(batch));
        PostMessage(g_hwnd, WM_NEW_ENTRIES, (WPARAM)pane, (LPARAM)b);
    }
    PostMessage(g_hwnd, WM_SCAN_DONE, 0, (LPARAM)pane);
    return 0;
}

static void StartScan(Pane *pane) {
    CancelScan(pane);
    pane->entries.clear();
    pane->selectedIndex = 0;
    pane->scrollOffset = 0;
    pane->scanning = true;
    ScanParam *sp = new ScanParam{pane, pane->currentPath};
    pane->scanThread = CreateThread(nullptr, 0, ScanThreadProc, sp, 0, nullptr);
}

static void CancelScan(Pane *pane) {
    if (pane->scanThread) {
        g_cancelScan.store(true);
        WaitForSingleObject(pane->scanThread, 3000);
        CloseHandle(pane->scanThread);
        pane->scanThread = nullptr;
        g_cancelScan.store(false);
    }
    pane->scanning = false;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
static void NavigateTo(Pane *pane, const std::wstring &path) {
    std::wstring p = path;
    NormalizePath(p);
    pane->currentPath = p;
    StartScan(pane);
}

static void GoToParent(Pane *pane) {
    if (pane->currentPath.size() <= 3) return;
    size_t pos = pane->currentPath.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return;
    std::wstring parent = pane->currentPath.substr(0, pos);
    if (parent.size() == 2 && parent[1] == L':') parent += L'\\';
    NavigateTo(pane, parent);
}

static void OpenSelected(Pane *pane) {
    if (pane->entries.empty()) return;
    int idx = pane->selectedIndex;
    if (idx < 0 || idx >= (int)pane->entries.size()) return;
    FileEntry &e = pane->entries[idx];
    std::wstring full = pane->currentPath + L"\\" + e.name;
    if (e.IsDir()) {
        NavigateTo(pane, full);
    } else {
        ShellExecuteW(g_hwnd, L"open", full.c_str(), nullptr, nullptr, SW_SHOW);
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
static void DrawPane(Pane *pane, float x, float y, float w, float h) {
    if (!g_rt) return;

    // Background
    g_rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), g_brushBg);

    // Path bar
    g_rt->FillRectangle(D2D1::RectF(x, y, x + w, y + g_pathH), g_brushHeader);
    if (g_tf) {
        std::wstring pathText = pane->currentPath.empty() ? L"(empty)" : pane->currentPath;
        g_rt->DrawTextW(pathText.c_str(), (UINT32)pathText.size(), g_tf,
            D2D1::RectF(x + 4, y + 2, x + w - 4, y + g_pathH - 2), g_brushBg);
    }

    float listY = y + g_pathH;
    float listH = h - g_pathH;

    // Header row
    g_rt->FillRectangle(D2D1::RectF(x, listY, x + w, listY + g_headerH), g_brushHeader);
    float cx = x;
    const wchar_t *headers[] = { L"Name", L"Size", L"Type", L"Date" };
    for (int i = 0; i < g_colCount; ++i) {
        D2D1_RECT_F rc = D2D1::RectF(cx + 4, listY + 2, cx + g_colW[i] - 2, listY + g_headerH - 2);
        if (g_tf) g_rt->DrawTextW(headers[i], (UINT32)wcslen(headers[i]), g_tf, rc, g_brushBg);
        // Column separator
        g_rt->DrawLine(D2D1::Point2F(cx + g_colW[i], listY), D2D1::Point2F(cx + g_colW[i], listY + g_headerH), g_brushDivider);
        cx += g_colW[i];
    }

    // Entries
    pane->visibleRows = (int)((listH - g_headerH) / g_rowH);
    EnterCriticalSection(&pane->cs);
    int entryCount = (int)pane->entries.size();
    for (int r = 0; r < pane->visibleRows; ++r) {
        int idx = pane->scrollOffset + r;
        if (idx >= entryCount) break;
        FileEntry &e = pane->entries[idx];
        float ry = listY + g_headerH + r * g_rowH;
        // Selection highlight
        if (idx == pane->selectedIndex) {
            g_rt->FillRectangle(D2D1::RectF(x, ry, x + w, ry + g_rowH), g_brushSel);
        }
        cx = x;
        // Name
        D2D1_RECT_F rc = D2D1::RectF(cx + 4, ry + 1, cx + g_colW[0] - 2, ry + g_rowH - 1);
        if (g_tf) {
            std::wstring display = e.name;
            if (e.name == L".." && !pane->currentPath.empty()) {
                std::wstring parent = pane->currentPath;
                size_t pos = parent.find_last_of(L"\\/");
                if (pos != std::wstring::npos) {
                    parent = parent.substr(0, pos);
                    if (parent.size() == 2 && parent[1] == L':') parent += L'\\';
                }
                display = parent;
            }
            g_rt->DrawTextW(display.c_str(), (UINT32)display.size(), g_tf, rc, g_brushText);
        }
        cx += g_colW[0];
        // Size
        if (!e.IsDir()) {
            wchar_t sz[32]; swprintf_s(sz, L"%lld", e.size);
            if (g_tf) g_rt->DrawTextW(sz, (UINT32)wcslen(sz), g_tf,
                D2D1::RectF(cx + 4, ry + 1, cx + g_colW[1] - 4, ry + g_rowH - 1), g_brushText);
        }
        cx += g_colW[1];
        // Type
        std::wstring type = e.IsDir() ? L"DIR" : L"FILE";
        if (g_tf) g_rt->DrawTextW(type.c_str(), (UINT32)type.size(), g_tf,
            D2D1::RectF(cx + 4, ry + 1, cx + g_colW[2] - 4, ry + g_rowH - 1), g_brushText);
        cx += g_colW[2];
        // Date
        FILETIME lt; SYSTEMTIME st;
        FileTimeToLocalFileTime(&e.lastWrite, &lt);
        FileTimeToSystemTime(&lt, &st);
        wchar_t dt[64]; swprintf_s(dt, L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
        if (g_tf) g_rt->DrawTextW(dt, (UINT32)wcslen(dt), g_tf,
            D2D1::RectF(cx + 4, ry + 1, cx + g_colW[3] - 4, ry + g_rowH - 1), g_brushText);
    }
    LeaveCriticalSection(&pane->cs);
}

static void Render() {
    if (!EnsureRT()) return;
    g_rt->BeginDraw();
    g_rt->Clear(D2D1::ColorF(0.9f, 0.9f, 0.9f));

    float w = g_clientSize.width;
    float h = g_clientSize.height;
    float divW = 4.0f;
    if (g_dividerPos < 0) g_dividerPos = (int)((w - divW) / 2);
    float lw = (float)g_dividerPos;
    float rw = w - g_dividerPos - divW;

    DrawPane(&g_panes[0], 0, 0, lw, h);
    DrawPane(&g_panes[1], g_dividerPos + divW, 0, rw, h);

    // Divider
    D2D1_RECT_F divRc = { lw, 0, lw + divW, h };
    g_rt->FillRectangle(divRc, g_brushDivider);

    HRESULT hr = g_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardRT();
    }
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        for (int i = 0; i < 2; ++i) g_panes[i].InitCS();
        // Init paths
        wchar_t curDir[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, curDir);
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        int argIdx = 1;
        if (argv && argc > argIdx && wcscmp(argv[argIdx], L"--embedded") == 0)
            ++argIdx;
        NavigateTo(&g_panes[0], (argv && argc > argIdx) ? argv[argIdx] : curDir);
        std::wstring rightPath = (argv && argc > argIdx + 1) ? argv[argIdx + 1] : L"";
        if (argv) LocalFree(argv);

        if (!rightPath.empty() && GetFileAttributesW(rightPath.c_str()) != INVALID_FILE_ATTRIBUTES)
            NavigateTo(&g_panes[1], rightPath);
        else {
            wchar_t root[4] = L"C:\\";
            DWORD drives = GetLogicalDrives();
            for (int i = 0; i < 26; ++i) {
                if (drives & (1 << i)) {
                    root[0] = L'A' + i;
                    if (GetDriveTypeW(root) == DRIVE_FIXED) { NavigateTo(&g_panes[1], root); break; }
                }
            }
        }
        DragAcceptFiles(hwnd, TRUE);
        SetFocus(hwnd);
        return 0;
    }

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wp;
        POINT pt;
        DragQueryPoint(hDrop, &pt);
        float divW = 4.0f;
        int paneIdx = (pt.x < g_dividerPos) ? 0 : 1;
        wchar_t path[MAX_PATH];
        DragQueryFileW(hDrop, 0, path, MAX_PATH);
        DWORD attr = GetFileAttributesW(path);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            NavigateTo(&g_panes[paneIdx], path);
        }
        DragFinish(hDrop);
        return 0;
    }

    case WM_SIZE: {
        g_clientSize = D2D1::SizeF((float)LOWORD(lp), (float)HIWORD(lp));
        if (g_rt) g_rt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
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

    case WM_NEW_ENTRIES: {
        Pane *pane = (Pane*)wp;
        auto *batch = (std::vector<FileEntry>*)lp;
        if (batch) {
            EnterCriticalSection(&pane->cs);
            for (auto &e : *batch) pane->entries.push_back(std::move(e));
            LeaveCriticalSection(&pane->cs);
            delete batch;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_SCAN_DONE: {
        Pane *pane = (Pane*)lp;
        if (pane->scanThread) { CloseHandle(pane->scanThread); pane->scanThread = nullptr; }
        pane->scanning = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        // Check divider
        if (x >= g_dividerPos - 2 && x <= g_dividerPos + 6) {
            g_draggingDivider = true;
            SetCapture(hwnd);
            return 0;
        }
        // Check which pane
        float divW = 4;
        float lw = (float)g_dividerPos;
        int paneIdx = (x < g_dividerPos) ? 0 : 1;
        Pane *pane = &g_panes[paneIdx];
        g_activePane = paneIdx;
        SetFocus(hwnd);
        // Calculate click row
        float paneX = (paneIdx == 0) ? 0 : (g_dividerPos + divW);
        float listY = g_pathH + g_headerH;
        if (y >= listY) {
            int row = (int)((y - listY) / g_rowH) + pane->scrollOffset;
            if (row >= 0 && row < (int)pane->entries.size()) {
                pane->selectedIndex = row;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (g_draggingDivider) {
            int x = GET_X_LPARAM(lp);
            int w; RECT rc; GetClientRect(hwnd, &rc); w = rc.right;
            x = (std::max)(80, (std::min)(w - 80, x));
            g_dividerPos = x;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP:
        if (g_draggingDivider) { g_draggingDivider = false; ReleaseCapture(); return 0; }
        break;

    case WM_KEYDOWN: {
        Pane *pane = &g_panes[g_activePane];
        switch (wp) {
        case VK_DOWN:
            if (pane->selectedIndex + 1 < (int)pane->entries.size()) {
                pane->selectedIndex++;
                if (pane->selectedIndex >= pane->scrollOffset + pane->visibleRows)
                    pane->scrollOffset++;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case VK_UP:
            if (pane->selectedIndex > 0) {
                pane->selectedIndex--;
                if (pane->selectedIndex < pane->scrollOffset)
                    pane->scrollOffset = (std::max)(0, pane->scrollOffset - 1);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case VK_NEXT: // Page Down
            pane->selectedIndex = (std::min)((int)pane->entries.size() - 1, pane->selectedIndex + pane->visibleRows);
            pane->scrollOffset = (std::max)(0, (std::min)((int)pane->entries.size() - pane->visibleRows, pane->selectedIndex));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case VK_PRIOR: // Page Up
            pane->selectedIndex = (std::max)(0, pane->selectedIndex - pane->visibleRows);
            pane->scrollOffset = (std::max)(0, pane->selectedIndex);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case VK_RETURN:
            OpenSelected(pane);
            return 0;
        case VK_BACK:
            GoToParent(pane);
            return 0;
        case VK_LEFT:
            if (g_activePane == 1) { g_activePane = 0; InvalidateRect(hwnd, nullptr, FALSE); }
            return 0;
        case VK_RIGHT:
            if (g_activePane == 0) { g_activePane = 1; InvalidateRect(hwnd, nullptr, FALSE); }
            return 0;
        case VK_F5:
            StartScan(pane);
            return 0;
        case VK_TAB:
            g_activePane = 1 - g_activePane;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    case WM_SETFOCUS:
        return 0;

    case WM_DESTROY:
        for (int i = 0; i < 2; ++i) {
            CancelScan(&g_panes[i]);
            g_panes[i].DoneCS();
        }
        DiscardRT();
        if (g_tf)  g_tf->Release();
        if (g_dw)  g_dw->Release();
        if (g_d2d) g_d2d->Release();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    g_hInst = hInst;

    // Check for --embedded flag (host requests hidden window for embedding)
    {
        int argc;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv && argc >= 2 && wcscmp(argv[1], L"--embedded") == 0)
            nCmdShow = SW_HIDE;
        if (argv) LocalFree(argv);
    }

    // Initialize D2D
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2d);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_dw);
    if (g_dw) g_dw->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &g_tf);
    if (g_tf) g_tf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FastDiredWindowClass";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"FastDiredWindowClass", L"FastDired",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 550,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
