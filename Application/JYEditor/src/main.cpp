#include <windows.h>
#include <shellapi.h>
#include <string>
#include "EditorWindow.h"

std::string g_langOverride;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Check for --embedded and --lang flags
    {
        int argc;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i < argc; ++i) {
                if (wcscmp(argv[i], L"--embedded") == 0)
                    nCmdShow = SW_HIDE;
                else if (wcscmp(argv[i], L"--lang") == 0 && i + 1 < argc) {
                    wchar_t wc = argv[i + 1][0];
                    g_langOverride = (wc == 'j' || wc == 'J') ? "jp" : "en";
                }
            }
        }
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
