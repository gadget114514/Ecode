#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <consoleapi3.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    HANDLE hInR, hInW, hOutR, hOutW;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HPCON hpc;
    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    char buf[4096];
    DWORD n;

    /* Check ConPTY availability */
    HMODULE hK32 = GetModuleHandleA("kernel32.dll");
    if (!hK32 || !GetProcAddress(hK32, "CreatePseudoConsole")) {
        fprintf(stderr, "ConPTY not available\n");
        return 1;
    }

    /* Create pipes */
    if (!CreatePipe(&hInR, &hInW, &sa, 0)) { fprintf(stderr, "pipe1 fail\n"); return 1; }
    if (!CreatePipe(&hOutR, &hOutW, &sa, 0)) { fprintf(stderr, "pipe2 fail\n"); return 1; }

    /* Mark near ends as non-inheritable */
    SetHandleInformation(hInW, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0);

    /* Create pseudo console */
    COORD sz = { 80, 25 };
    if (FAILED(CreatePseudoConsole(sz, hInR, hOutW, 0, &hpc))) {
        fprintf(stderr, "CreatePseudoConsole fail\n"); return 1;
    }

    /* Build startup info */
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);

    SIZE_T cb;
    InitializeProcThreadAttributeList(NULL, 1, 0, &cb);
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(cb);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &cb);
    UpdateProcThreadAttribute(si.lpAttributeList, 0,
                              PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                              hpc, sizeof(HPCON), NULL, NULL);

    /* Launch cmd.exe */
    BOOL ok = CreateProcessW(
        NULL, L"cmd.exe", NULL, NULL, TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        NULL, NULL, &si.StartupInfo, &pi);

    free(si.lpAttributeList);
    CloseHandle(hInR);
    CloseHandle(hOutW);

    if (!ok) { fprintf(stderr, "CreateProcess fail\n"); ClosePseudoConsole(hpc); return 1; }

    printf("Process started: PID=%lu\n", pi.dwProcessId);

    CloseHandle(pi.hThread);

    /* Read output */
    printf("Reading output...\n");
    for (int i = 0; i < 20; i++) {
        if (ReadFile(hOutR, buf, sizeof(buf) - 1, &n, NULL) && n > 0) {
            buf[n] = '\0';
            printf("--- Read %lu bytes ---\n", n);
            for (DWORD j = 0; j < n; j++) {
                if (buf[j] >= 32 && buf[j] < 127) putchar(buf[j]);
                else if (buf[j] == '\r') printf("\\r");
                else if (buf[j] == '\n') printf("\\n\n");
                else if (buf[j] == '\x1b') printf("^[");
                else printf("\\x%02X", (unsigned char)buf[j]);
            }
            printf("\n");
        } else {
            printf("ReadFile returned FALSE or EOF\n");
            break;
        }
    }

    /* Now write a command */
    const char *cmd = "echo hello world\r";
    DWORD written;
    WriteFile(hInW, cmd, (DWORD)strlen(cmd), &written, NULL);
    printf("\nSent: '%s'\n", cmd);

    /* Read response */
    Sleep(500);
    for (int i = 0; i < 10; i++) {
        if (ReadFile(hOutR, buf, sizeof(buf) - 1, &n, NULL) && n > 0) {
            buf[n] = '\0';
            printf("--- Response %lu bytes ---\n", n);
            for (DWORD j = 0; j < n; j++) {
                if (buf[j] >= 32 && buf[j] < 127) putchar(buf[j]);
                else if (buf[j] == '\r') printf("\\r");
                else if (buf[j] == '\n') printf("\\n\n");
                else if (buf[j] == '\x1b') printf("^[");
                else printf("\\x%02X", (unsigned char)buf[j]);
            }
            printf("\n");
        } else {
            printf("No more data\n");
            break;
        }
    }

    /* Cleanup */
    ClosePseudoConsole(hpc);
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(hInW);
    CloseHandle(hOutR);

    printf("Done\n");
    return 0;
}
