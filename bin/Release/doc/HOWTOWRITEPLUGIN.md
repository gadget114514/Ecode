# How to Write an Ecode Plugin

A plugin is a **standalone Windows executable (`.exe`)** that ecode launches and embeds as a tab in the main window. Plugins are discovered automatically from the `plugins\` directory.

## Quick Start (Minimal Plugin)

```cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

const wchar_t CLASS_NAME[] = L"MyPluginClass";

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        TextOutW(ps.hdc, 10, 10, L"Hello from plugin!", 18);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"My Plugin",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        640, 480, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
```

## How Ecode Discovers Plugins

Ecode scans **two locations** (in order) on startup:

| Order | Directory | Notes |
|---|---|---|
| 1 | `{ecode.exe dir}\plugins\` | Default plugin directory |
| 2 | User-configured directory | Set in Settings → Plugins Directory |

**Naming collision**: if the same `.exe` filename exists in both dirs, the user directory takes precedence.

### Built-in Fallback

`Dired.exe`, `CSVEditor.exe`, `FastFileSearch.exe`, and `JYEditor.exe` located alongside `ecode.exe` are also discovered (only these four named files, not arbitrary `.exe`).

## Plugin Lifecycle

```
1. ScanPlugins()       → ecode finds .exe files, populates Plugins menu
2. User clicks menu    → ecode calls LaunchPlugin(index)
3. CreateProcess()     → ecode launches the .exe
4. Window enumeration  → ecode polls every 100ms (up to 5s) for a visible top-level window
5. WM_EMBED_APP        → ecode reparents the plugin window into an app tab
6. User closes tab     → ecode calls TerminateProcess() and cleans up
```

## Requirements

### 1. Subsystem: `WIN32` (GUI)

Link as a GUI application, not a console app. In CMake:

```cmake
add_executable(myplugin WIN32 src/main.cpp)
```

### 2. Create a Top-Level Window

The window must be **visible** and **not owned by another window** when created. Ecode finds it via `EnumWindows`:

```cpp
CreateWindowExW(0, CLASS_NAME, L"My Plugin",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
    nullptr,  // ← no parent
    nullptr, hInst, nullptr);
ShowWindow(hwnd, nCmdShow);
```

### 3. Message Loop

A standard `GetMessage` loop is required. Ecode's `WM_EMBED_APP` handler remaps the window style to `WS_CHILD` and sets the parent, so your window procedure should handle `WM_SIZE` to lay out content (the size will be set by ecode).

```cpp
case WM_SIZE: {
    int w = LOWORD(lp), h = HIWORD(lp);
    // Resize child controls to fit new dimensions
    MoveWindow(hChildControl, 0, 0, w, h, TRUE);
    return 0;
}
```

## Optional: Receiving Command-Line Arguments

Ecode's current `LaunchPlugin` does **not** pass command-line arguments, but you can parse `GetCommandLineW()` for future extensibility:

```cpp
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
    // pCmdLine contains arguments (currently empty when launched from menu)
    // Parse for future configuration
}
```

## Optional: Communication with Ecode

Plugins can communicate with ecode via:

- **Window messages**: `SendMessage(GetParent(hwnd), ...)` to send custom `WM_USER` messages to ecode
- **Clipboard**: Standard clipboard operations for data exchange
- **Files**: Read/write files in a shared working directory

Currently ecode does not define a formal IPC protocol, but `WM_USER` messages can be used for app-specific communication.

## Build Configuration (CMake)

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(myplugin WIN32 src/main.cpp)

target_link_libraries(myplugin
    user32
    gdi32
    shell32
    comctl32
)

if(MSVC)
    target_compile_options(myplugin PRIVATE /utf-8)
    target_compile_definitions(myplugin PRIVATE
        UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
    )
endif()
```

The built `myplugin.exe` should be copied to `{ecode_dir}\plugins\` (or the user-configured plugin directory).

## Example: List View Plugin

A plugin that displays a simple list:

```cpp
case WM_CREATE: {
    HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT,
        0, 0, 100, 100, hwnd, nullptr, hInst, nullptr);
    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT);

    LVCOLUMNW lvc = { LVCF_TEXT | LVCF_WIDTH };
    lvc.pszText = L"Item"; lvc.cx = 200;
    ListView_InsertColumn(hList, 0, &lvc);
    lvc.pszText = L"Value"; lvc.cx = 200;
    ListView_InsertColumn(hList, 1, &lvc);

    LVITEMW li = { LVIF_TEXT };
    li.pszText = L"Hello"; li.iItem = 0;
    ListView_InsertItem(hList, &li);
    return 0;
}
```

## Troubleshooting

- **Plugin doesn't appear in menu**: Check that the `.exe` is in `{ecode}\plugins\` and restart ecode (or use Plugins → Rescan Plugins)
- **Window not embedded**: Make sure the window is created **visible**, **top-level** (no parent), and **not owned** (`GetWindow(hwnd, GW_OWNER)` must return `NULL`)
- **Plugin crashes on launch**: Test the `.exe` standalone first
- **Tab shows "Starting..." forever**: Ecode couldn't find the plugin's window within 5 seconds. Check window creation and visibility
- **Console window appears**: Link as `WIN32` subsystem, not `CONSOLE`
