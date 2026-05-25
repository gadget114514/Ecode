// =============================================================================
// TabIconHelper.inl
// Manages tab icons: colored squares + embedded 24x24 icons + exe/file icons
// Included by main.cpp
// =============================================================================

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

static const COLORREF kTabIconColors[TAB_ICON_COUNT] = {
    RGB(0x99, 0x99, 0x99), RGB(0x4A, 0x90, 0xD9), RGB(0x50, 0xC8, 0x64),
    RGB(0xF5, 0xA6, 0x23), RGB(0x9B, 0x59, 0xB6), RGB(0x1A, 0xBC, 0x9C),
    RGB(0xE7, 0x4C, 0x3C), RGB(0xE9, 0x1E, 0x63), RGB(0xF1, 0xC4, 0x0F),
    RGB(0x00, 0xBC, 0xD4), RGB(0x8B, 0xC3, 0x4A), RGB(0x79, 0x55, 0x48),
};

const wchar_t* g_tabIconNames[TAB_ICON_COUNT] = {
    L"Gray", L"Blue", L"Green", L"Orange", L"Purple", L"Teal",
    L"Red", L"Pink", L"Yellow", L"Cyan", L"Lime", L"Brown",
};

static HBITMAP CreateColorIconBitmap(COLORREF color, int size) {
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, size, size);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBmp);
    HBRUSH hBrush = CreateSolidBrush(color);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcMem, hBrush);
    HPEN hPen = CreatePen(PS_NULL, 0, 0);
    HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);
    RECT rc = {0, 0, size, size};
    FillRect(hdcMem, &rc, hBrush);
    SelectObject(hdcMem, hOldBrush);
    SelectObject(hdcMem, hOldPen);
    SelectObject(hdcMem, hOld);
    DeleteObject(hBrush);
    DeleteObject(hPen);
    ReleaseDC(nullptr, hdcScreen);
    DeleteDC(hdcMem);
    return hBmp;
}

// Pre-populate image list with colored squares and embedded named icons.
// Returns the total number of icons added.
#include "icons.h"

static int InitTabIcons(HIMAGELIST himl) {
    // Colored squares first (indices 0 .. TAB_ICON_COUNT-1)
    for (int i = 0; i < TAB_ICON_COUNT; ++i) {
        HBITMAP hBmp = CreateColorIconBitmap(kTabIconColors[i], 16);
        ImageList_Add(himl, hBmp, nullptr);
        DeleteObject(hBmp);
    }
    // Embedded 24x24 icons (resized to 16x16 by ImageList)
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    for (int i = 0; i < IDI_ICON_COUNT; ++i) {
        HICON hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_ICON_BASE + i),
                                        IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        if (hIcon) {
            ImageList_AddIcon(himl, hIcon);
            DestroyIcon(hIcon);
        }
    }
    return TAB_ICON_COUNT + IDI_ICON_COUNT; // total icons pre-loaded
}

// Extract file type icon using SHGetFileInfo, add to image list
static int AddFileTypeIcon(HIMAGELIST himl, const std::wstring &filePath) {
    SHFILEINFOW sfi = {0};
    UINT flags = SHGFI_ICON | SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    if (filePath.empty())
        flags |= SHGFI_USEFILEATTRIBUTES;
    DWORD_PTR ret = SHGetFileInfoW(
        filePath.empty() ? L".txt" : filePath.c_str(),
        FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags);
    if (!ret || !sfi.hIcon)
        return -1;
    int idx = ImageList_AddIcon(himl, sfi.hIcon);
    DestroyIcon(sfi.hIcon);
    return idx;
}

// Extract icon from an executable, add to image list
static int AddExeIcon(HIMAGELIST himl, const std::wstring &exePath) {
    HICON hIcon = ExtractIconW(nullptr, exePath.c_str(), 0);
    if (!hIcon || hIcon == (HICON)(INT_PTR)1)
        return -1;
    int idx = ImageList_AddIcon(himl, hIcon);
    DestroyIcon(hIcon);
    return idx;
}
