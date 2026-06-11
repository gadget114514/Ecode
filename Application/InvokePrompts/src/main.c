#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    WCHAR exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    WCHAR *last = wcsrchr(exePath, L'\\');
    if (last) *(last + 1) = L'\0';

    WCHAR target[MAX_PATH];
    wsprintfW(target, L"%sPrompts\\Prompts.exe", exePath);

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask      = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb     = L"open";
    sei.lpFile     = target;
    sei.lpDirectory = exePath;
    sei.nShow      = SW_SHOWNORMAL;

    ShellExecuteExW(&sei);
    return 0;
}
