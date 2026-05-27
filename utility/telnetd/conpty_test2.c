#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <consoleapi3.h>
#include <stdio.h>
#include <stdlib.h>
int main() {
    printf("Step 1\n"); fflush(stdout);
    HANDLE hInR, hInW, hOutR, hOutW;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hInR, &hInW, &sa, 0)) { printf("pipe1 fail\n"); return 1; }
    if (!CreatePipe(&hOutR, &hOutW, &sa, 0)) { printf("pipe2 fail\n"); return 1; }
    SetHandleInformation(hInW, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0);
    COORD sz = {80, 25};
    HPCON hpc;
    HRESULT hr = CreatePseudoConsole(sz, hInR, hOutW, 0, &hpc);
    printf("CreatePseudoConsole hr=0x%lx\n", hr); fflush(stdout);
    if (FAILED(hr)) return 1;
    STARTUPINFOEXW si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    SIZE_T cb;
    InitializeProcThreadAttributeList(NULL, 1, 0, &cb);
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(cb);
    if (!si.lpAttributeList) { printf("malloc fail\n"); return 1; }
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &cb)) {
        printf("InitAttrList fail %lu\n", GetLastError()); return 1;
    }
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        hpc, sizeof(HPCON), NULL, NULL)) {
        printf("UpdateAttr fail %lu\n", GetLastError()); return 1;
    }
    PROCESS_INFORMATION pi;
    /* CreateProcessW MODIFIES the command line - must be in writable memory */
    wchar_t cmdline[] = L"cmd.exe";
    BOOL ok = CreateProcessW(NULL, cmdline, NULL, NULL, FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        NULL, NULL, &si.StartupInfo, &pi);
    printf("CreateProcessW ok=%d", ok); fflush(stdout);
    if (!ok) { printf(" err=%lu\n", GetLastError()); return 1; }
    printf(" pid=%lu\n", pi.dwProcessId);
    free(si.lpAttributeList);
    CloseHandle(hInR);
    CloseHandle(hOutW);
    CloseHandle(pi.hThread);
    char buf[4096]; DWORD n;
    printf("Reading output:\n");
    for (int i = 0; i < 10 && ReadFile(hOutR, buf, sizeof(buf)-1, &n, NULL) && n > 0; i++) {
        buf[n] = 0; printf("[%lu] %s\n", n, buf); fflush(stdout);
    }
    const char *cmd = "echo hello\r";
    DWORD w;
    WriteFile(hInW, cmd, (DWORD)strlen(cmd), &w, NULL);
    printf("Wrote %lu bytes\n", w);
    Sleep(500);
    printf("Reading response:\n");
    for (int i = 0; i < 10 && ReadFile(hOutR, buf, sizeof(buf)-1, &n, NULL) && n > 0; i++) {
        buf[n] = 0; printf("[%lu] %s\n", n, buf); fflush(stdout);
    }
    ClosePseudoConsole(hpc);
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(hInW);
    CloseHandle(hOutR);
    printf("Done\n");
    return 0;
}
