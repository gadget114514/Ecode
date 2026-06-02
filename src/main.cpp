#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <objbase.h>
#include "App.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Init common controls
    INITCOMMONCONTROLSEX icex = {sizeof(icex), ICC_BAR_CLASSES | ICC_TAB_CLASSES};
    InitCommonControlsEx(&icex);
    
    // Parse command line for --embedded
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool embedded = false;
    if (argv) {
        for (int i = 1; i < argc; i++) {
            if (wcscmp(argv[i], L"--embedded") == 0) {
                embedded = true;
                break;
            }
        }
        LocalFree(argv);
    }
    
    // Initialize COM for WebView2
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;
    
    App app;
    if (!app.Init(hInstance, embedded)) { CoUninitialize(); return 1; }
    
    int ret = app.Run();
    CoUninitialize();
    return ret;
}
