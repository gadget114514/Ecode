// =============================================================================
// FastFD — Multi-pane multi-column file manager with Direct2D rendering
// Design: designdoc/first.md
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
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
#include <commctrl.h>

#define WM_NEW_ENTRIES (WM_USER + 10)
#define WM_SCAN_DONE   (WM_USER + 11)

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr float g_rowH     = 20.0f;
static constexpr float g_headerH  = 22.0f;
static constexpr float g_pathH    = 24.0f;
static constexpr float g_statusH  = 20.0f;
static constexpr float g_fkeyH    = 20.0f;
static constexpr float g_divSize  = 4.0f;
static constexpr float g_colSepHit = 3.0f;
static constexpr int   g_maxPanes = 4;

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
enum class ColType : int {
    Name, Ext, Size, SizeOnDisk, Date, Time, Attr, Type
};

enum class LayoutMode : int {
    Horizontal, Vertical, Grid2x2
};

struct Column {
    ColType type  = ColType::Name;
    float   width = 100.0f;
    bool    visible = true;
};

struct FileEntry {
    std::wstring name;
    std::wstring ext;
    int64_t      size = 0;
    FILETIME     lastWrite{};
    DWORD        attributes = 0;
    bool IsDir() const { return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
    bool IsHidden() const { return (attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0; }
    bool IsExe() const {
        if (IsDir()) return false;
        std::wstring lower = name;
        for (auto& c : lower) c = (wchar_t)towlower(c);
        size_t n = lower.size();
        return (n >= 4 && lower.substr(n - 4) == L".exe")
            || (n >= 4 && lower.substr(n - 4) == L".com")
            || (n >= 4 && lower.substr(n - 4) == L".bat")
            || (n >= 4 && lower.substr(n - 4) == L".cmd");
    }
};

struct Pane {
    std::wstring           currentPath;
    std::vector<FileEntry> entries;
    std::vector<bool>      marked;
    std::vector<Column>    columns;
    int                    selectedIndex = 0;
    int                    scrollOffset  = 0;
    int                    visibleRows   = 0;
    int                    draggingCol   = -1;
    float                  dragStartX    = 0.0f;
    ColType                sortCol       = ColType::Name;
    bool                   sortAsc       = true;
    HANDLE                 scanThread    = nullptr;
    CRITICAL_SECTION       cs;
    bool                   scanning      = false;
    HANDLE                 cancelEvent   = nullptr;

    void InitCS() {
        InitializeCriticalSection(&cs);
        cancelEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }
    void DoneCS() {
        if (cancelEvent) CloseHandle(cancelEvent);
        DeleteCriticalSection(&cs);
    }
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static HINSTANCE             g_hInst       = nullptr;
static HWND                  g_hwnd        = nullptr;
static Pane                  g_panes[g_maxPanes];
static int                   g_paneCount   = 2;
static std::vector<float>    g_dividerFractions; // proportional (0..1), len = panes-1
static int                   g_activePane  = 0;
static LayoutMode            g_layoutMode  = LayoutMode::Horizontal;
static int                   g_draggingPaneDivider = -1;
static bool                  g_draggingColHeader   = false;

// Inline edit for rename/mkdir
static HWND                  g_editHwnd    = nullptr;
static int                   g_editPane    = -1;
static int                   g_editMode    = 0; // 1=rename, 2=mkdir

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
static ID2D1SolidColorBrush* g_brushDir    = nullptr;
static ID2D1SolidColorBrush* g_brushExe    = nullptr;
static ID2D1SolidColorBrush* g_brushSys    = nullptr;
static ID2D1SolidColorBrush* g_brushMarked = nullptr;
static ID2D1SolidColorBrush* g_brushColHdr = nullptr;
static ID2D1SolidColorBrush* g_brushStatus = nullptr;
static ID2D1SolidColorBrush* g_brushFKey   = nullptr;
static ID2D1SolidColorBrush* g_brushFKeyNum= nullptr;
static ID2D1SolidColorBrush* g_brushMarkFg = nullptr;
static D2D1_SIZE_F           g_clientSize  {};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void NavigateTo(Pane* pane, const std::wstring& path);
static void StartScan(Pane* pane);
static void CancelScan(Pane* pane);
static void InitColumns(Pane& p);
static void Render();

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
static void NormalizePath(std::wstring& path) {
    wchar_t full[MAX_PATH];
    if (GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr)) {
        path = full;
    }
    if (path.size() == 2 && path[1] == L':') path += L'\\';
}

static D2D1_COLOR_F ColorFromRGB(DWORD hex) {
    return D2D1::ColorF(
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8) & 0xFF) / 255.0f,
        (hex & 0xFF) / 255.0f
    );
}

static std::wstring FormatSize(int64_t bytes) {
    wchar_t buf[32];
    if (bytes < 1024) swprintf_s(buf, L"%lld B", bytes);
    else if (bytes < 1024LL * 1024) swprintf_s(buf, L"%.1f KB", bytes / 1024.0);
    else if (bytes < 1024LL * 1024 * 1024) swprintf_s(buf, L"%.1f MB", bytes / (1024.0 * 1024));
    else swprintf_s(buf, L"%.2f GB", bytes / (1024.0 * 1024 * 1024));
    return buf;
}

static std::wstring FormatDate(FILETIME ft) {
    FILETIME lt; SYSTEMTIME st;
    FileTimeToLocalFileTime(&ft, &lt);
    FileTimeToSystemTime(&lt, &st);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return buf;
}

static std::wstring FormatAttr(DWORD attr) {
    wchar_t buf[8] = L"------";
    if (attr & FILE_ATTRIBUTE_DIRECTORY) buf[0] = L'D';
    if (attr & FILE_ATTRIBUTE_READONLY)  buf[1] = L'R';
    if (attr & FILE_ATTRIBUTE_HIDDEN)    buf[2] = L'H';
    if (attr & FILE_ATTRIBUTE_SYSTEM)    buf[3] = L'S';
    if (attr & FILE_ATTRIBUTE_ARCHIVE)   buf[4] = L'A';
    if (attr & FILE_ATTRIBUTE_COMPRESSED)buf[5] = L'C';
    return buf;
}

static std::wstring ColumnText(const FileEntry& e, ColType type) {
    switch (type) {
    case ColType::Name: return e.name;
    case ColType::Ext:  return e.ext.empty() ? L"" : e.ext.substr(1);
    case ColType::Size: return e.IsDir() ? L"<DIR>" : FormatSize(e.size);
    case ColType::SizeOnDisk: return e.IsDir() ? L"" : FormatSize(((e.size + 4095) / 4096) * 4096);
    case ColType::Date: return FormatDate(e.lastWrite);
    case ColType::Time: {
        FILETIME lt; SYSTEMTIME st;
        FileTimeToLocalFileTime(&e.lastWrite, &lt);
        FileTimeToSystemTime(&lt, &st);
        wchar_t buf[16];
        swprintf_s(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
        return buf;
    }
    case ColType::Attr: return FormatAttr(e.attributes);
    case ColType::Type: return e.IsDir() ? L"DIR" : (e.ext.empty() ? L"FILE" : e.ext.substr(1) + L" file");
    }
    return L"";
}

static int CompareFileEntries(const FileEntry& a, const FileEntry& b, ColType col, bool asc) {
    auto dirCmp = [&]() -> int {
        if (a.IsDir() != b.IsDir()) return a.IsDir() ? -1 : 1;
        return 0;
    };
    int cmp = 0;
    switch (col) {
    case ColType::Name:
        cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
        break;
    case ColType::Ext: {
        std::wstring ae = a.ext.empty() ? L"" : a.ext.substr(1);
        std::wstring be = b.ext.empty() ? L"" : b.ext.substr(1);
        cmp = _wcsicmp(ae.c_str(), be.c_str());
        break;
    }
    case ColType::Size:
    case ColType::SizeOnDisk: {
        int d = dirCmp();
        if (d != 0) return d;
        if (a.size < b.size) cmp = -1; else if (a.size > b.size) cmp = 1;
        break;
    }
    case ColType::Date:
    case ColType::Time: {
        int d = dirCmp();
        if (d != 0) return d;
        LONG64 ta = ((LONG64)a.lastWrite.dwHighDateTime << 32) | a.lastWrite.dwLowDateTime;
        LONG64 tb = ((LONG64)b.lastWrite.dwHighDateTime << 32) | b.lastWrite.dwLowDateTime;
        if (ta < tb) cmp = -1; else if (ta > tb) cmp = 1;
        break;
    }
    case ColType::Attr: {
        if (a.attributes < b.attributes) cmp = -1; else if (a.attributes > b.attributes) cmp = 1;
        break;
    }
    case ColType::Type: {
        if (a.IsDir() && !b.IsDir()) return -1;
        if (!a.IsDir() && b.IsDir()) return 1;
        cmp = _wcsicmp(a.ext.c_str(), b.ext.c_str());
        break;
    }
    }
    return asc ? cmp : -cmp;
}

// ---------------------------------------------------------------------------
// Sort
// ---------------------------------------------------------------------------
static void SortEntries(Pane* pane) {
    std::sort(pane->entries.begin(), pane->entries.end(),
        [pane](const FileEntry& a, const FileEntry& b) {
            return CompareFileEntries(a, b, pane->sortCol, pane->sortAsc) < 0;
        });
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

    auto makeBrush = [&](D2D1_COLOR_F c) -> ID2D1SolidColorBrush* {
        ID2D1SolidColorBrush* b = nullptr;
        g_rt->CreateSolidColorBrush(c, &b);
        return b;
    };

    g_brushText    = makeBrush(D2D1::ColorF(0xF0F0F0 / 255.0f, 0xF0F0F0 / 255.0f, 0xF0F0F0 / 255.0f));
    g_brushBg      = makeBrush(ColorFromRGB(0x1A1A1A));
    g_brushSel     = makeBrush(ColorFromRGB(0x000080));
    g_brushHeader  = makeBrush(D2D1::ColorF(0.2f, 0.2f, 0.22f));
    g_brushDivider = makeBrush(D2D1::ColorF(0.4f, 0.4f, 0.4f));
    g_brushDir     = makeBrush(ColorFromRGB(0x00BFFF));
    g_brushExe     = makeBrush(ColorFromRGB(0x00C800));
    g_brushSys     = makeBrush(ColorFromRGB(0xFF40FF));
    g_brushMarked  = makeBrush(ColorFromRGB(0xFFD700));
    g_brushColHdr  = makeBrush(D2D1::ColorF(0.25f, 0.25f, 0.28f));
    g_brushStatus  = makeBrush(D2D1::ColorF(0.15f, 0.15f, 0.17f));
    g_brushFKey    = makeBrush(D2D1::ColorF(0.12f, 0.12f, 0.14f));
    g_brushFKeyNum = makeBrush(ColorFromRGB(0x00BFFF));
    g_brushMarkFg  = makeBrush(ColorFromRGB(0xFFD700));
    return true;
}

static void DiscardRT() {
    auto safeRelease = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    safeRelease(g_rt);
    safeRelease(g_brushText);
    safeRelease(g_brushBg);
    safeRelease(g_brushSel);
    safeRelease(g_brushHeader);
    safeRelease(g_brushDivider);
    safeRelease(g_brushDir);
    safeRelease(g_brushExe);
    safeRelease(g_brushSys);
    safeRelease(g_brushMarked);
    safeRelease(g_brushColHdr);
    safeRelease(g_brushStatus);
    safeRelease(g_brushFKey);
    safeRelease(g_brushFKeyNum);
    safeRelease(g_brushMarkFg);
}

// ---------------------------------------------------------------------------
// Scan thread (incremental)
// ---------------------------------------------------------------------------
struct ScanParam {
    Pane* pane;
    std::wstring path;
};

static DWORD WINAPI ScanThreadProc(LPVOID lp) {
    ScanParam* sp = (ScanParam*)lp;
    Pane* pane = sp->pane;
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
    do {
        if (WaitForSingleObject(pane->cancelEvent, 0) == WAIT_OBJECT_0) {
            FindClose(hFind);
            PostMessage(g_hwnd, WM_SCAN_DONE, 0, (LPARAM)pane);
            return 0;
        }
        if (wcscmp(ffd.cFileName, L".") == 0) continue;
        FileEntry e;
        e.name = ffd.cFileName;
        e.size = ((int64_t)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow;
        e.lastWrite = ffd.ftLastWriteTime;
        e.attributes = ffd.dwFileAttributes;
        size_t dot = e.name.rfind(L'.');
        e.ext = (dot != std::wstring::npos) ? e.name.substr(dot) : L"";
        batch.push_back(std::move(e));
        if (batch.size() >= 100) {
            auto* b = new std::vector<FileEntry>(std::move(batch));
            PostMessage(g_hwnd, WM_NEW_ENTRIES, (WPARAM)pane, (LPARAM)b);
            batch.clear();
            batch.reserve(100);
        }
    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);

    if (!batch.empty()) {
        auto* b = new std::vector<FileEntry>(std::move(batch));
        PostMessage(g_hwnd, WM_NEW_ENTRIES, (WPARAM)pane, (LPARAM)b);
    }
    PostMessage(g_hwnd, WM_SCAN_DONE, 0, (LPARAM)pane);
    return 0;
}

static void StartScan(Pane* pane) {
    CancelScan(pane);
    EnterCriticalSection(&pane->cs);
    pane->entries.clear();
    pane->marked.clear();
    pane->selectedIndex = 0;
    pane->scrollOffset = 0;
    LeaveCriticalSection(&pane->cs);
    pane->scanning = true;
    ResetEvent(pane->cancelEvent);
    ScanParam* sp = new ScanParam{pane, pane->currentPath};
    pane->scanThread = CreateThread(nullptr, 0, ScanThreadProc, sp, 0, nullptr);
}

static void CancelScan(Pane* pane) {
    if (pane->scanThread) {
        SetEvent(pane->cancelEvent);
        WaitForSingleObject(pane->scanThread, 3000);
        CloseHandle(pane->scanThread);
        pane->scanThread = nullptr;
    }
    pane->scanning = false;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
static void NavigateTo(Pane* pane, const std::wstring& path) {
    std::wstring p = path;
    NormalizePath(p);
    pane->currentPath = p;
    StartScan(pane);
}

static void GoToParent(Pane* pane) {
    if (pane->currentPath.size() <= 3) return;
    size_t pos = pane->currentPath.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return;
    std::wstring parent = pane->currentPath.substr(0, pos);
    if (parent.size() == 2 && parent[1] == L':') parent += L'\\';
    NavigateTo(pane, parent);
}

static void OpenSelected(Pane* pane) {
    if (pane->entries.empty()) return;
    int idx = pane->selectedIndex;
    if (idx < 0 || idx >= (int)pane->entries.size()) return;
    FileEntry& e = pane->entries[idx];
    std::wstring full = pane->currentPath + L"\\" + e.name;
    if (e.IsDir()) {
        NavigateTo(pane, full);
    } else {
        ShellExecuteW(g_hwnd, L"open", full.c_str(), nullptr, nullptr, SW_SHOW);
    }
}

// ---------------------------------------------------------------------------
// Pane management
// ---------------------------------------------------------------------------
static void InitColumns(Pane& p) {
    p.columns.clear();
    p.columns.push_back({ColType::Name, 200.0f, true});
    p.columns.push_back({ColType::Ext,  48.0f,  true});
    p.columns.push_back({ColType::Size, 80.0f,  true});
    p.columns.push_back({ColType::Date, 100.0f, true});
    p.columns.push_back({ColType::Time, 60.0f,  true});
    p.columns.push_back({ColType::Attr, 48.0f,  true});
}

static void InitPane(Pane& p, const std::wstring& path) {
    p.currentPath = path;
    p.InitCS();
    InitColumns(p);
    p.sortCol = ColType::Name;
    p.sortAsc = true;
    p.selectedIndex = 0;
    p.scrollOffset = 0;
    p.visibleRows = 0;
    p.draggingCol = -1;
    p.scanning = false;
    p.scanThread = nullptr;
    NormalizePath(p.currentPath);
    StartScan(&p);
}

static void LayoutDividers() {
    g_dividerFractions.resize((g_paneCount > 1) ? g_paneCount - 1 : 0);
    if (g_layoutMode == LayoutMode::Grid2x2) {
        if (g_dividerFractions.size() >= 1) g_dividerFractions[0] = 0.5f;
        if (g_dividerFractions.size() >= 2) g_dividerFractions[1] = 0.5f;
    } else {
        for (int i = 0; i < (int)g_dividerFractions.size(); ++i)
            g_dividerFractions[i] = (float)(i + 1) / g_paneCount;
    }
}

static void AddPane() {
    if (g_paneCount >= g_maxPanes) return;
    int newCount = g_paneCount + 1;
    InitPane(g_panes[newCount - 1], g_panes[g_activePane].currentPath);
    g_paneCount = newCount;
    LayoutDividers();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void ClosePane() {
    if (g_paneCount <= 1) return;
    CancelScan(&g_panes[g_activePane]);
    g_panes[g_activePane].DoneCS();
    for (int i = g_activePane; i < g_paneCount - 1; ++i) {
        g_panes[i] = std::move(g_panes[i + 1]);
    }
    g_paneCount--;
    g_panes[g_paneCount].DoneCS();
    g_panes[g_paneCount] = {};
    if (g_activePane >= g_paneCount) g_activePane = g_paneCount - 1;
    LayoutDividers();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
static std::vector<D2D1_RECT_F> CalculatePaneRects() {
    std::vector<D2D1_RECT_F> rects;
    if (g_paneCount == 0) return rects;

    float w = g_clientSize.width;
    float h = g_clientSize.height - g_statusH - g_fkeyH;

    if (g_layoutMode == LayoutMode::Horizontal) {
        float divTotal = (g_paneCount - 1) * g_divSize;
        float availW = w - divTotal;
        float x = 0;
        for (int i = 0; i < g_paneCount; ++i) {
            float frac;
            if (g_paneCount == 1) {
                frac = 1.0f;
            } else if (i == 0) {
                frac = g_dividerFractions[0];
            } else if (i == g_paneCount - 1) {
                frac = 1.0f - g_dividerFractions[i - 1];
            } else {
                frac = g_dividerFractions[i] - g_dividerFractions[i - 1];
            }
            float pw = availW * frac;
            rects.push_back(D2D1::RectF(x, 0, x + pw, h));
            x += pw + g_divSize;
        }
    } else if (g_layoutMode == LayoutMode::Vertical) {
        float divTotal = (g_paneCount - 1) * g_divSize;
        float availH = h - divTotal;
        float y = 0;
        for (int i = 0; i < g_paneCount; ++i) {
            float frac;
            if (g_paneCount == 1) {
                frac = 1.0f;
            } else if (i == 0) {
                frac = g_dividerFractions[0];
            } else if (i == g_paneCount - 1) {
                frac = 1.0f - g_dividerFractions[i - 1];
            } else {
                frac = g_dividerFractions[i] - g_dividerFractions[i - 1];
            }
            float ph = availH * frac;
            rects.push_back(D2D1::RectF(0, y, w, y + ph));
            y += ph + g_divSize;
        }
    } else {
        // Grid2x2
        float hFrac = (g_dividerFractions.size() > 0) ? g_dividerFractions[0] : 0.5f;
        float vFrac = (g_dividerFractions.size() > 1) ? g_dividerFractions[1] : 0.5f;
        float w1 = (w - g_divSize) * hFrac;
        float h1 = (h - g_divSize) * vFrac;
        if (g_paneCount >= 1) rects.push_back(D2D1::RectF(0, 0, w1, h1));
        if (g_paneCount >= 2) rects.push_back(D2D1::RectF(w1 + g_divSize, 0, w, h1));
        if (g_paneCount >= 3) rects.push_back(D2D1::RectF(0, h1 + g_divSize, w1, h));
        if (g_paneCount >= 4) rects.push_back(D2D1::RectF(w1 + g_divSize, h1 + g_divSize, w, h));
    }
    return rects;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
static void DrawColumnHeaders(Pane& pane, float x, float y, float w) {
    g_rt->FillRectangle(D2D1::RectF(x, y, x + w, y + g_headerH), g_brushColHdr);
    float cx = x;
    for (size_t ci = 0; ci < pane.columns.size(); ++ci) {
        auto& col = pane.columns[ci];
        if (!col.visible) continue;

        float cw = col.width;
        D2D1_RECT_F rc = D2D1::RectF(cx + 4, y + 2, cx + cw - 2, y + g_headerH - 2);

        // Column label
        const wchar_t* label = L"";
        switch (col.type) {
        case ColType::Name: label = L"Name"; break;
        case ColType::Ext:  label = L"Ext";  break;
        case ColType::Size: label = L"Size"; break;
        case ColType::SizeOnDisk: label = L"SizeOnDisk"; break;
        case ColType::Date: label = L"Date"; break;
        case ColType::Time: label = L"Time"; break;
        case ColType::Attr: label = L"Attr"; break;
        case ColType::Type: label = L"Type"; break;
        }

        std::wstring header = label;
        if (col.type == pane.sortCol) {
            header += pane.sortAsc ? L" ▲" : L" ▼";
        }

        if (g_tf) {
            g_rt->DrawTextW(header.c_str(), (UINT32)header.size(), g_tf, rc, g_brushText);
        }

        // Column separator
        float sepX = cx + cw;
        g_rt->DrawLine(D2D1::Point2F(sepX, y), D2D1::Point2F(sepX, y + g_headerH), g_brushDivider);
        cx = sepX;
    }
}

static void DrawStatusBar(float y, float w) {
    g_rt->FillRectangle(D2D1::RectF(0, y, w, y + g_statusH), g_brushStatus);

    Pane& pane = g_panes[g_activePane];
    int markCount = 0;
    int64_t markSize = 0;
    for (size_t i = 0; i < pane.marked.size() && i < pane.entries.size(); ++i) {
        if (pane.marked[i]) {
            markCount++;
            markSize += pane.entries[i].size;
        }
    }

    std::wstring status;
    status += pane.currentPath.empty() ? L"(empty)" : pane.currentPath;
    if (markCount > 0) {
        status += L"  |  " + std::to_wstring(markCount) + L" marked (" + FormatSize(markSize) + L")";
    }
    status += L"  |  " + std::to_wstring(pane.entries.size()) + L" files";

    if (pane.scanning) {
        status += L"  |  Scanning\u2026";
    }

    if (g_tf) {
        g_rt->DrawTextW(status.c_str(), (UINT32)status.size(), g_tf,
            D2D1::RectF(4, y + 1, w - 4, y + g_statusH - 1), g_brushText);
    }
}

static void DrawFunctionKeyBar(float y, float w) {
    g_rt->FillRectangle(D2D1::RectF(0, y, w, y + g_fkeyH), g_brushFKey);

    struct FKey { const wchar_t* num; const wchar_t* label; };
    static const FKey keys[] = {
        {L"1", L"Help"},   {L"2", L"Ren"},   {L"3", L"View"},
        {L"4", L"Tree"},   {L"5", L"Copy"},   {L"6", L"Move"},
        {L"7", L"MkDir"},  {L"8", L"Del"},    {L"9", L"Refresh"},
        {L"0", L"Quit"}
    };
    int nKeys = sizeof(keys) / sizeof(keys[0]);
    float segW = w / nKeys;

    for (int i = 0; i < nKeys; ++i) {
        float sx = i * segW;
        std::wstring num(keys[i].num);
        num += keys[i].label;
        if (g_tf) {
            // Draw number in highlight color, label in text color
            // We draw the number as a separate text
            std::wstring numStr = keys[i].num;
            g_rt->DrawTextW(numStr.c_str(), (UINT32)numStr.size(), g_tf,
                D2D1::RectF(sx + 2, y + 1, sx + 20, y + g_fkeyH - 1), g_brushFKeyNum);
            g_rt->DrawTextW(keys[i].label, (UINT32)wcslen(keys[i].label), g_tf,
                D2D1::RectF(sx + 20, y + 1, sx + segW - 2, y + g_fkeyH - 1), g_brushText);
        }
        // Separator
        if (i > 0) {
            g_rt->DrawLine(D2D1::Point2F(sx, y), D2D1::Point2F(sx, y + g_fkeyH), g_brushDivider);
        }
    }
}

static void DrawPane(Pane& pane, D2D1_RECT_F rect) {
    if (!g_rt) return;
    float x = rect.left, y = rect.top, w = rect.right - rect.left, h = rect.bottom - rect.top;

    // Background
    g_rt->FillRectangle(rect, g_brushBg);

    // Path bar
    g_rt->FillRectangle(D2D1::RectF(x, y, x + w, y + g_pathH), g_brushHeader);
    if (g_tf) {
        std::wstring pathText = pane.currentPath.empty() ? L"(empty)" : pane.currentPath;
        g_rt->DrawTextW(pathText.c_str(), (UINT32)pathText.size(), g_tf,
            D2D1::RectF(x + 4, y + 2, x + w - 4, y + g_pathH - 2), g_brushText);
    }

    float listY = y + g_pathH;
    float listH = h - g_pathH;

    // Column headers
    DrawColumnHeaders(pane, x, listY, w);

    // Entries
    pane.visibleRows = (int)((listH - g_headerH) / g_rowH);
    EnterCriticalSection(&pane.cs);
    int entryCount = (int)pane.entries.size();
    pane.marked.resize(entryCount, false);

    for (int r = 0; r < pane.visibleRows; ++r) {
        int idx = pane.scrollOffset + r;
        if (idx >= entryCount) break;
        FileEntry& e = pane.entries[idx];
        bool isMarked = (idx < (int)pane.marked.size()) && pane.marked[idx];
        float ry = listY + g_headerH + r * g_rowH;

        // Selection highlight
        if (idx == pane.selectedIndex) {
            g_rt->FillRectangle(D2D1::RectF(x, ry, x + w, ry + g_rowH), g_brushSel);
        }

        // Mark indicator
        float cx = x;
        if (isMarked) {
            if (g_tf) {
                g_rt->DrawTextW(L"*", 1, g_tf,
                    D2D1::RectF(cx + 2, ry + 1, cx + 14, ry + g_rowH - 1), g_brushMarkFg);
            }
            cx += 14;
        }

        // Determine text color
        ID2D1SolidColorBrush* textBrush = g_brushText;
        if (isMarked) {
            textBrush = g_brushMarked;
        } else {
            if (e.IsDir()) textBrush = g_brushDir;
            else if (e.IsExe()) textBrush = g_brushExe;
            else if (e.IsHidden()) textBrush = g_brushSys;
        }

        // Draw columns
        for (size_t ci = 0; ci < pane.columns.size(); ++ci) {
            auto& col = pane.columns[ci];
            if (!col.visible) continue;

            std::wstring text = ColumnText(e, col.type);
            if (g_tf) {
                g_rt->DrawTextW(text.c_str(), (UINT32)text.size(), g_tf,
                    D2D1::RectF(cx + 4, ry + 1, cx + col.width - 4, ry + g_rowH - 1), textBrush);
            }
            cx += col.width;
        }
    }
    LeaveCriticalSection(&pane.cs);
}

static void DrawPaneDividers(const std::vector<D2D1_RECT_F>& rects) {
    if (g_paneCount <= 1) return;

    if (g_layoutMode == LayoutMode::Horizontal) {
        for (int i = 0; i < g_paneCount - 1; ++i) {
            float dx = rects[i].right;
            g_rt->FillRectangle(D2D1::RectF(dx, 0, dx + g_divSize,
                g_clientSize.height - g_statusH - g_fkeyH), g_brushDivider);
        }
    } else if (g_layoutMode == LayoutMode::Vertical) {
        for (int i = 0; i < g_paneCount - 1; ++i) {
            float dy = rects[i].bottom;
            g_rt->FillRectangle(D2D1::RectF(0, dy, g_clientSize.width, dy + g_divSize),
                g_brushDivider);
        }
    } else if (g_layoutMode == LayoutMode::Grid2x2 && g_paneCount >= 3) {
        // Vertical divider
        float vx = rects[0].right;
        g_rt->FillRectangle(D2D1::RectF(vx, 0, vx + g_divSize,
            g_clientSize.height - g_statusH - g_fkeyH), g_brushDivider);
        if (g_paneCount >= 3) {
            // Horizontal divider
            float hy = rects[0].bottom;
            g_rt->FillRectangle(D2D1::RectF(0, hy, g_clientSize.width, hy + g_divSize),
                g_brushDivider);
        }
    }
}

static void Render() {
    if (!EnsureRT()) return;
    g_rt->BeginDraw();
    g_rt->Clear(D2D1::ColorF(0.1f, 0.1f, 0.1f));

    auto rects = CalculatePaneRects();
    for (int i = 0; i < g_paneCount && i < (int)rects.size(); ++i) {
        DrawPane(g_panes[i], rects[i]);
    }
    DrawPaneDividers(rects);

    float bottomY = g_clientSize.height;
    DrawStatusBar(bottomY - g_statusH - g_fkeyH, g_clientSize.width);
    DrawFunctionKeyBar(bottomY - g_fkeyH, g_clientSize.width);

    HRESULT hr = g_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardRT();
    }
}

// ---------------------------------------------------------------------------
// Inline edit control for rename / mkdir
// ---------------------------------------------------------------------------
static void ShowEdit(int mode, const wchar_t* initialText, const wchar_t* /*prefix*/) {
    if (g_editHwnd) DestroyWindow(g_editHwnd);
    g_editMode = mode;
    g_editPane = g_activePane;

    float bottomY = g_clientSize.height;
    RECT rc = {
        100, (LONG)(bottomY - g_statusH - g_fkeyH + 1),
        (LONG)(g_clientSize.width - 100), (LONG)(bottomY - g_fkeyH - 1)
    };
    g_editHwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", initialText,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        g_hwnd, nullptr, g_hInst, nullptr);
    if (g_editHwnd) {
        SetWindowFont(g_editHwnd, GetStockFont(DEFAULT_GUI_FONT), TRUE);
        SetFocus(g_editHwnd);
        SendMessageW(g_editHwnd, EM_SETSEL, 0, -1);
    }
}

static void HideEdit() {
    if (g_editHwnd) {
        DestroyWindow(g_editHwnd);
        g_editHwnd = nullptr;
    }
    g_editMode = 0;
    g_editPane = -1;
    SetFocus(g_hwnd);
}

static void CommitEdit() {
    if (!g_editHwnd || g_editPane < 0 || g_editPane >= g_paneCount) return;
    int len = GetWindowTextLengthW(g_editHwnd);
    if (len == 0) { HideEdit(); return; }
    std::wstring text(len + 1, L'\0');
    GetWindowTextW(g_editHwnd, &text[0], len + 1);
    text.resize(len);

    Pane& pane = g_panes[g_editPane];

    if (g_editMode == 1) {
        // Rename
        if (pane.selectedIndex >= 0 && pane.selectedIndex < (int)pane.entries.size()) {
            std::wstring oldPath = pane.currentPath + L"\\" + pane.entries[pane.selectedIndex].name;
            std::wstring newPath = pane.currentPath + L"\\" + text;
            if (MoveFileW(oldPath.c_str(), newPath.c_str())) {
                StartScan(&pane);
            }
        }
    } else if (g_editMode == 2) {
        // MkDir
        std::wstring newPath = pane.currentPath + L"\\" + text;
        if (CreateDirectoryW(newPath.c_str(), nullptr)) {
            StartScan(&pane);
        }
    } else if (g_editMode == 3 || g_editMode == 4) {
        // Mark / unmark by glob
        pane.marked.resize(pane.entries.size(), false);
        for (size_t i = 0; i < pane.entries.size(); ++i) {
            if (PathMatchSpecW(pane.entries[i].name.c_str(), text.c_str())) {
                pane.marked[i] = (g_editMode == 3);
            }
        }
        InvalidateRect(g_hwnd, nullptr, FALSE);
    }
    HideEdit();
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------
static void DoCopyMove(bool move) {
    Pane& src = g_panes[g_activePane];
    int dstIdx = (g_paneCount > 1) ? (g_activePane + 1) % g_paneCount : -1;
    if (dstIdx < 0) return;
    Pane& dst = g_panes[dstIdx];
    std::wstring dstPath = dst.currentPath;

    // Collect files to operate on
    std::vector<std::wstring> sources;
    bool hasMarks = false;
    for (size_t i = 0; i < src.marked.size(); ++i) {
        if (src.marked[i]) { hasMarks = true; break; }
    }
    if (hasMarks) {
        for (size_t i = 0; i < src.marked.size() && i < src.entries.size(); ++i) {
            if (src.marked[i])
                sources.push_back(src.currentPath + L"\\" + src.entries[i].name);
        }
    } else if (src.selectedIndex >= 0 && src.selectedIndex < (int)src.entries.size()) {
        sources.push_back(src.currentPath + L"\\" + src.entries[src.selectedIndex].name);
    }
    if (sources.empty()) return;

    std::wstring msg = (move ? L"Move " : L"Copy ") + std::to_wstring(sources.size())
        + L" file(s) to " + dstPath + L"? [Y/N]";
    int ret = MessageBoxW(g_hwnd, msg.c_str(), L"Confirm", MB_YESNO | MB_ICONQUESTION);
    if (ret != IDYES) return;

    for (auto& srcPath : sources) {
        if (move) {
            MoveFileW(srcPath.c_str(), (dstPath + L"\\" + srcPath.substr(srcPath.find_last_of(L'\\') + 1)).c_str());
        } else {
            CopyFileW(srcPath.c_str(), (dstPath + L"\\" + srcPath.substr(srcPath.find_last_of(L'\\') + 1)).c_str(), FALSE);
        }
    }
    StartScan(&src);
    StartScan(&dst);
}

static void DoDelete() {
    Pane& pane = g_panes[g_activePane];
    std::vector<std::wstring> targets;
    bool hasMarks = false;
    for (size_t i = 0; i < pane.marked.size(); ++i) {
        if (pane.marked[i]) { hasMarks = true; break; }
    }
    if (hasMarks) {
        for (size_t i = 0; i < pane.marked.size() && i < pane.entries.size(); ++i) {
            if (pane.marked[i])
                targets.push_back(pane.currentPath + L"\\" + pane.entries[i].name);
        }
    } else if (pane.selectedIndex >= 0 && pane.selectedIndex < (int)pane.entries.size()) {
        targets.push_back(pane.currentPath + L"\\" + pane.entries[pane.selectedIndex].name);
    }
    if (targets.empty()) return;

    std::wstring msg;
    if (targets.size() == 1) {
        msg = L"Delete \"" + targets[0] + L"\"? [Y/N]";
    } else {
        msg = L"Delete " + std::to_wstring(targets.size()) + L" files? [Y/N]";
    }
    int ret = MessageBoxW(g_hwnd, msg.c_str(), L"Confirm", MB_YESNO | MB_ICONWARNING);
    if (ret != IDYES) return;

    // Use SHFileOperation for Recycle Bin
    std::vector<wchar_t> buf;
    for (auto& t : targets) {
        for (auto c : t) buf.push_back(c);
        buf.push_back(0);
    }
    buf.push_back(0);

    SHFILEOPSTRUCTW op = {};
    op.hwnd = g_hwnd;
    op.wFunc = FO_DELETE;
    op.pFrom = buf.data();
    op.fFlags = FOF_ALLOWUNDO | FOF_NO_UI;
    SHFileOperationW(&op);
    StartScan(&pane);
}

// ---------------------------------------------------------------------------
// Mark helpers
// ---------------------------------------------------------------------------
static void ToggleMark(Pane& pane) {
    if (pane.entries.empty()) return;
    int idx = pane.selectedIndex;
    if (idx < 0 || idx >= (int)pane.entries.size()) return;
    pane.marked.resize(pane.entries.size(), false);
    pane.marked[idx] = !pane.marked[idx];
    // Advance cursor
    if (pane.selectedIndex + 1 < (int)pane.entries.size()) {
        pane.selectedIndex++;
        if (pane.selectedIndex >= pane.scrollOffset + pane.visibleRows)
            pane.scrollOffset++;
    }
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void InvertMarks(Pane& pane) {
    pane.marked.resize(pane.entries.size(), false);
    for (size_t i = 0; i < pane.entries.size(); ++i) {
        pane.marked[i] = !pane.marked[i];
    }
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        g_hwnd = hwnd;
        for (int i = 0; i < g_maxPanes; ++i) {
            g_panes[i].InitCS();
        }
        wchar_t curDir[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, curDir);
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

        InitPane(g_panes[0], (argv && argc > 1) ? argv[1] : curDir);
        std::wstring rightPath = (argv && argc > 2) ? argv[2] : L"";
        if (argv) LocalFree(argv);

        if (!rightPath.empty() && GetFileAttributesW(rightPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            InitPane(g_panes[1], rightPath);
        } else {
            InitPane(g_panes[1], curDir);
        }
        g_activePane = 0;
        g_paneCount = 2;
        LayoutDividers();
        DragAcceptFiles(hwnd, TRUE);
        SetFocus(hwnd);
        return 0;
    }

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wp;
        POINT pt;
        DragQueryPoint(hDrop, &pt);
        auto rects = CalculatePaneRects();
        int paneIdx = 0;
        for (int i = 0; i < (int)rects.size() && i < g_paneCount; ++i) {
            if (pt.x >= rects[i].left && pt.x < rects[i].right &&
                pt.y >= rects[i].top && pt.y < rects[i].bottom) {
                paneIdx = i;
                break;
            }
        }
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
        if (g_editHwnd) {
            float bottomY = g_clientSize.height;
            RECT rc = { 100, (LONG)(bottomY - g_statusH - g_fkeyH + 1),
                (LONG)(g_clientSize.width - 100), (LONG)(bottomY - g_fkeyH - 1) };
            SetWindowPos(g_editHwnd, nullptr, rc.left, rc.top,
                rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);
        }
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
        Pane* pane = (Pane*)wp;
        auto* batch = (std::vector<FileEntry>*)lp;
        if (batch) {
            EnterCriticalSection(&pane->cs);
            for (auto& e : *batch) pane->entries.push_back(std::move(e));
            // After adding entries, sort
            SortEntries(pane);
            pane->marked.resize(pane->entries.size(), false);
            LeaveCriticalSection(&pane->cs);
            delete batch;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_SCAN_DONE: {
        Pane* pane = (Pane*)lp;
        if (pane->scanThread) { CloseHandle(pane->scanThread); pane->scanThread = nullptr; }
        pane->scanning = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (g_editHwnd) { HideEdit(); return 0; }
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);

        // Check pane dividers
        if (g_layoutMode == LayoutMode::Horizontal) {
            auto rects = CalculatePaneRects();
            for (int i = 0; i < g_paneCount - 1; ++i) {
                float dx = rects[i].right;
                if (x >= dx && x <= dx + g_divSize) {
                    g_draggingPaneDivider = i;
                    SetCapture(hwnd);
                    return 0;
                }
            }
        } else if (g_layoutMode == LayoutMode::Vertical) {
            auto rects = CalculatePaneRects();
            for (int i = 0; i < g_paneCount - 1; ++i) {
                float dy = rects[i].bottom;
                if (y >= dy && y <= dy + g_divSize) {
                    g_draggingPaneDivider = i;
                    SetCapture(hwnd);
                    return 0;
                }
            }
        } else if (g_layoutMode == LayoutMode::Grid2x2) {
            // Check the two dividers
            auto rects = CalculatePaneRects();
            if (g_paneCount >= 2) {
                float vx = rects[0].right;
                if (x >= vx && x <= vx + g_divSize) {
                    g_draggingPaneDivider = 0;
                    SetCapture(hwnd);
                    return 0;
                }
            }
            if (g_paneCount >= 3) {
                float hy = rects[0].bottom;
                if (y >= hy && y <= hy + g_divSize) {
                    g_draggingPaneDivider = 2;
                    SetCapture(hwnd);
                    return 0;
                }
            }
        }

        // Check column header drag in active pane
        {
            auto rects = CalculatePaneRects();
            if (g_activePane >= 0 && g_activePane < (int)rects.size()) {
                auto& r = rects[g_activePane];
                Pane& pane = g_panes[g_activePane];
                float listY = r.top + g_pathH;
                if (y >= listY && y < listY + g_headerH) {
                    // Check column separators
                    float cx = r.left;
                    for (size_t ci = 0; ci < pane.columns.size(); ++ci) {
                        if (!pane.columns[ci].visible) continue;
                        float cw = pane.columns[ci].width;
                        float sepX = cx + cw;
                        if (x >= sepX - g_colSepHit && x <= sepX + g_colSepHit) {
                            pane.draggingCol = (int)ci;
                            pane.dragStartX = (float)x;
                            g_draggingColHeader = true;
                            SetCapture(hwnd);
                            return 0;
                        }
                        cx = sepX;
                    }
                    // Column header click for sort
                    cx = r.left;
                    for (size_t ci = 0; ci < pane.columns.size(); ++ci) {
                        if (!pane.columns[ci].visible) continue;
                        float cw = pane.columns[ci].width;
                        if (x >= cx && x < cx + cw) {
                            if (pane.sortCol == pane.columns[ci].type) {
                                pane.sortAsc = !pane.sortAsc;
                            } else {
                                pane.sortCol = pane.columns[ci].type;
                                pane.sortAsc = true;
                            }
                            SortEntries(&pane);
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return 0;
                        }
                        cx += cw;
                    }
                }
            }
        }

        // Determine which pane was clicked
        auto rects = CalculatePaneRects();
        int paneIdx = -1;
        for (int i = 0; i < (int)rects.size() && i < g_paneCount; ++i) {
            if (x >= rects[i].left && x < rects[i].right &&
                y >= rects[i].top && y < rects[i].bottom) {
                paneIdx = i;
                break;
            }
        }
        if (paneIdx < 0) return 0;
        g_activePane = paneIdx;
        Pane& pane = g_panes[paneIdx];
        SetFocus(hwnd);

        // Calculate click row in file list
        float listY = rects[paneIdx].top + g_pathH + g_headerH;
        if (y >= listY) {
            int row = (int)((y - listY) / g_rowH) + pane.scrollOffset;
            if (row >= 0 && row < (int)pane.entries.size()) {
                pane.selectedIndex = row;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    }

    case WM_RBUTTONDOWN: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        auto rects = CalculatePaneRects();
        if (g_activePane >= 0 && g_activePane < (int)rects.size()) {
            auto& r = rects[g_activePane];
            Pane& pane = g_panes[g_activePane];
            float listY = r.top + g_pathH;
            if (y >= listY && y < listY + g_headerH) {
                // Check which column was right-clicked
                float cx = r.left;
                int colIdx = -1;
                for (size_t ci = 0; ci < pane.columns.size(); ++ci) {
                    if (!pane.columns[ci].visible) continue;
                    if (x >= cx && x < cx + pane.columns[ci].width) {
                        colIdx = (int)ci;
                        break;
                    }
                    cx += pane.columns[ci].width;
                }
                if (colIdx >= 0) {
                    // Show context menu for column visibility
                    HMENU menu = CreatePopupMenu();
                    for (size_t ci = 0; ci < pane.columns.size(); ++ci) {
                        std::wstring label;
                        switch (pane.columns[ci].type) {
                        case ColType::Name: label = L"&Name"; break;
                        case ColType::Ext:  label = L"E&xt"; break;
                        case ColType::Size: label = L"&Size"; break;
                        case ColType::SizeOnDisk: label = L"Size on &Disk"; break;
                        case ColType::Date: label = L"&Date"; break;
                        case ColType::Time: label = L"&Time"; break;
                        case ColType::Attr: label = L"&Attr"; break;
                        case ColType::Type: label = L"T&ype"; break;
                        }
                        UINT flags = MF_STRING;
                        if (pane.columns[ci].visible) flags |= MF_CHECKED;
                        AppendMenuW(menu, flags, 100 + (UINT)ci, label.c_str());
                    }
                    POINT pt = { x, y };
                    ClientToScreen(hwnd, &pt);
                    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
                    if (cmd >= 100) {
                        int ci = cmd - 100;
                        if (ci >= 0 && ci < (int)pane.columns.size()) {
                            pane.columns[ci].visible = !pane.columns[ci].visible;
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    }
                    DestroyMenu(menu);
                    return 0;
                }
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (g_draggingPaneDivider >= 0) {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            float w = g_clientSize.width;
            float h = g_clientSize.height - g_statusH - g_fkeyH;

            if (g_layoutMode == LayoutMode::Horizontal) {
                float minX = 80.0f, maxX = w - 80.0f;
                float clampedX = (std::max)(minX, (std::min)(maxX, (float)x));
                // Convert to fraction
                float availW = w - (g_paneCount - 1) * g_divSize;
                float frac = clampedX / availW;
                // Adjust adjacent dividers
                if (g_draggingPaneDivider == 0) {
                    if (g_dividerFractions.size() > 0) g_dividerFractions[0] = (std::max)(0.1f, (std::min)(0.9f, frac));
                } else if (g_draggingPaneDivider < (int)g_dividerFractions.size()) {
                    float prev = g_dividerFractions[g_draggingPaneDivider - 1];
                    float next = (g_draggingPaneDivider + 1 < (int)g_dividerFractions.size())
                        ? g_dividerFractions[g_draggingPaneDivider + 1] : 1.0f;
                    float minF = prev + 0.05f;
                    float maxF = next - 0.05f;
                    g_dividerFractions[g_draggingPaneDivider] = (std::max)(minF, (std::min)(maxF, frac));
                }
            } else if (g_layoutMode == LayoutMode::Vertical) {
                float minY = 80.0f, maxY = h - 80.0f;
                float clampedY = (std::max)(minY, (std::min)(maxY, (float)y));
                float availH = h - (g_paneCount - 1) * g_divSize;
                float frac = clampedY / availH;
                if (g_draggingPaneDivider == 0) {
                    if (g_dividerFractions.size() > 0) g_dividerFractions[0] = (std::max)(0.1f, (std::min)(0.9f, frac));
                } else if (g_draggingPaneDivider < (int)g_dividerFractions.size()) {
                    float prev = g_dividerFractions[g_draggingPaneDivider - 1];
                    float next = (g_draggingPaneDivider + 1 < (int)g_dividerFractions.size())
                        ? g_dividerFractions[g_draggingPaneDivider + 1] : 1.0f;
                    float minF = prev + 0.05f;
                    float maxF = next - 0.05f;
                    g_dividerFractions[g_draggingPaneDivider] = (std::max)(minF, (std::min)(maxF, frac));
                }
            } else if (g_layoutMode == LayoutMode::Grid2x2) {
                if (g_draggingPaneDivider == 0 && g_dividerFractions.size() > 0) {
                    float minX = 80.0f, maxX = w - 80.0f;
                    float clampedX = (std::max)(minX, (std::min)(maxX, (float)x));
                    g_dividerFractions[0] = clampedX / w;
                } else if (g_draggingPaneDivider == 2 && g_dividerFractions.size() > 1) {
                    float minY = 80.0f, maxY = h - 80.0f;
                    float clampedY = (std::max)(minY, (std::min)(maxY, (float)y));
                    g_dividerFractions[1] = clampedY / h;
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (g_draggingColHeader) {
            if (g_activePane >= 0 && g_activePane < g_paneCount) {
                Pane& pane = g_panes[g_activePane];
                if (pane.draggingCol >= 0 && pane.draggingCol < (int)pane.columns.size()) {
                    int x = GET_X_LPARAM(lp);
                    float delta = (float)x - pane.dragStartX;
                    float newWidth = pane.columns[pane.draggingCol].width + delta;
                    newWidth = (std::max)(30.0f, newWidth);
                    pane.columns[pane.draggingCol].width = newWidth;
                    pane.dragStartX = (float)x;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }

        // Update cursor for dividers
        {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            bool onDivider = false;
            if (g_layoutMode == LayoutMode::Horizontal) {
                auto rects = CalculatePaneRects();
                for (int i = 0; i < g_paneCount - 1 && i < (int)rects.size(); ++i) {
                    float dx = rects[i].right;
                    if (x >= dx - 2 && x <= dx + g_divSize + 2) { onDivider = true; break; }
                }
            } else if (g_layoutMode == LayoutMode::Vertical) {
                auto rects = CalculatePaneRects();
                for (int i = 0; i < g_paneCount - 1 && i < (int)rects.size(); ++i) {
                    float dy = rects[i].bottom;
                    if (y >= dy - 2 && y <= dy + g_divSize + 2) { onDivider = true; break; }
                }
            }
            SetCursor(LoadCursor(nullptr, onDivider ? IDC_SIZEWE : IDC_ARROW));
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_draggingPaneDivider >= 0) {
            g_draggingPaneDivider = -1;
            ReleaseCapture();
            return 0;
        }
        if (g_draggingColHeader) {
            g_draggingColHeader = false;
            if (g_activePane >= 0 && g_activePane < g_paneCount)
                g_panes[g_activePane].draggingCol = -1;
            ReleaseCapture();
            return 0;
        }
        return 0;
    }

    case WM_KEYDOWN: {
        if (g_paneCount <= 0) return 0;
        Pane& pane = g_panes[g_activePane];

        switch (wp) {

        case VK_DOWN:
            if (pane.selectedIndex + 1 < (int)pane.entries.size()) {
                pane.selectedIndex++;
                if (pane.selectedIndex >= pane.scrollOffset + pane.visibleRows)
                    pane.scrollOffset++;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case VK_UP:
            if (pane.selectedIndex > 0) {
                pane.selectedIndex--;
                if (pane.selectedIndex < pane.scrollOffset)
                    pane.scrollOffset = (std::max)(0, pane.scrollOffset - 1);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case VK_NEXT:
            pane.selectedIndex = (std::min)((int)pane.entries.size() - 1, pane.selectedIndex + pane.visibleRows);
            pane.scrollOffset = (std::max)(0, (std::min)((int)pane.entries.size() - pane.visibleRows, pane.selectedIndex));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case VK_PRIOR:
            pane.selectedIndex = (std::max)(0, pane.selectedIndex - pane.visibleRows);
            pane.scrollOffset = (std::max)(0, pane.selectedIndex);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case VK_HOME: {
            pane.selectedIndex = 0;
            pane.scrollOffset = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case VK_END: {
            pane.selectedIndex = (std::max)(0, (int)pane.entries.size() - 1);
            pane.scrollOffset = (std::max)(0, (int)pane.entries.size() - pane.visibleRows);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case VK_RETURN:
            OpenSelected(&pane);
            return 0;

        case VK_BACK:
            GoToParent(&pane);
            return 0;

        case VK_TAB: {
            int mod = GetKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
            g_activePane = (g_activePane + mod + g_paneCount) % g_paneCount;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case VK_SPACE:
            ToggleMark(pane);
            return 0;

        case VK_F1:
            MessageBoxW(hwnd, L"FastFD — Fast Directory Manipulator\n\n"
                L"F1 Help   F2 Rename   F3 View   F4 Tree\n"
                L"F5 Copy   F6 Move   F7 MkDir   F8 Delete\n"
                L"F9 Refresh   F10 Quit\n\n"
                L"Ctrl+P Add Pane   Ctrl+W Close Pane\n"
                L"Alt+H/V/G Layout   Ctrl+H Toggle Hidden",
                L"FastFD Help", MB_OK | MB_ICONINFORMATION);
            return 0;

        case VK_F2:
            // Rename
            if (!pane.entries.empty() && pane.selectedIndex >= 0) {
                ShowEdit(1, pane.entries[pane.selectedIndex].name.c_str(), L"Rename");
            }
            return 0;

        case VK_F4:
            // Tree view overlay (placeholder)
            MessageBoxW(hwnd, L"Directory tree view (coming soon)", L"Tree View", MB_OK);
            return 0;

        case VK_F5:
            DoCopyMove(false);
            return 0;

        case VK_F6:
            DoCopyMove(true);
            return 0;

        case VK_F7:
            ShowEdit(2, L"New Folder", L"New folder");
            return 0;

        case VK_F8:
            DoDelete();
            return 0;

        case VK_F9:
            StartScan(&pane);
            return 0;

        case VK_F10:
            SendMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;

        case VK_OEM_PLUS:
        case VK_ADD: {
            // Mark by glob - use a simple approach
            wchar_t pattern[256] = L"*.*";
            ShowEdit(3, pattern, L"Mark files matching: ");
            g_editMode = 3;
            // We'll handle this in WM_CHAR or via the edit commit
            return 0;
        }

        case VK_OEM_MINUS:
        case VK_SUBTRACT: {
            wchar_t pattern[256] = L"*.*";
            ShowEdit(4, pattern, L"Unmark files matching: ");
            g_editMode = 4;
            return 0;
        }

        case VK_MULTIPLY:
            InvertMarks(pane);
            return 0;

        default:
            if (wp >= 'A' && wp <= 'Z' && GetKeyState(VK_CONTROL) & 0x8000) {
                switch (wp) {
                case 'P': AddPane(); return 0;
                case 'W': ClosePane(); return 0;
                case 'H': {
                    // Toggle show hidden files - simple refresh
                    StartScan(&pane);
                    return 0;
                }
                case 'S': {
                    // Sort menu
                    HMENU menu = CreatePopupMenu();
                    AppendMenuW(menu, MF_STRING, 200, L"Sort by &Name");
                    AppendMenuW(menu, MF_STRING, 201, L"Sort by Si&ze");
                    AppendMenuW(menu, MF_STRING, 202, L"Sort by T&ype");
                    AppendMenuW(menu, MF_STRING, 203, L"Sort by &Date");
                    POINT pt; GetCursorPos(&pt);
                    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
                    if (cmd >= 200) {
                        static const ColType sortMap[] = { ColType::Name, ColType::Size, ColType::Type, ColType::Date };
                        int idx = cmd - 200;
                        if (idx >= 0 && idx < 4) {
                            if (pane.sortCol == sortMap[idx])
                                pane.sortAsc = !pane.sortAsc;
                            else {
                                pane.sortCol = sortMap[idx];
                                pane.sortAsc = true;
                            }
                            SortEntries(&pane);
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    }
                    DestroyMenu(menu);
                    return 0;
                }
                }
            }
            if (wp >= '0' && wp <= '9' && GetKeyState(VK_MENU) & 0x8000) {
                // Alt+number would switch layout modes, but we already use Alt+H/V/G
                return 0;
            }
            return 0;
        }
        return 0;
    }

    case WM_CHAR: {
        if (g_editHwnd) {
            if (wp == VK_RETURN) {
                CommitEdit();
                return 0;
            } else if (wp == VK_ESCAPE) {
                HideEdit();
                return 0;
            }
            // Forward to EDIT control
            SendMessageW(g_editHwnd, WM_CHAR, wp, lp);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    case WM_SYSKEYDOWN: {
        if (wp >= 'A' && wp <= 'Z' && (GetKeyState(VK_MENU) & 0x8000)) {
            switch (wp) {
            case 'H': g_layoutMode = LayoutMode::Horizontal; LayoutDividers(); InvalidateRect(hwnd, nullptr, FALSE); return 0;
            case 'V': g_layoutMode = LayoutMode::Vertical; LayoutDividers(); InvalidateRect(hwnd, nullptr, FALSE); return 0;
            case 'G':
                g_layoutMode = LayoutMode::Grid2x2;
                if (g_paneCount < 4) {
                    while (g_paneCount < 4) AddPane();
                }
                LayoutDividers();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    case WM_SETFOCUS:
        return 0;

    case WM_DESTROY:
        for (int i = 0; i < g_maxPanes; ++i) {
            CancelScan(&g_panes[i]);
            g_panes[i].DoneCS();
        }
        if (g_editHwnd) DestroyWindow(g_editHwnd);
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

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2d);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_dw);
    if (g_dw) {
        g_dw->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &g_tf);
    }
    if (g_tf) g_tf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FastFDWindowClass";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"FastFDWindowClass", L"FastFD",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 600,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.hwnd == g_editHwnd || !IsDialogMessage(g_hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
