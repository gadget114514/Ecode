#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

/* ================================================================
 * ConPTY function pointers (loaded from conpty.dll/kernel32.dll)
 * ================================================================ */
#ifndef HPCON_DEFINED
#define HPCON_DEFINED
typedef VOID *HPCON;
#endif

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE \
    ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)
#endif

typedef HRESULT (WINAPI *FN_CreatePseudoConsole)(COORD, HANDLE, HANDLE, DWORD, HPCON *);
typedef HRESULT (WINAPI *FN_ResizePseudoConsole)(HPCON, COORD);
typedef VOID   (WINAPI *FN_ClosePseudoConsole)(HPCON);

static FN_CreatePseudoConsole  ConPtyCreate  = NULL;
static FN_ResizePseudoConsole  ConPtyResize  = NULL;
static FN_ClosePseudoConsole   ConPtyClose   = NULL;

static HMODULE g_hConPty = NULL;

/* ================================================================
 * Telnet protocol constants
 * ================================================================ */
#define TEL_IAC     0xFF
#define TEL_DONT    0xFE
#define TEL_DO      0xFD
#define TEL_WONT    0xFC
#define TEL_WILL    0xFB
#define TEL_SB      0xFA
#define TEL_SE      0xF0

#define TELOPT_ECHO           1
#define TELOPT_SUPPRESS_GA    3
#define TELOPT_STATUS         5
#define TELOPT_TIMING_MARK    6
#define TELOPT_TERMINAL_TYPE 24
#define TELOPT_NAWS          31
#define TELOPT_LINEMODE      34
#define TELOPT_BINARY         0

/* ================================================================
 * Configuration
 * ================================================================ */
static int       g_port         = 23;
static char      g_addr[64]     = "0.0.0.0";
static char      g_password[256] = "";
static int       g_auth         = 0;
static char      g_shell[64]    = "cmd";
static char      g_term[64]     = "xterm-256color";
static int       g_verbose      = 0;
static volatile int g_running   = 1;

#define VPRINT(fmt, ...) do { if (g_verbose) fprintf(stderr, "[V] " fmt "\r\n", ##__VA_ARGS__); } while(0)

/* ================================================================
 * Per-client context
 * ================================================================ */
typedef struct {
    SOCKET  sock;
    struct  sockaddr_in addr;
    HPCON   hpc;
    HANDLE  hPipeInWrite;
    HANDLE  hPipeOutRead;
    HANDLE  hShellProc;
    HANDLE  hShellThread;
    HANDLE  hOutThread;
    HANDLE  hShutdownEvent;
    volatile LONG running;
} CLIENT_CTX;

/* ================================================================
 * Helpers
 * ================================================================ */
static int send_all(SOCKET s, const char *buf, int len) {
    while (len > 0) {
        int n = send(s, buf, len, 0);
        if (n == SOCKET_ERROR) return 0;
        buf += n;  len -= n;
    }
    return 1;
}

static void telnet_iac(SOCKET s, int cmd, int opt) {
    unsigned char p[3] = { TEL_IAC, (unsigned char)cmd, (unsigned char)opt };
    send(s, (const char *)p, 3, 0);
}

/* ================================================================
 * Build Unicode environment block with terminal identity vars
 * ================================================================ */
static wchar_t* build_env_block(void) {
    wchar_t* env = GetEnvironmentStringsW();
    if (!env) return NULL;

    wchar_t termVar[128];
    swprintf_s(termVar, 128, L"TERM=%hs", g_term);

    const wchar_t* overrides[] = {
        termVar,
        L"COLORTERM=truecolor",
        L"CLICOLOR_FORCE=1",
        L"FORCE_COLOR=3",
        L"LANG=en_US.UTF-8",
        L"LC_ALL=en_US.UTF-8",
        NULL
    };

    size_t total = 0;
    const wchar_t* p = env;
    while (*p) {
        size_t len = wcslen(p);
        int skip = 0;
        for (int i = 0; overrides[i]; i++) {
            const wchar_t* eq = wcschr(overrides[i], L'=');
            if (!eq) continue;
            ptrdiff_t klen = eq - overrides[i];
            if ((int)len > klen && wcsncmp(p, overrides[i], klen) == 0 && p[klen] == L'=') {
                skip = 1; break;
            }
        }
        if (!skip) total += len + 1;
        p += len + 1;
    }
    for (int i = 0; overrides[i]; i++)
        total += wcslen(overrides[i]) + 1;
    total += 1;

    wchar_t* block = (wchar_t*)malloc(total * sizeof(wchar_t));
    if (!block) { FreeEnvironmentStringsW(env); return NULL; }

    wchar_t* wp = block;
    p = env;
    while (*p) {
        size_t len = wcslen(p);
        int skip = 0;
        for (int i = 0; overrides[i]; i++) {
            const wchar_t* eq = wcschr(overrides[i], L'=');
            if (!eq) continue;
            ptrdiff_t klen = eq - overrides[i];
            if ((int)len > klen && wcsncmp(p, overrides[i], klen) == 0 && p[klen] == L'=') {
                skip = 1; break;
            }
        }
        if (!skip) {
            wcscpy_s(wp, total - (wp - block), p);
            wp += len + 1;
        }
        p += len + 1;
    }
    for (int i = 0; overrides[i]; i++) {
        size_t len = wcslen(overrides[i]);
        wcscpy_s(wp, total - (wp - block), overrides[i]);
        wp += len + 1;
    }
    *wp = L'\0';

    FreeEnvironmentStringsW(env);
    return block;
}

/* ================================================================
 * Resolve shell path
 * ================================================================ */
static const char *resolve_shell(const char *shell) {
    static char path[MAX_PATH + 1];

    if (_stricmp(shell, "cmd") == 0) {
        GetSystemDirectoryA(path, MAX_PATH);
        strcat_s(path, MAX_PATH, "\\cmd.exe");
        return path;
    }
    if (_stricmp(shell, "powershell") == 0) {
        GetSystemDirectoryA(path, MAX_PATH);
        strcat_s(path, MAX_PATH, "\\WindowsPowerShell\\v1.0\\powershell.exe");
        return path;
    }
    if (_stricmp(shell, "bash") == 0) {
        char buf[MAX_PATH + 1];
        if (SearchPathA(NULL, "bash", ".exe", MAX_PATH, buf, NULL) > 0) {
            strcpy_s(path, MAX_PATH, buf);
            return path;
        }
        static const char *fallback_paths[] = {
        	"C:\\Program Files\\Git\\usr\\bin\\bash.exe",
            "C:\\Program Files\\Git\\bin\\bash.exe",
            "C:\\Program Files (x86)\\Git\\bin\\bash.exe",
            NULL
        };
        for (int i = 0; fallback_paths[i]; i++) {
            DWORD attr = GetFileAttributesA(fallback_paths[i]);
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                strcpy_s(path, MAX_PATH, fallback_paths[i]);
                return path;
            }
        }
        char *local = getenv("LOCALAPPDATA");
        if (local) {
            char lp[MAX_PATH + 1];
            sprintf_s(lp, MAX_PATH, "%s\\Programs\\Git\\bin\\bash.exe", local);
            DWORD attr = GetFileAttributesA(lp);
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                strcpy_s(path, MAX_PATH, lp);
                return path;
            }
        }
        return NULL;
    }
    return shell;
}

/* ================================================================
 * IAC processing
 * ================================================================ */
static int process_iac(const unsigned char *in, int inlen,
                       unsigned char *out, int *outlen,
                       SOCKET s, CLIENT_CTX *ctx)
{
    int i = 0, o = 0;
    unsigned char sb_opt = 0;

    while (i < inlen) {
        if (in[i] != TEL_IAC) {
            out[o++] = in[i++];
            continue;
        }
        if (i + 1 >= inlen) break;
        unsigned char cmd = in[i + 1];

        if (cmd == TEL_IAC) {
            out[o++] = TEL_IAC;
            i += 2;
            continue;
        }
        if (cmd == TEL_WILL || cmd == TEL_WONT ||
            cmd == TEL_DO   || cmd == TEL_DONT) {
            if (i + 2 >= inlen) break;
            unsigned char opt = in[i + 2];
            switch (cmd) {
            case TEL_WILL:
                telnet_iac(s, (opt == TELOPT_ECHO || opt == TELOPT_SUPPRESS_GA ||
                               opt == TELOPT_STATUS || opt == TELOPT_TIMING_MARK ||
                               opt == TELOPT_BINARY)
                            ? TEL_DO : TEL_DONT, opt);
                break;
            case TEL_DO:
                telnet_iac(s, (opt == TELOPT_ECHO || opt == TELOPT_SUPPRESS_GA ||
                               opt == TELOPT_NAWS || opt == TELOPT_TERMINAL_TYPE ||
                               opt == TELOPT_BINARY)
                            ? TEL_WILL : TEL_WONT, opt);
                break;
            default: break;
            }
            i += 3;
        } else if (cmd == TEL_SB) {
            i += 2;
            sb_opt = (i < inlen) ? in[i] : 0;
            if (sb_opt == TELOPT_NAWS) {
                i++; /* skip option byte */
                unsigned char sb_buf[4];
                int sb_pos = 0;
                while (i < inlen && sb_pos < 4) {
                    if (in[i] == TEL_IAC && i + 1 < inlen && in[i + 1] == TEL_SE)
                        break;
                    if (in[i] == TEL_IAC && i + 1 < inlen && in[i + 1] == TEL_IAC) {
                        sb_buf[sb_pos++] = TEL_IAC;
                        i += 2;
                        continue;
                    }
                    sb_buf[sb_pos++] = in[i++];
                }
                if (sb_pos == 4 && ctx && ctx->hpc && ConPtyResize) {
                    int w = ((int)sb_buf[0] << 8) | sb_buf[1];
                    int h = ((int)sb_buf[2] << 8) | sb_buf[3];
                    COORD sz = { (SHORT)w, (SHORT)h };
                    ConPtyResize(ctx->hpc, sz);
                    VPRINT("NAWS resize to %dx%d", w, h);
                }
                /* skip to IAC SE */
                while (i < inlen) {
                    if (in[i] == TEL_IAC && i + 1 < inlen && in[i + 1] == TEL_SE) { i += 2; break; }
                    i++;
                }
            } else {
                /* unknown SB — skip to IAC SE */
                while (i < inlen) {
                    if (in[i] == TEL_IAC) {
                        if (i + 1 < inlen && in[i + 1] == TEL_SE) { i += 2; break; }
                        if (i + 1 < inlen && in[i + 1] == TEL_IAC) { i += 2; continue; }
                    }
                    i++;
                }
            }
        } else {
            i += 2;
        }
    }
    *outlen = o;
    return i;
}

/* ================================================================
 * Load ConPTY from local conpty.dll (kernel32 fallback)
 * ================================================================ */
static int init_conpty(void) {
    g_hConPty = LoadLibraryW(L"conpty.dll");
    if (!g_hConPty) {
        VPRINT("conpty.dll not found, trying kernel32.dll");
        g_hConPty = LoadLibraryW(L"kernel32.dll");
    } else {
        VPRINT("loaded conpty.dll");
    }
    if (!g_hConPty) return 0;

    ConPtyCreate = (FN_CreatePseudoConsole)GetProcAddress(g_hConPty, "CreatePseudoConsole");
    ConPtyResize = (FN_ResizePseudoConsole)GetProcAddress(g_hConPty, "ResizePseudoConsole");
    ConPtyClose  = (FN_ClosePseudoConsole) GetProcAddress(g_hConPty, "ClosePseudoConsole");

    if (!ConPtyCreate || !ConPtyResize || !ConPtyClose) {
        VPRINT("ConPTY exports not found in module");
        FreeLibrary(g_hConPty);
        g_hConPty = NULL;
        return 0;
    }
    VPRINT("ConPTY API loaded");
    return 1;
}

/* ================================================================
 * Create ConPTY + launch shell
 * ================================================================ */
static int create_conpty(CLIENT_CTX *ctx, wchar_t *envBlock) {
    HANDLE hPipeInR = NULL, hPipeInW = NULL;
    HANDLE hPipeOutR = NULL, hPipeOutW = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    BOOL ok;

    VPRINT("creating pipes");
    if (!CreatePipe(&hPipeInR, &hPipeInW, &sa, 0))         return 0;
    if (!CreatePipe(&hPipeOutR, &hPipeOutW, &sa, 0)) {
        CloseHandle(hPipeInR); CloseHandle(hPipeInW);      return 0;
    }

    COORD sz = { 80, 25 };
    HPCON hpc;
    VPRINT("CreatePseudoConsole");
    if (FAILED(ConPtyCreate(sz, hPipeInR, hPipeOutW, 0, &hpc))) {
        CloseHandle(hPipeInR); CloseHandle(hPipeInW);
        CloseHandle(hPipeOutR); CloseHandle(hPipeOutW);    return 0;
    }

    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdOutput = hPipeOutW;
    si.StartupInfo.hStdError  = hPipeOutW;

    SIZE_T cbAttr;
    InitializeProcThreadAttributeList(NULL, 1, 0, &cbAttr);
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(cbAttr);
    if (!si.lpAttributeList) { ConPtyClose(hpc); CloseHandle(hPipeInR); CloseHandle(hPipeInW); CloseHandle(hPipeOutR); CloseHandle(hPipeOutW); return 0; }

    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &cbAttr);
    UpdateProcThreadAttribute(si.lpAttributeList, 0,
                              PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                              hpc, sizeof(HPCON), NULL, NULL);

    const char *sh = resolve_shell(g_shell);
    if (!sh) {
        VPRINT("shell not found %s", g_shell);
        free(si.lpAttributeList);
        ConPtyClose(hpc);
        CloseHandle(hPipeInR); CloseHandle(hPipeInW);
        CloseHandle(hPipeOutR); CloseHandle(hPipeOutW);
        return 0;
    }

    wchar_t cmdline[4096];
    if (_stricmp(g_shell, "bash") == 0) {
        char buf[MAX_PATH + 1];
        strcpy_s(buf, MAX_PATH, sh);
        strcat_s(buf, MAX_PATH, " --login -i");
        int wn = MultiByteToWideChar(CP_UTF8, 0, buf, -1, NULL, 0);
        wchar_t *wbuf = (wchar_t *)malloc(wn * sizeof(wchar_t));
        if (!wbuf) { free(si.lpAttributeList); ConPtyClose(hpc); CloseHandle(hPipeInR); CloseHandle(hPipeInW); CloseHandle(hPipeOutR); CloseHandle(hPipeOutW); return 0; }
        MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, wn);
        wcsncpy_s(cmdline, 4096, wbuf, _TRUNCATE);
        free(wbuf);
    } else {
        int wn = MultiByteToWideChar(CP_UTF8, 0, sh, -1, NULL, 0);
        wchar_t *wbuf = (wchar_t *)malloc(wn * sizeof(wchar_t));
        if (!wbuf) { free(si.lpAttributeList); ConPtyClose(hpc); CloseHandle(hPipeInR); CloseHandle(hPipeInW); CloseHandle(hPipeOutR); CloseHandle(hPipeOutW); return 0; }
        MultiByteToWideChar(CP_UTF8, 0, sh, -1, wbuf, wn);
        swprintf_s(cmdline, 4096, L"\"%s\"", wbuf);
        free(wbuf);
    }

    VPRINT("shell=%s", sh);
    ok = CreateProcessW(
        NULL, cmdline, NULL, NULL, TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        envBlock, NULL, &si.StartupInfo, &pi);
    VPRINT("CreateProcessW=%d pid=%lu", ok, ok ? pi.dwProcessId : 0);

    free(si.lpAttributeList);
    CloseHandle(hPipeInR);
    CloseHandle(hPipeOutW);

    if (!ok) {
        ConPtyClose(hpc);
        CloseHandle(hPipeInW);
        CloseHandle(hPipeOutR);
        return 0;
    }

    ctx->hpc          = hpc;
    ctx->hPipeInWrite = hPipeInW;
    ctx->hPipeOutRead = hPipeOutR;
    ctx->hShellProc   = pi.hProcess;
    ctx->hShellThread = pi.hThread;
    ctx->running      = 1;
    return 1;
}

/* ================================================================
 * Output thread  -  ConPTY pipe -> socket
 * Uses PeekNamedPipe polling so we can detect child process exit.
 * ================================================================ */
DWORD WINAPI output_thread(LPVOID param) {
    CLIENT_CTX *ctx = (CLIENT_CTX *)param;
    char in[4096];
    unsigned char out[8192];
    DWORD n;

    while (ctx->running && g_running) {
        DWORD avail = 0;
        if (!PeekNamedPipe(ctx->hPipeOutRead, NULL, 0, NULL, &avail, NULL)) {
            fprintf(stderr, "[ERR] output_thread: PeekNamedPipe failed, error=%lu\r\n", GetLastError());
            break;
        }
        if (avail == 0) {
            if (ctx->hShutdownEvent) {
                if (WaitForSingleObject(ctx->hShutdownEvent, 10) == WAIT_OBJECT_0) {
                    VPRINT("output_thread: shutdown signaled");
                    break;
                }
            } else {
                Sleep(10);
            }
            continue;
        }

        DWORD toRead = min(avail, (DWORD)sizeof(in));
        if (!ReadFile(ctx->hPipeOutRead, in, toRead, &n, NULL)) {
            fprintf(stderr, "[ERR] output_thread: ReadFile failed, error=%lu\r\n", GetLastError());
            break;
        }
        if (n == 0) {
            fprintf(stderr, "[ERR] output_thread: pipe EOF (shell exited)\r\n");
            break;
        }

        int o = 0;
        VPRINT("pipe->sock %lu bytes", n);
        if (g_verbose) {
            char dbg[128]; int di = 0;
            for (DWORD i = 0; i < n && di < (int)sizeof(dbg)-3; i++) {
                unsigned char c = (unsigned char)in[i];
                if (c >= 32 && c < 127) dbg[di++] = (char)c;
                else { dbg[di++] = '\\'; dbg[di++] = 'x'; di += sprintf_s(dbg+di, sizeof(dbg)-di, "%02X", c); }
            }
            dbg[di] = 0;
            VPRINT("  data: %s", dbg);
        }
        for (DWORD i = 0; i < n && o < (int)sizeof(out) - 2; i++) {
            unsigned char c = (unsigned char)in[i];
            if (c == TEL_IAC) { out[o++] = TEL_IAC; out[o++] = TEL_IAC; }
            else              { out[o++] = c; }
        }
        if (o > 0 && !send_all(ctx->sock, (const char *)out, o)) {
            fprintf(stderr, "[ERR] output_thread: send to client failed, error=%d\r\n", WSAGetLastError());
            break;
        }
    }

    /* Drain remaining pipe data (non-blocking) */
    DWORD avail;
    while (PeekNamedPipe(ctx->hPipeOutRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
        DWORD toRead = min(avail, (DWORD)sizeof(in));
        if (!ReadFile(ctx->hPipeOutRead, in, toRead, &n, NULL) || n == 0)
            break;
        int o = 0;
        for (DWORD i = 0; i < n && o < (int)sizeof(out) - 2; i++) {
            unsigned char c = (unsigned char)in[i];
            if (c == TEL_IAC) { out[o++] = TEL_IAC; out[o++] = TEL_IAC; }
            else              { out[o++] = c; }
        }
        if (o > 0 && !send_all(ctx->sock, (const char *)out, o))
            break;
    }

    shutdown(ctx->sock, SD_SEND);
    InterlockedExchange(&ctx->running, 0);
    SetEvent(ctx->hShutdownEvent);
    return 0;
}

/* ================================================================
 * Authentication
 * ================================================================ */
static int do_auth(CLIENT_CTX *ctx) {
    char buf[256];
    int pos = 0;

    /* Echo is already suppressed by WILL ECHO sent before auth */
    if (!send_all(ctx->sock, "Password: ", 10)) return 0;

    DWORD to = 30000;
    setsockopt(ctx->sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&to, sizeof(to));

    pos = 0;
    while (1) {
        char c;
        int n = recv(ctx->sock, &c, 1, 0);
        if (n <= 0) return 0;

        if ((unsigned char)c == TEL_IAC) {
            unsigned char cmd, opt;
            if (recv(ctx->sock, (char *)&cmd, 1, 0) <= 0) return 0;
            if (cmd == TEL_IAC) continue;
            if (recv(ctx->sock, (char *)&opt, 1, 0) <= 0) return 0;
            continue;
        }
        if (c == '\r' || c == '\n') break;
        if ((c == '\b' || c == 127) && pos > 0) pos--;
        else if (pos < (int)sizeof(buf) - 1) buf[pos++] = c;
    }
    buf[pos] = '\0';
    VPRINT("password read (%d chars)", pos);

    send_all(ctx->sock, "\r\n", 2);

    to = 0;
    setsockopt(ctx->sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&to, sizeof(to));

    if (strcmp(buf, g_password) != 0) {
        VPRINT("auth FAILED (got %d chars)", pos);
        send_all(ctx->sock, "\r\n*** Authentication failed: invalid password ***\r\n", 52);
        Sleep(2000);
        return 0;
    }
    VPRINT("auth OK");
    return 1;
}

/* ================================================================
 * Client handler thread
 * ================================================================ */
DWORD WINAPI client_thread(LPVOID param) {
    CLIENT_CTX *ctx = (CLIENT_CTX *)param;

    VPRINT("client thread started");

    wchar_t *envBlock = build_env_block();
    if (!envBlock) VPRINT("env block is NULL, using inherited environment");

    send_all(ctx->sock, "Windows Telnet Server (ConPTY)\r\n", 33);
    telnet_iac(ctx->sock, TEL_WILL, TELOPT_SUPPRESS_GA);
    telnet_iac(ctx->sock, TEL_WILL, TELOPT_ECHO);
    telnet_iac(ctx->sock, TEL_WILL, TELOPT_BINARY);

    if (g_auth && !do_auth(ctx)) {
        VPRINT("auth failed");
        free(envBlock);
        closesocket(ctx->sock);
        free(ctx);
        return 0;
    }

    if (!create_conpty(ctx, envBlock)) {
        VPRINT("create_conpty failed");
        free(envBlock);
        send_all(ctx->sock, "ERROR: shell not available\r\n", 28);
        closesocket(ctx->sock);
        free(ctx);
        return 0;
    }

    ctx->hShutdownEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ctx->hOutThread = CreateThread(NULL, 0, output_thread, ctx, 0, NULL);
    VPRINT("output thread started");

    /* Set a 1-second recv timeout so we can detect child process exit */
    DWORD rcvtimeo = 1000;
    setsockopt(ctx->sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&rcvtimeo, sizeof(rcvtimeo));

    unsigned char buf[4096];
    unsigned char data[4096];
    int remain = 0;
    unsigned char rbuf[4096];

    while (1) {
        if (!ctx->running || !g_running) {
            VPRINT("client already stopped");
            break;
        }
        /* Check if shutdown was signaled (child process exited) */
        if (WaitForSingleObject(ctx->hShutdownEvent, 0) == WAIT_OBJECT_0) {
            VPRINT("shutdown event signaled");
            break;
        }
        int ret = recv(ctx->sock, (char *)rbuf, sizeof(rbuf), 0);
        if (ret == SOCKET_ERROR) {
            int wsaErr = WSAGetLastError();
            if (wsaErr == WSAETIMEDOUT) {
                /* Timeout — check if child process has exited */
                if (WaitForSingleObject(ctx->hShellProc, 0) == WAIT_OBJECT_0) {
                    VPRINT("child process has exited");
                    SetEvent(ctx->hShutdownEvent);
                    break;
                }
                continue;
            }
            VPRINT("recv failed: %d", wsaErr);
            break;
        }
        if (ret == 0) { VPRINT("client disconnected"); break; }
        VPRINT("sock->pipe %d bytes", ret);
        if (g_verbose) {
            char dbg[128]; int di = 0;
            for (int i = 0; i < ret && di < (int)sizeof(dbg)-3; i++) {
                unsigned char c = (unsigned char)rbuf[i];
                if (c >= 32 && c < 127) dbg[di++] = (char)c;
                else { dbg[di++] = '\\'; dbg[di++] = 'x'; di += sprintf_s(dbg+di, sizeof(dbg)-di, "%02X", c); }
            }
            dbg[di] = 0;
            VPRINT("  data: %s", dbg);
        }

        if (remain + ret > (int)sizeof(buf)) remain = 0;
        memcpy(buf + remain, rbuf, ret);
        remain += ret;

        int consumed = 0;
        while (consumed < remain) {
            int datalen;
            int n = process_iac(buf + consumed, remain - consumed,
                                data, &datalen, ctx->sock, ctx);
            if (n <= 0) break;
            consumed += n;
            if (datalen > 0) {
                DWORD written;
                if (!WriteFile(ctx->hPipeInWrite, data, datalen, &written, NULL))
                    { remain = 0; goto done; }
            }
        }
        if (consumed > 0 && consumed < remain)
            memmove(buf, buf + consumed, remain - consumed);
        remain -= consumed;
    }

done:
    VPRINT("cleaning up client");
    InterlockedExchange(&ctx->running, 0);

    free(envBlock);

    /* Signal shutdown event so output_thread can exit promptly */
    if (ctx->hShutdownEvent) SetEvent(ctx->hShutdownEvent);

    /* Close pipe handles to unblock any pending I/O */
    if (ctx->hPipeInWrite) { CloseHandle(ctx->hPipeInWrite); ctx->hPipeInWrite = NULL; }
    if (ctx->hPipeOutRead) { CloseHandle(ctx->hPipeOutRead); ctx->hPipeOutRead = NULL; }

    if (ctx->hOutThread) {
        WaitForSingleObject(ctx->hOutThread, 5000);
        CloseHandle(ctx->hOutThread);
    }

    if (ctx->hpc)          { ConPtyClose(ctx->hpc); }
    if (ctx->hShellProc)   { WaitForSingleObject(ctx->hShellProc, 5000); CloseHandle(ctx->hShellProc); }
    if (ctx->hShellThread) { CloseHandle(ctx->hShellThread); }

    if (ctx->hShutdownEvent) CloseHandle(ctx->hShutdownEvent);

    closesocket(ctx->sock);
    free(ctx);
    return 0;
}

/* ================================================================
 * Console control handler
 * ================================================================ */
static BOOL WINAPI on_ctrl(DWORD ev) {
    if (ev == CTRL_C_EVENT || ev == CTRL_BREAK_EVENT) {
        g_running = 0;
        return TRUE;
    }
    return FALSE;
}

/* ================================================================
 * Main
 * ================================================================ */
int main(int argc, char **argv) {
    WSADATA wsa;

    printf("telnetd - Windows Telnet Server (ConPTY)\r\n\r\n");
    VPRINT("server starting");

    int argi = 1;
    if (argi < argc && argv[argi][0] != '-')
        strncpy_s(g_addr, sizeof(g_addr), argv[argi++], _TRUNCATE);

    for (; argi < argc; argi++) {
        if (!strcmp(argv[argi], "-p") && argi + 1 < argc) {
            g_port = atoi(argv[++argi]);
        } else if (!strcmp(argv[argi], "-k") && argi + 1 < argc) {
            strncpy_s(g_password, sizeof(g_password), argv[++argi], _TRUNCATE);
            g_auth = 1;
        } else if (!strcmp(argv[argi], "-s") && argi + 1 < argc) {
            strncpy_s(g_shell, sizeof(g_shell), argv[++argi], _TRUNCATE);
        } else if (!strcmp(argv[argi], "-t") && argi + 1 < argc) {
            strncpy_s(g_term, sizeof(g_term), argv[++argi], _TRUNCATE);
        } else if (!strcmp(argv[argi], "-fg")) {
            /* foreground mode (default) */
        } else if (!strcmp(argv[argi], "-V")) {
            g_verbose = 1;
        } else if (!strcmp(argv[argi], "-h")) {
            printf("Usage: %s [address] [-p port] [-k password] [-s cmd|powershell|bash] [-t term] [-fg] [-V]\r\n"
                   "  address     Listening address (default 0.0.0.0)\r\n"
                   "  -p port     Listening port (default 23)\r\n"
                   "  -k password Require password authentication\r\n"
                   "  -s shell    Shell to launch (default cmd)\r\n"
                   "  -t term     TERM variable value (default xterm-256color)\r\n"
                   "  -fg         Run in foreground (default)\r\n"
                   "  -V          Verbose/debug output\r\n"
                   "  -h          Display this help and exit\r\n",
                   argv[0]);
            return 0;
        } else {
            printf("Usage: %s [address] [-p port] [-k password] [-s cmd|powershell|bash] [-t term] [-fg] [-V]\r\n"
                   "  address     Listening address (default 0.0.0.0)\r\n"
                   "  -p port     Listening port (default 23)\r\n"
                   "  -k password Require password authentication\r\n"
                   "  -s shell    Shell to launch (default cmd)\r\n"
                   "  -t term     TERM variable value (default xterm-256color)\r\n"
                   "  -fg         Run in foreground (default)\r\n"
                   "  -V          Verbose/debug output\r\n",
                   argv[0]);
            return 0;
        }
    }

    if (!init_conpty()) {
        fprintf(stderr, "Error: ConPTY requires Windows 10 1809+ (build 17763)\n");
        return 1;
    }
    VPRINT("bind address=%s port=%d", g_addr, g_port);

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n"); return 1;
    }

    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) {
        fprintf(stderr, "socket: %d\n", WSAGetLastError());
        WSACleanup(); return 1;
    }

    int opt = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, g_addr, &sa.sin_addr);
    sa.sin_port = htons((unsigned short)g_port);

    if (bind(ls, (struct sockaddr *)&sa, sizeof(sa)) == SOCKET_ERROR) {
        fprintf(stderr, "bind %s:%d failed: %d (WSAEADDRNOTAVAIL=10049, WSAEADDRINUSE=10048)\n",
                g_addr, g_port, WSAGetLastError());
        closesocket(ls); WSACleanup(); return 1;
    }
    VPRINT("bind OK");
    if (listen(ls, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "listen: %d\n", WSAGetLastError());
        closesocket(ls); WSACleanup(); return 1;
    }

    printf("Listening on %s:%d ...\r\n", g_addr, g_port);
    printf("Shell: %s\r\n", g_shell);
    printf("TERM:  %s\r\n", g_term);
    printf("Auth:  %s\r\n\r\n", g_auth ? "enabled" : "disabled");

    SetConsoleCtrlHandler(on_ctrl, TRUE);

    while (g_running) {
        CLIENT_CTX *ctx = (CLIENT_CTX *)calloc(1, sizeof(CLIENT_CTX));
        if (!ctx) continue;

        int alen = sizeof(ctx->addr);
        ctx->sock = accept(ls, (struct sockaddr *)&ctx->addr, &alen);
        if (ctx->sock == INVALID_SOCKET) { free(ctx); continue; }

        int nd = 1;
        setsockopt(ctx->sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&nd, sizeof(nd));

        char ip[64];
        inet_ntop(AF_INET, &ctx->addr.sin_addr, ip, sizeof(ip));
        printf("+ %s:%d\r\n", ip, ntohs(ctx->addr.sin_port));
        VPRINT("accept %s:%d", ip, ntohs(ctx->addr.sin_port));

        HANDLE h = CreateThread(NULL, 0, client_thread, ctx, 0, NULL);
        if (h) CloseHandle(h);
        else   { closesocket(ctx->sock); free(ctx); }
    }

    VPRINT("server shutdown");
    printf("\r\nShutting down ...\r\n");
    closesocket(ls);
    WSACleanup();
    return 0;
}
