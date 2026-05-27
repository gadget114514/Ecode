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

/* ================================================================
 * Configuration
 * ================================================================ */
static int       g_port         = 23;
static char      g_addr[64]     = "0.0.0.0";
static char      g_password[256] = "";
static int       g_auth         = 0;
static char      g_shell[64]    = "cmd";
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
        return NULL;
    }
    return shell;
}

/* ================================================================
 * IAC processing
 * ================================================================ */
static int process_iac(const unsigned char *in, int inlen,
                       unsigned char *out, int *outlen,
                       SOCKET s)
{
    int i = 0, o = 0;

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
                               opt == TELOPT_STATUS || opt == TELOPT_TIMING_MARK)
                           ? TEL_DO : TEL_DONT, opt);
                break;
            case TEL_DO:
                telnet_iac(s, (opt == TELOPT_ECHO || opt == TELOPT_SUPPRESS_GA ||
                               opt == TELOPT_NAWS || opt == TELOPT_TERMINAL_TYPE)
                           ? TEL_WILL : TEL_WONT, opt);
                break;
            default: break;
            }
            i += 3;
        } else if (cmd == TEL_SB) {
            i += 2;
            while (i < inlen) {
                if (in[i] == TEL_IAC) {
                    if (i + 1 < inlen && in[i + 1] == TEL_SE) { i += 2; break; }
                    if (i + 1 < inlen && in[i + 1] == TEL_IAC) { i += 2; continue; }
                }
                i++;
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
static int create_conpty(CLIENT_CTX *ctx) {
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
        VPRINT("shell not found");
        free(si.lpAttributeList);
        ConPtyClose(hpc);
        CloseHandle(hPipeInR); CloseHandle(hPipeInW);
        CloseHandle(hPipeOutR); CloseHandle(hPipeOutW);
        return 0;
    }
    int wn = MultiByteToWideChar(CP_UTF8, 0, sh, -1, NULL, 0);
    wchar_t *wsh = (wchar_t *)malloc(wn * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, sh, -1, wsh, wn);

    wchar_t cmdline[4096];
    if (_stricmp(g_shell, "bash") == 0)
        swprintf_s(cmdline, 4096, L"\"%s\" --login -i", wsh);
    else
        swprintf_s(cmdline, 4096, L"\"%s\"", wsh);
    free(wsh);

    VPRINT("shell=%s", sh);
    ok = CreateProcessW(
        NULL, cmdline, NULL, NULL, TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        NULL, NULL, &si.StartupInfo, &pi);
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
 * ================================================================ */
DWORD WINAPI output_thread(LPVOID param) {
    CLIENT_CTX *ctx = (CLIENT_CTX *)param;
    char in[4096];
    unsigned char out[8192];
    DWORD n;

    while (1) {
        if (!ReadFile(ctx->hPipeOutRead, in, sizeof(in), &n, NULL)) {
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

    shutdown(ctx->sock, SD_SEND);
    InterlockedExchange(&ctx->running, 0);
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
    send_all(ctx->sock, "Windows Telnet Server (ConPTY)\r\n", 33);
    telnet_iac(ctx->sock, TEL_WILL, TELOPT_SUPPRESS_GA);
    telnet_iac(ctx->sock, TEL_WILL, TELOPT_ECHO);

    if (g_auth && !do_auth(ctx)) {
        VPRINT("auth failed");
        closesocket(ctx->sock);
        free(ctx);
        return 0;
    }

    if (!create_conpty(ctx)) {
        VPRINT("create_conpty failed");
        send_all(ctx->sock, "ERROR: shell not available\r\n", 28);
        closesocket(ctx->sock);
        free(ctx);
        return 0;
    }

    ctx->hOutThread = CreateThread(NULL, 0, output_thread, ctx, 0, NULL);
    VPRINT("output thread started");

    unsigned char buf[4096];
    unsigned char data[4096];
    int remain = 0;
    unsigned char rbuf[4096];

    while (1) {
        if (!ctx->running || !g_running) {
            VPRINT("client already stopped");
            break;
        }
        int ret = recv(ctx->sock, (char *)rbuf, sizeof(rbuf), 0);
        if (ret <= 0) { VPRINT("recv returned %d, client disconnected", ret); break; }
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
                                data, &datalen, ctx->sock);
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

    if (ctx->hPipeInWrite) { CloseHandle(ctx->hPipeInWrite); ctx->hPipeInWrite = NULL; }
    if (ctx->hPipeOutRead) { CloseHandle(ctx->hPipeOutRead); ctx->hPipeOutRead = NULL; }

    if (ctx->hOutThread) {
        WaitForSingleObject(ctx->hOutThread, 5000);
        CloseHandle(ctx->hOutThread);
    }

    if (ctx->hpc)          { ConPtyClose(ctx->hpc); }
    if (ctx->hShellProc)   { WaitForSingleObject(ctx->hShellProc, 5000); CloseHandle(ctx->hShellProc); }
    if (ctx->hShellThread) { CloseHandle(ctx->hShellThread); }

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
        } else if (!strcmp(argv[argi], "-fg")) {
            /* foreground mode (default) */
        } else if (!strcmp(argv[argi], "-V")) {
            g_verbose = 1;
        } else {
            printf("Usage: %s [address] [-p port] [-k password] [-s cmd|powershell|bash] [-fg] [-V]\r\n"
                   "  address     Listening address (default 0.0.0.0)\r\n"
                   "  -p port     Listening port (default 23)\r\n"
                   "  -k password Require password authentication\r\n"
                   "  -s shell    Shell to launch (default cmd)\r\n"
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
