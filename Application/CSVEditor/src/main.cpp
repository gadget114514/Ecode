#include <windows.h>
#include <shellapi.h>
#include "MainWindow.h"
#include "Localization.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Check for --embedded and --lang flags
    {
        int argc;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i < argc; ++i) {
                if (wcscmp(argv[i], L"--embedded") == 0)
                    nCmdShow = SW_HIDE;
                else if (wcscmp(argv[i], L"--lang") == 0 && i + 1 < argc) {
                    if (wcscmp(argv[i + 1], L"jp") == 0)
                        CsvLocalization::SetLanguage(CsvLanguage::Japanese);
                    else
                        CsvLocalization::SetLanguage(CsvLanguage::English);
                }
            }
        }
        if (argv) LocalFree(argv);
    }

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
