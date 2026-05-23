// =============================================================================
// TabIconHelper.inl
// Colored square icons for tab control image list
// Included by main.cpp
// =============================================================================

static const COLORREF kTabIconColors[TAB_ICON_COUNT] = {
    RGB(0x99, 0x99, 0x99), // Gray
    RGB(0x4A, 0x90, 0xD9), // Blue
    RGB(0x50, 0xC8, 0x64), // Green
    RGB(0xF5, 0xA6, 0x23), // Orange
    RGB(0x9B, 0x59, 0xB6), // Purple
    RGB(0x1A, 0xBC, 0x9C), // Teal
    RGB(0xE7, 0x4C, 0x3C), // Red
    RGB(0xE9, 0x1E, 0x63), // Pink
    RGB(0xF1, 0xC4, 0x0F), // Yellow
    RGB(0x00, 0xBC, 0xD4), // Cyan
    RGB(0x8B, 0xC3, 0x4A), // Lime
    RGB(0x79, 0x55, 0x48), // Brown
};

const wchar_t* g_tabIconNames[TAB_ICON_COUNT] = {
    L"Gray",
    L"Blue",
    L"Green",
    L"Orange",
    L"Purple",
    L"Teal",
    L"Red",
    L"Pink",
    L"Yellow",
    L"Cyan",
    L"Lime",
    L"Brown",
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

static void InitTabIcons(HIMAGELIST himl) {
    for (int i = 0; i < TAB_ICON_COUNT; ++i) {
        HBITMAP hBmp = CreateColorIconBitmap(kTabIconColors[i], 16);
        ImageList_Add(himl, hBmp, nullptr);
        DeleteObject(hBmp);
    }
}
