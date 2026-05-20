#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include "../include/TerminalView.h"
#include <string>

static TerminalView* g_view  = nullptr;
static std::wstring  g_shell = L"powershell.exe";

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_view = new TerminalView();
        if (!g_view->Create(hwnd)) {
            delete g_view;
            g_view = nullptr;
            return -1;
        }
        return 0;

    case WM_SIZE:
        if (g_view)
            g_view->MoveAndResize(0, 0, LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_SETFOCUS:
        if (g_view && g_view->Hwnd())
            SetFocus(g_view->Hwnd());
        return 0;

    case WM_DESTROY:
        if (g_view) { delete g_view; g_view = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // Check for --embedded flag (host requests hidden window for embedding)
    bool embedded = false;
    int argIdx = 1;
    if (argv && argc >= 2 && wcscmp(argv[1], L"--embedded") == 0) {
        embedded = true;
        argIdx = 2;
    }

    if (argv && argc > argIdx) {
        g_shell = argv[argIdx];
        if (g_shell.find(L' ') != std::wstring::npos)
            g_shell = L"\"" + g_shell + L"\"";
        for (int i = argIdx + 1; i < argc; ++i) {
            g_shell += L" ";
            std::wstring arg = argv[i];
            if (arg.find(L' ') != std::wstring::npos)
                arg = L"\"" + arg + L"\"";
            g_shell += arg;
        }
    }
    if (argv) LocalFree(argv);

    // Derive window title: "powershell.exe" -> "powershell"
    std::wstring title = g_shell;
    size_t slash = title.find_last_of(L"\\/");
    if (slash != std::wstring::npos) title = title.substr(slash + 1);
    size_t dot = title.rfind(L'.');
    if (dot != std::wstring::npos) title = title.substr(0, dot);

    TerminalView::RegisterWindowClass(hInstance);

    const wchar_t CLASS_NAME[] = L"EcodeTerminalWindow";
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (!RegisterClassExW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 500,
        NULL, NULL, hInstance, NULL);
    if (!hwnd) return 1;

    // When embedded, stay hidden until host sends WM_EMBED_APP
    ShowWindow(hwnd, embedded ? SW_HIDE : nCmdShow);

    if (g_view && !g_view->StartSession(g_shell)) {
        std::wstring msg = L"Failed to start shell: " + g_shell + L"\n\n" + g_view->LastError();
        MessageBoxW(hwnd, msg.c_str(), L"Terminal", MB_OK | MB_ICONERROR);
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
