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

    // Register classes
    TerminalView::RegisterWindowClass(hInstance);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"EcodeTerminalWindow";
    if (!RegisterClassExW(&wc)) return 1;

    // Parse shell argument (join remaining args, re-quoting elements with spaces)
    if (argv && argc >= argIdx + 1) {
        auto quoteIfNeeded = [](const std::wstring& s) -> std::wstring {
            if (s.find(L' ') != std::wstring::npos && s.find(L'"') == std::wstring::npos)
                return L"\"" + s + L"\"";
            return s;
        };
        g_shell = quoteIfNeeded(argv[argIdx]);
        for (int i = argIdx + 1; i < argc; ++i) {
            g_shell += L" " + quoteIfNeeded(argv[i]);
        }
    }

    // Create window
    HWND hwnd = CreateWindowExW(
        embedded ? WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW : 0,
        L"EcodeTerminalWindow",
        L"Terminal",
        embedded ? WS_POPUP : WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        900, 640,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 1;

    if (embedded) {
        // Parent (ecode) will show and reposition after embedding via WM_EMBED_APP
    } else {
        ShowWindow(hwnd, nCmdShow);
    }

    if (g_view)
        g_view->StartSession(g_shell);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

