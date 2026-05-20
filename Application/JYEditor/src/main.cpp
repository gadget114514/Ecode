#include <windows.h>
#include <shellapi.h>
#include "EditorWindow.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Check for --embedded flag (host requests hidden window for embedding)
    {
        int argc;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv && argc >= 2 && wcscmp(argv[1], L"--embedded") == 0)
            nCmdShow = SW_HIDE;
        if (argv) LocalFree(argv);
    }

    // Initialize COM
    if (FAILED(CoInitialize(NULL))) {
        return 0;
    }

    EditorWindow window;
    if (!window.Create(L"JYEditor", WS_OVERLAPPEDWINDOW)) {
        CoUninitialize();
        return 0;
    }

    ShowWindow(window.Window(), nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return 0;
}
