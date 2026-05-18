// =============================================================================
// Dired — Two-pane directory browser for Ecode
// Pure Win32, virtual ListView, drag-drop, context menu
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
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>

#define IDC_LIST_LEFT    101
#define IDC_LIST_RIGHT   102
#define IDC_PATH_LEFT    103
#define IDC_PATH_RIGHT   104
#define IDC_STATUS       105
#define ID_OPEN          2001
#define ID_DELETE        2002
#define ID_RENAME        2003
#define ID_COPY_PATH     2004
#define ID_PROPERTIES    2005
#define ID_MKDIR         2006
#define ID_COPY          2007
#define ID_MOVE          2008
#define ID_REFRESH       2009
#define ID_PARENT        2010
#define WM_SCAN_DONE     (WM_USER + 10)

enum SortColumn { SORT_NAME, SORT_SIZE, SORT_TYPE, SORT_DATE };

struct FileEntry {
    std::wstring name;
    std::wstring ext;
    int64_t      size;
    FILETIME     lastWrite;
    DWORD        attributes;
    int          iconIndex;
    bool IsDir() const { return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
};

struct Pane {
    HWND          hwndList = nullptr;
    HWND          hwndPath = nullptr;
    std::wstring  currentPath;
    std::vector<FileEntry> entries;
    SortColumn    sortCol = SORT_NAME;
    bool          sortAsc = true;
    HANDLE        scanThread = nullptr;
    CRITICAL_SECTION cs;
    int           activePane = 0; // 0=left, 1=right

    void InitCS() { InitializeCriticalSection(&cs); }
    void DoneCS() { DeleteCriticalSection(&cs); }
};

static HINSTANCE   g_hInst = nullptr;
static HWND        g_hwnd = nullptr;
static HWND        g_hStatus = nullptr;
static HIMAGELIST  g_himlSmall = nullptr;
static Pane        g_panes[2];
static int         g_dragging = 0; // 0=not dragging, 1=dragging from left, 2=from right
static int         g_dragItems = 0; // number of items being dragged
static int         g_dividerPos = -1; // divider X position
static bool        g_draggingDivider = false;

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
static Pane* GetPane(HWND hList) {
    if (hList == g_panes[0].hwndList) return &g_panes[0];
    if (hList == g_panes[1].hwndList) return &g_panes[1];
    return nullptr;
}

static int CompareEntries(const FileEntry &a, const FileEntry &b, SortColumn col, bool asc) {
    if (a.IsDir() != b.IsDir()) return a.IsDir() ? -1 : 1;
    int r = 0;
    switch (col) {
    case SORT_NAME: r = _wcsicmp(a.name.c_str(), b.name.c_str()); break;
    case SORT_SIZE: r = (a.size > b.size) ? 1 : (a.size < b.size) ? -1 : 0; break;
    case SORT_TYPE: r = _wcsicmp(a.ext.c_str(), b.ext.c_str()); break;
    case SORT_DATE: r = CompareFileTime(&a.lastWrite, &b.lastWrite); break;
    }
    return asc ? r : -r;
}

// ---------------------------------------------------------------------------
// Scan thread
// ---------------------------------------------------------------------------
struct ScanParam { Pane *pane; };

static DWORD WINAPI ScanThreadProc(LPVOID lp) {
    ScanParam *sp = (ScanParam*)lp;
    Pane *pane = sp->pane;
    delete sp;

    std::vector<FileEntry> results;
    std::wstring searchPath = pane->currentPath + L"\\*";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(ffd.cFileName, L".") == 0) continue;
            FileEntry e;
            e.name = ffd.cFileName;
            e.size = ((int64_t)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow;
            e.lastWrite = ffd.ftLastWriteTime;
            e.attributes = ffd.dwFileAttributes;
            e.iconIndex = -1;
            size_t dot = e.name.rfind(L'.');
            e.ext = (dot != std::wstring::npos) ? e.name.substr(dot) : L"";
            results.push_back(std::move(e));
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
    }

    EnterCriticalSection(&pane->cs);
    pane->entries = std::move(results);
    LeaveCriticalSection(&pane->cs);

    std::sort(pane->entries.begin(), pane->entries.end(),
        [pane](const FileEntry &a, const FileEntry &b) {
            return CompareEntries(a, b, pane->sortCol, pane->sortAsc);
        });

    PostMessage(g_hwnd, WM_SCAN_DONE, 0, (LPARAM)pane);
    return 0;
}

static void StartScan(Pane *pane) {
    if (pane->scanThread) {
        WaitForSingleObject(pane->scanThread, 2000);
        CloseHandle(pane->scanThread);
        pane->scanThread = nullptr;
    }
    pane->entries.clear();
    ListView_SetItemCount(pane->hwndList, 0);
    ScanParam *sp = new ScanParam{pane};
    pane->scanThread = CreateThread(nullptr, 0, ScanThreadProc, sp, 0, nullptr);
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
static void NavigateTo(Pane *pane, const std::wstring &path) {
    pane->currentPath = path;
    SetWindowTextW(pane->hwndPath, path.c_str());
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

static void OpenItem(Pane *pane) {
    int sel = ListView_GetNextItem(pane->hwndList, -1, LVNI_SELECTED);
    if (sel < 0 || (size_t)sel >= pane->entries.size()) return;
    FileEntry &e = pane->entries[sel];
    std::wstring full = pane->currentPath + L"\\" + e.name;
    if (e.IsDir()) {
        NavigateTo(pane, full);
    } else {
        ShellExecuteW(pane->hwndList, L"open", full.c_str(), nullptr, nullptr, SW_SHOW);
    }
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------
static void ShowContextMenu(HWND hwnd, Pane *pane, int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_OPEN,       L"Open\tEnter");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_COPY,       L"Copy to Other Pane\tF5");
    AppendMenuW(hMenu, MF_STRING, ID_MOVE,       L"Move to Other Pane\tF6");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_MKDIR,      L"New Folder...\tF7");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_DELETE,     L"Delete\tDel");
    AppendMenuW(hMenu, MF_STRING, ID_RENAME,     L"Rename\tF2");
    AppendMenuW(hMenu, MF_STRING, ID_COPY_PATH,  L"Copy Path");
    AppendMenuW(hMenu, MF_STRING, ID_PROPERTIES, L"Properties\tAlt+Enter");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                             x, y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
    if (!cmd) return;

    int sel = ListView_GetNextItem(pane->hwndList, -1, LVNI_SELECTED);
    if (sel < 0 && cmd != ID_MKDIR) return;

    switch (cmd) {
    case ID_OPEN:
        OpenItem(pane);
        break;
    case ID_COPY:
    case ID_MOVE: {
        Pane *dst = (pane == &g_panes[0]) ? &g_panes[1] : &g_panes[0];
        for (int i = ListView_GetNextItem(pane->hwndList, -1, LVNI_SELECTED);
             i >= 0; i = ListView_GetNextItem(pane->hwndList, i, LVNI_SELECTED)) {
            if ((size_t)i >= pane->entries.size()) break;
            FileEntry &e = pane->entries[i];
            std::wstring src = pane->currentPath + L"\\" + e.name;
            std::wstring dstPath = dst->currentPath + L"\\" + e.name;
            if (cmd == ID_COPY)
                CopyFileW(src.c_str(), dstPath.c_str(), FALSE);
            else
                MoveFileW(src.c_str(), dstPath.c_str());
        }
        StartScan(dst);
        StartScan(pane);
        break;
    }
    case ID_MKDIR: {
        wchar_t dirName[256] = L"New Folder";
        if (DialogBoxW(g_hInst, nullptr, hwnd, nullptr) == 0) { // simple fallback
            std::wstring newDir = pane->currentPath + L"\\New Folder";
            CreateDirectoryW(newDir.c_str(), nullptr);
            StartScan(pane);
        }
        break;
    }
    case ID_DELETE: {
        wchar_t msg[512];
        swprintf_s(msg, L"Delete %s?", pane->entries[sel].name.c_str());
        if (MessageBoxW(hwnd, msg, L"Confirm", MB_YESNO | MB_ICONWARNING) == IDYES) {
            std::wstring full = pane->currentPath + L"\\" + pane->entries[sel].name;
            if (pane->entries[sel].IsDir())
                RemoveDirectoryW(full.c_str());
            else
                DeleteFileW(full.c_str());
            StartScan(pane);
        }
        break;
    }
    case ID_RENAME: {
        std::wstring oldName = pane->entries[sel].name;
        std::wstring full = pane->currentPath + L"\\" + oldName;
        // Use shell rename dialog
        SHFILEOPSTRUCTW sh = {0};
        sh.hwnd = hwnd;
        sh.wFunc = FO_RENAME;
        std::wstring from = full + L"\0";
        std::wstring to = full + L"\0"; // shell shows rename UI
        sh.pFrom = from.c_str();
        sh.pTo = to.c_str();
        sh.fFlags = FOF_ALLOWUNDO | FOF_RENAMEONCOLLISION;
        SHFileOperationW(&sh);
        StartScan(pane);
        break;
    }
    case ID_COPY_PATH: {
        std::wstring full = pane->currentPath + L"\\" + pane->entries[sel].name;
        if (OpenClipboard(hwnd)) {
            EmptyClipboard();
            size_t bytes = (full.size() + 1) * sizeof(wchar_t);
            HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (h) { memcpy(GlobalLock(h), full.c_str(), bytes); GlobalUnlock(h); }
            SetClipboardData(CF_UNICODETEXT, h);
            CloseClipboard();
        }
        break;
    }
    case ID_PROPERTIES: {
        std::wstring full = pane->currentPath + L"\\" + pane->entries[sel].name;
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_INVOKEIDLIST;
        sei.lpVerb = L"properties";
        sei.lpFile = full.c_str();
        ShellExecuteExW(&sei);
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Drag-drop (LVN_BEGINDRAG) — set cursor tracking, perform copy/move on drop
// ---------------------------------------------------------------------------
static void OnBeginDrag(Pane *pane, NMITEMACTIVATE *nm) {
    if (nm->iItem < 0 || (size_t)nm->iItem >= pane->entries.size()) return;
    g_dragging = (pane == &g_panes[0]) ? 1 : 2;
    g_dragItems = 1;
    SetCapture(g_hwnd);
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
    // Wait for mouse up to perform the copy/move
    MSG msg;
    while (GetCapture() == g_hwnd && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_LBUTTONUP) {
            POINT pt = { GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam) };
            ClientToScreen(g_hwnd, &pt);
            // Check if dropped on the other pane
            Pane *target = (pane == &g_panes[0]) ? &g_panes[1] : &g_panes[0];
            RECT rc;
            GetClientRect(target->hwndList, &rc);
            MapWindowPoints(target->hwndList, g_hwnd, (POINT*)&rc, 2);
            if (pt.x >= rc.left && pt.x <= rc.right && pt.y >= rc.top && pt.y <= rc.bottom) {
                FileEntry &e = pane->entries[nm->iItem];
                std::wstring src = pane->currentPath + L"\\" + e.name;
                std::wstring dst = target->currentPath + L"\\" + e.name;
                if (MessageBoxW(g_hwnd, L"Copy here?", L"Drag & Drop", MB_YESNO) == IDYES) {
                    CopyFileW(src.c_str(), dst.c_str(), FALSE);
                    StartScan(target);
                }
            }
            g_dragging = 0;
            ReleaseCapture();
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    g_dragging = 0;
    if (GetCapture() == g_hwnd) ReleaseCapture();
}

// ---------------------------------------------------------------------------
// LVN_GETDISPINFO
// ---------------------------------------------------------------------------
static void OnGetDispInfo(NMLVDISPINFOW *di, Pane *pane) {
    int idx = (int)di->item.iItem;
    if (idx < 0 || (size_t)idx >= pane->entries.size()) return;
    FileEntry &e = pane->entries[idx];
    if (di->item.mask & LVIF_TEXT) {
        switch (di->item.iSubItem) {
        case 0:
            wcsncpy_s(di->item.pszText, di->item.cchTextMax, e.name.c_str(), _TRUNCATE);
            break;
        case 1:
            if (e.IsDir()) di->item.pszText[0] = L'\0';
            else { wchar_t b[64]; swprintf_s(b, L"%lld", e.size); wcsncpy_s(di->item.pszText, di->item.cchTextMax, b, _TRUNCATE); }
            break;
        case 2:
            wcsncpy_s(di->item.pszText, di->item.cchTextMax, e.IsDir() ? L"DIR" : L"FILE", _TRUNCATE);
            break;
        case 3: {
            FILETIME lt; SYSTEMTIME st;
            FileTimeToLocalFileTime(&e.lastWrite, &lt);
            FileTimeToSystemTime(&lt, &st);
            wchar_t b[64]; swprintf_s(b, L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
            wcsncpy_s(di->item.pszText, di->item.cchTextMax, b, _TRUNCATE);
            break;
        }
        }
    }
    if (di->item.mask & LVIF_IMAGE) {
        if (e.iconIndex < 0) {
            SHFILEINFOW sfi = {0};
            std::wstring full = pane->currentPath + L"\\" + e.name;
            SHGetFileInfoW(full.c_str(), e.attributes, &sfi, sizeof(sfi),
                           SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);
            e.iconIndex = sfi.iIcon;
        }
        di->item.iImage = e.iconIndex;
    }
}

// ---------------------------------------------------------------------------
// Create pane controls
// ---------------------------------------------------------------------------
static HWND CreatePaneList(HWND parent, int id) {
    HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA |
        LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 100, 100, parent, (HMENU)(LONG_PTR)id, g_hInst, nullptr);
    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW lvc = { LVCF_TEXT | LVCF_WIDTH | LVCF_FMT };
    int cols[] = { 180, 70, 50, 120 };
    const wchar_t *hdr[] = { L"Name", L"Size", L"Type", L"Date" };
    for (int i = 0; i < 4; ++i) { lvc.cx = cols[i]; lvc.pszText = (LPWSTR)hdr[i]; ListView_InsertColumn(hList, i, &lvc); }
    if (g_himlSmall) ListView_SetImageList(hList, g_himlSmall, LVSIL_SMALL);
    return hList;
}

static HWND CreatePathBar(HWND parent, int id) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 100, 22, parent, (HMENU)(LONG_PTR)id, g_hInst, nullptr);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        SHFILEINFOW sfi = {0};
        SHGetFileInfoW(L"", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
        g_himlSmall = (HIMAGELIST)sfi.hIcon;

        for (int i = 0; i < 2; ++i) {
            g_panes[i].InitCS();
            g_panes[i].hwndPath = CreatePathBar(hwnd, (i == 0) ? IDC_PATH_LEFT : IDC_PATH_RIGHT);
            g_panes[i].hwndList = CreatePaneList(hwnd, (i == 0) ? IDC_LIST_LEFT : IDC_LIST_RIGHT);
        }

        g_hStatus = CreateWindowW(STATUSCLASSNAME, L"",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS, g_hInst, nullptr);

        // Init paths from command line args
        wchar_t curDir[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, curDir);
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::wstring leftPath = (argv && argc > 1) ? argv[1] : curDir;
        std::wstring rightPath = (argv && argc > 2) ? argv[2] : L"";
        if (argv) LocalFree(argv);

        NavigateTo(&g_panes[0], leftPath);
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

        g_dividerPos = -1;
        SetFocus(g_panes[0].hwndList);
        return 0;
    }

    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        int pathH = 24, statusH = 22, divW = 4;
        int halfW = (w - divW) / 2;
        if (g_dividerPos < 0) g_dividerPos = halfW;
        int lw = g_dividerPos;
        int rw = w - g_dividerPos - divW;
        int listH = h - pathH - statusH;

        MoveWindow(g_panes[0].hwndPath, 0, 0, lw, pathH, TRUE);
        MoveWindow(g_panes[1].hwndPath, g_dividerPos + divW, 0, rw, pathH, TRUE);
        MoveWindow(g_panes[0].hwndList, 0, pathH, lw, listH, TRUE);
        MoveWindow(g_panes[1].hwndList, g_dividerPos + divW, pathH, rw, listH, TRUE);
        SendMessage(g_hStatus, WM_SIZE, 0, 0);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lp);
        int divX = g_dividerPos;
        if (x >= divX - 2 && x <= divX + 6) {
            g_draggingDivider = true;
            SetCapture(hwnd);
            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        if (g_draggingDivider) {
            int x = GET_X_LPARAM(lp);
            int w; RECT rc; GetClientRect(hwnd, &rc); w = rc.right;
            x = (std::max)(80, (std::min)(w - 80, x));
            g_dividerPos = x;
            SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(w, rc.bottom));
            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
            return 0;
        }
        break;
    }
    case WM_LBUTTONUP:
        if (g_draggingDivider) { g_draggingDivider = false; ReleaseCapture(); return 0; }
        break;

    case WM_SETCURSOR:
        if (!g_draggingDivider) {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            int divX = g_dividerPos;
            if (pt.x >= divX - 2 && pt.x <= divX + 6) {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;

    case WM_NOTIFY: {
        LPNMHDR nm = (LPNMHDR)lp;
        Pane *pane = GetPane(nm->hwndFrom);
        if (!pane) break;

        switch (nm->code) {
        case LVN_GETDISPINFOW:
            OnGetDispInfo((NMLVDISPINFOW*)lp, pane);
            return 0;
        case LVN_COLUMNCLICK: {
            int col = ((NMLISTVIEW*)lp)->iSubItem;
            if (col == (int)pane->sortCol) pane->sortAsc = !pane->sortAsc;
            else { pane->sortCol = (SortColumn)col; pane->sortAsc = true; }
            std::sort(pane->entries.begin(), pane->entries.end(),
                [pane](const FileEntry &a, const FileEntry &b) { return CompareEntries(a, b, pane->sortCol, pane->sortAsc); });
            ListView_SetItemCount(pane->hwndList, (int)pane->entries.size());
            ListView_RedrawItems(pane->hwndList, 0, (int)pane->entries.size());
            return 0;
        }
        case NM_DBLCLK:
            OpenItem(pane);
            return 0;
        case NM_RCLICK: {
            POINT pt; GetCursorPos(&pt);
            ShowContextMenu(hwnd, pane, pt.x, pt.y);
            return 0;
        }
        case LVN_KEYDOWN: {
            int vk = ((NMLVKEYDOWN*)lp)->wVKey;
            if (vk == VK_RETURN) OpenItem(pane);
            else if (vk == VK_BACK) GoToParent(pane);
            else if (vk == VK_F5) StartScan(pane);
            else if (vk == VK_F7) {
                std::wstring nd = pane->currentPath + L"\\New Folder";
                CreateDirectoryW(nd.c_str(), nullptr);
                StartScan(pane);
            }
            else if (vk == VK_DELETE) {
                int s = ListView_GetNextItem(pane->hwndList, -1, LVNI_SELECTED);
                if (s >= 0 && (size_t)s < pane->entries.size()) {
                    std::wstring fp = pane->currentPath + L"\\" + pane->entries[s].name;
                    if (pane->entries[s].IsDir()) RemoveDirectoryW(fp.c_str());
                    else DeleteFileW(fp.c_str());
                    StartScan(pane);
                }
            }
            else if (vk == VK_F2) {
                int s = ListView_GetNextItem(pane->hwndList, -1, LVNI_SELECTED);
                if (s >= 0 && (size_t)s < pane->entries.size()) {
                    std::wstring fp = pane->currentPath + L"\\" + pane->entries[s].name;
                    SHFILEOPSTRUCTW sh = {0}; sh.hwnd = hwnd; sh.wFunc = FO_RENAME;
                    std::wstring f = fp + L"\0";
                    sh.pFrom = f.c_str(); sh.pTo = f.c_str();
                    sh.fFlags = FOF_ALLOWUNDO;
                    SHFileOperationW(&sh);
                    StartScan(pane);
                }
            }
            return 0;
        }
        case LVN_BEGINDRAG:
            OnBeginDrag(pane, (NMITEMACTIVATE*)lp);
            return 0;
        }
        return 0;
    }

    case WM_SCAN_DONE: {
        Pane *pane = (Pane*)lp;
        if (pane->scanThread) { CloseHandle(pane->scanThread); pane->scanThread = nullptr; }
        ListView_SetItemCount(pane->hwndList, (int)pane->entries.size());
        wchar_t buf[128];
        swprintf_s(buf, L"%s: %zu items", pane->currentPath.c_str(), pane->entries.size());
        SetWindowTextW(g_hStatus, buf);
        InvalidateRect(pane->hwndList, nullptr, TRUE);
        return 0;
    }

    case WM_SETFOCUS:
        SetFocus(g_panes[0].hwndList);
        return 0;

    case WM_COMMAND:
        if (HIWORD(wp) == EN_SETFOCUS) {
            HWND hEdit = (HWND)lp;
            if (hEdit == g_panes[0].hwndPath) SetFocus(g_panes[0].hwndList);
            if (hEdit == g_panes[1].hwndPath) SetFocus(g_panes[1].hwndList);
        }
        break;

    case WM_DESTROY:
        for (int i = 0; i < 2; ++i) {
            if (g_panes[i].scanThread) {
                WaitForSingleObject(g_panes[i].scanThread, 2000);
                CloseHandle(g_panes[i].scanThread);
            }
            g_panes[i].DoneCS();
        }
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

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"DiredWindowClass";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"DiredWindowClass", L"Dired",
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

    OleUninitialize();
    return 0;
}
