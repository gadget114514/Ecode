#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <consoleapi3.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    HANDLE hInR, hInW, hOutR, hOutW;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };

    if (!CreatePipe(&hInR, &hInW, &sa, 0)) { printf("pipe1 fail\n"); return 1; }
    if (!CreatePipe(&hOutR, &hOutW, &sa, 0)) { printf("pipe2 fail\n"); return 1; }

    SetHandleInformation(hInW, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0);

    COORD sz = {80, 25};
    HPCON hpc;
    if (FAILED(CreatePseudoConsole(sz, hInR, hOutW, 0, &hpc))) { printf("CPC fail\n"); return 1; }

    STARTUPINFOEXW si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput  = hInR;
    si.StartupInfo.hStdOutput = hOutW;
    si.StartupInfo.hStdError  = hOutW;

    SIZE_T cb;
    InitializeProcThreadAttributeList(NULL, 1, 0, &cb);
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(cb);
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &cb)) { printf("Init fail\n"); return 1; }
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        hpc, sizeof(HPCON), NULL, NULL)) { printf("Update fail\n"); return 1; }

    PROCESS_INFORMATION pi;
    wchar_t cmdline[] = L"cmd.exe";

    BOOL ok = CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
        EXTENDED_STARTUPINFO_PRESENT,
        NULL, NULL, &si.StartupInfo, &pi);
    printf("CreateProcessW ok=%d", ok);
    if (!ok) { printf(" err=%lu\n", GetLastError()); return 1; }
    printf(" pid=%lu\n", pi.dwProcessId);

    free(si.lpAttributeList);
    CloseHandle(hInR);
    CloseHandle(hOutW);
    CloseHandle(pi.hThread);

    char buf[32768]; DWORD n;
    printf("Reading output:\n");
    for (int i = 0; i < 20; i++) {
        BOOL r = ReadFile(hOutR, buf, sizeof(buf)-1, &n, NULL);
        if (r && n > 0) {
            buf[n] = 0;
            printf("[%lu] ", n);
            for (DWORD j = 0; j < n; j++) {
                if (buf[j] >= 32 && buf[j] < 127) putchar(buf[j]);
                else if (buf[j] == '\r') printf("\\r");
                else if (buf[j] == '\n') printf("\\n\n");
                else if (buf[j] == 27) printf("^[");
                else printf("\\x%02X", (unsigned char)buf[j]);
            }
            printf("\n");
        } else {
            printf("ReadFile ret=%d n=%lu err=%lu\n", r, n, GetLastError());
            break;
        }
    }

    const char *cmd = "echo hello\r";
    DWORD w;
    if (!WriteFile(hInW, cmd, (DWORD)strlen(cmd), &w, NULL))
        printf("WriteFile fail %lu\n", GetLastError());
    else
        printf("Wrote %lu bytes\n", w);

    Sleep(2000);

    printf("Reading response:\n");
    for (int i = 0; i < 10; i++) {
        BOOL r = ReadFile(hOutR, buf, sizeof(buf)-1, &n, NULL);
        if (r && n > 0) {
            buf[n] = 0; printf("[resp %lu] %s\n", n, buf);
        } else {
            printf("ReadFile ret=%d err=%lu\n", r, GetLastError());
            break;
        }
    }

    ClosePseudoConsole(hpc);
    if (WaitForSingleObject(pi.hProcess, 3000) == WAIT_TIMEOUT)
        TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(hInW);
    CloseHandle(hOutR);
    printf("Done\n");
    return 0;
}
