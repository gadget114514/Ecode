#include <windows.h>
#include "MainWindow.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize COM
    if (FAILED(CoInitialize(NULL))) {
        return 1;
    }

    MainWindow window;
    if (!window.Create(hInstance, nCmdShow)) {
        return 1;
    }

    // For debugging/demo: try to open the complex test file if it exists, or create a dummy one
    // In real app, we would parse cmd line or show OpenDialog
    window.OpenFile(L"test_complex.csv");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return (int) msg.wParam;
}
