#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

#define TEL_IAC   0xFF
#define TEL_WILL  0xFB
#define TEL_WONT  0xFC
#define TEL_DO    0xFD
#define TEL_DONT  0xFE
#define TEL_SB    0xFA
#define TEL_SE    0xF0

#define TELOPT_ECHO           1
#define TELOPT_SUPPRESS_GA    3
#define TELOPT_STATUS         5
#define TELOPT_TERMINAL_TYPE 24
#define TELOPT_NAWS          31
#define TELOPT_LINEMODE      34

static HANDLE g_hStdIn  = nullptr;
static HANDLE g_hStdOut = nullptr;
static HANDLE g_hStdErr = nullptr;

static std::atomic<bool> g_connected{false};
static std::atomic<bool> g_running{true};
static SOCKET g_sock = INVALID_SOCKET;
static bool g_verbose = false;
static bool g_rawMode = false;
static bool g_hexDump = false;
static bool g_crlf = false;

static const char* OptName(unsigned char opt) {
    switch (opt) {
        case TELOPT_ECHO:          return "ECHO";
        case TELOPT_SUPPRESS_GA:   return "SUPPRESS_GA";
        case TELOPT_STATUS:        return "STATUS";
        case TELOPT_TERMINAL_TYPE: return "TERMINAL_TYPE";
        case TELOPT_NAWS:          return "NAWS";
        case TELOPT_LINEMODE:      return "LINEMODE";
        default: return "?";
    }
}

static void DebugLog(const char* fmt, ...) {
    if (!g_verbose) return;
    va_list args;
    va_start(args, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    DWORD w = 0;
    WriteConsoleA(g_hStdErr, buf, (DWORD)strlen(buf), &w, nullptr);
}

static bool SendRaw(const void* data, int len) {
    return send(g_sock, (const char*)data, len, 0) != SOCKET_ERROR;
}

static bool SendIAC(unsigned char cmd, unsigned char opt) {
    unsigned char buf[3] = { TEL_IAC, cmd, opt };
    return SendRaw(buf, 3);
}

static size_t ProcessTelnet(const unsigned char* buf, size_t len,
                            std::vector<char>& data) {
    size_t i = 0;
    while (i < len) {
        if (buf[i] != TEL_IAC) {
            data.push_back((char)buf[i]);
            i++;
            continue;
        }
        if (i + 1 >= len) return i;

        unsigned char cmd = buf[i + 1];

        if (cmd == TEL_IAC) {
            data.push_back((char)TEL_IAC);
            i += 2;
            continue;
        }

        if (cmd == TEL_WILL || cmd == TEL_WONT) {
            if (i + 2 >= len) return i;
            unsigned char opt = buf[i + 2];
            if (cmd == TEL_WILL) {
                DebugLog("[IAC] WILL %s (%d)\r\n", OptName(opt), opt);
                switch (opt) {
                    case TELOPT_SUPPRESS_GA:
                    case TELOPT_ECHO:
                        SendIAC(TEL_DO, opt);
                        DebugLog("[IAC] --> DO %s\r\n", OptName(opt));
                        break;
                    default:
                        SendIAC(TEL_DONT, opt);
                        DebugLog("[IAC] --> DONT %s\r\n", OptName(opt));
                        break;
                }
            }
            i += 3;
            continue;
        }

        if (cmd == TEL_DO || cmd == TEL_DONT) {
            if (i + 2 >= len) return i;
            unsigned char opt = buf[i + 2];
            if (cmd == TEL_DO) {
                DebugLog("[IAC] DO %s (%d)\r\n", OptName(opt), opt);
                switch (opt) {
                    case TELOPT_SUPPRESS_GA:
                        SendIAC(TEL_WILL, opt);
                        DebugLog("[IAC] --> WILL %s\r\n", OptName(opt));
                        break;
                    default:
                        SendIAC(TEL_WONT, opt);
                        DebugLog("[IAC] --> WONT %s\r\n", OptName(opt));
                        break;
                }
            }
            i += 3;
            continue;
        }

        if (cmd == TEL_SB) {
            size_t end = i + 2;
            while (end < len) {
                if (buf[end] == TEL_IAC && end + 1 < len && buf[end + 1] == TEL_SE) {
                    DebugLog("[IAC] SB option=%d length=%zu\r\n", buf[i + 2], end - (i + 2));
                    end += 2;
                    break;
                }
                end++;
            }
            if (end >= len) return i;
            i = end;
            continue;
        }

        DebugLog("[IAC] unknown cmd=0x%02X\r\n", cmd);
        i += 2;
    }
    return i;
}

static void SocketReaderThread() {
    std::vector<char> buf(4096);
    std::vector<char> data;

    while (g_running && g_connected) {
        int ret = recv(g_sock, buf.data(), (int)buf.size(), 0);
        if (ret == 0) {
            g_connected = false;
            break;
        }
        if (ret < 0) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                Sleep(10);
                continue;
            }
            g_connected = false;
            break;
        }

        if (g_hexDump) {
            char hex[4096 * 3 + 128];
            int pos = 0;
            pos += snprintf(hex + pos, sizeof(hex) - pos, "[HEX %d bytes] ", ret);
            for (int j = 0; j < ret && j < 128 && pos < (int)sizeof(hex) - 8; j++) {
                pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", (unsigned char)buf[j]);
            }
            if (ret > 128) {
                pos += snprintf(hex + pos, sizeof(hex) - pos, "(+%d more)", ret - 128);
            }
            pos += snprintf(hex + pos, sizeof(hex) - pos, "\r\n");
            DWORD w = 0;
            WriteConsoleA(g_hStdErr, hex, (DWORD)strlen(hex), &w, nullptr);
        }

        if (g_rawMode) {
            if (g_crlf) {
                std::vector<char> crlfBuf;
                crlfBuf.reserve(ret * 2);
                for (int j = 0; j < ret; j++) {
                    if (buf[j] == '\n' && (j == 0 || buf[j - 1] != '\r'))
                        crlfBuf.push_back('\r');
                    crlfBuf.push_back(buf[j]);
                }
                DWORD written = 0;
                WriteConsoleA(g_hStdOut, crlfBuf.data(), (DWORD)crlfBuf.size(), &written, nullptr);
            } else {
                DWORD written = 0;
                WriteConsoleA(g_hStdOut, buf.data(), (DWORD)ret, &written, nullptr);
            }
        } else {
            data.clear();
            ProcessTelnet((const unsigned char*)buf.data(), ret, data);
            if (!data.empty()) {
                if (g_crlf) {
                    std::vector<char> crlfData;
                    crlfData.reserve(data.size() * 2);
                    for (size_t j = 0; j < data.size(); j++) {
                        if (data[j] == '\n' && (j == 0 || data[j - 1] != '\r'))
                            crlfData.push_back('\r');
                        crlfData.push_back(data[j]);
                    }
                    DWORD written = 0;
                    WriteConsoleA(g_hStdOut, crlfData.data(), (DWORD)crlfData.size(), &written, nullptr);
                } else {
                    DWORD written = 0;
                    WriteConsoleA(g_hStdOut, data.data(), (DWORD)data.size(), &written, nullptr);
                }
            }
        }
    }
    g_connected = false;
}

static void ConsoleInputReaderThread() {
    while (g_running) {
        DWORD events = 0;
        if (!GetNumberOfConsoleInputEvents(g_hStdIn, &events)) break;
        if (events == 0) { Sleep(10); continue; }

        INPUT_RECORD rec[128];
        DWORD readCount = 0;
        if (!ReadConsoleInputW(g_hStdIn, rec, 128, &readCount)) break;

        for (DWORD i = 0; i < readCount; i++) {
            if (rec[i].EventType != KEY_EVENT) continue;
            if (!rec[i].Event.KeyEvent.bKeyDown) continue;

            KEY_EVENT_RECORD& key = rec[i].Event.KeyEvent;

            if (key.wVirtualKeyCode == VK_OEM_6 &&
                (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) {
                continue;
            }

            wchar_t wch = key.uChar.UnicodeChar;
            if (wch == 0) continue;

            char utf8[8];
            int len = WideCharToMultiByte(CP_UTF8, 0, &wch, 1,
                                          utf8, sizeof(utf8), nullptr, nullptr);
            if (len > 0) SendRaw(utf8, len);
        }
    }
}

static void PrintUsage() {
    printf("myclient - TCP telnet client\n"
           "Usage: myclient.exe [-v] [--raw] [--hex] [--crlf] <host> [port]\n"
           "  -v       verbose IAC negotiation logging\n"
           "  --raw    pass all bytes through without IAC processing\n"
           "  --hex    hex dump all received data to stderr\n"
           "  --crlf    convert LF -> CRLF in server output\n"
           "  host     remote hostname or IP address\n"
           "  port     remote port (default: 23)\n");
}

int main(int argc, char* argv[]) {
    g_hStdIn  = GetStdHandle(STD_INPUT_HANDLE);
    g_hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    g_hStdErr = GetStdHandle(STD_ERROR_HANDLE);

    int argIdx = 1;
    while (argIdx < argc) {
        if (strcmp(argv[argIdx], "-v") == 0) { g_verbose = true; argIdx++; }
        else if (strcmp(argv[argIdx], "--raw") == 0) { g_rawMode = true; argIdx++; }
        else if (strcmp(argv[argIdx], "--hex") == 0) { g_hexDump = true; argIdx++; }
        else if (strcmp(argv[argIdx], "--crlf") == 0) { g_crlf = true; argIdx++; }
        else break;
    }

    if (argc < argIdx + 1) { PrintUsage(); return 1; }
    const char* host = argv[argIdx];
    int port = (argc >= argIdx + 2) ? atoi(argv[argIdx + 1]) : 23;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n"); return 1;
    }

    struct addrinfo hints{}, *ai = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    char pstr[16]; snprintf(pstr, sizeof(pstr), "%d", port);

    int rc = getaddrinfo(host, pstr, &hints, &ai);
    if (rc != 0) {
        fprintf(stderr, "Failed to resolve '%s': %d\n", host, rc);
        WSACleanup(); return 1;
    }

    g_sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (g_sock == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed\n");
        freeaddrinfo(ai); WSACleanup(); return 1;
    }

    DebugLog("[myclient] Connecting to %s:%d...\r\n", host, port);
    printf("Connecting to %s:%d...\n", host, port);
    if (connect(g_sock, ai->ai_addr, (int)ai->ai_addrlen) != 0) {
        fprintf(stderr, "connect() failed: %d\n", WSAGetLastError());
        closesocket(g_sock); freeaddrinfo(ai); WSACleanup(); return 1;
    }
    freeaddrinfo(ai);
    printf("Connected.\n");
    DebugLog("[myclient] Connected.\r\n");

    u_long nonblock = 1;
    ioctlsocket(g_sock, FIONBIO, &nonblock);

    struct { DWORD inMode, outMode; } orig;
    GetConsoleMode(g_hStdIn, &orig.inMode);
    GetConsoleMode(g_hStdOut, &orig.outMode);

    DWORD inMode  = orig.inMode;
    DWORD outMode = orig.outMode;
    inMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    inMode |= ENABLE_WINDOW_INPUT;
    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(g_hStdIn, inMode);
    SetConsoleMode(g_hStdOut, outMode);

    g_connected = true;

    std::thread reader(SocketReaderThread);
    std::thread inputReader(ConsoleInputReaderThread);

    while (g_connected && g_running) Sleep(100);

    g_running = false;
    if (inputReader.joinable()) inputReader.join();
    if (reader.joinable()) reader.join();

    SetConsoleMode(g_hStdIn, orig.inMode);
    SetConsoleMode(g_hStdOut, orig.outMode);

    printf("\nConnection closed.\n");

    if (g_sock != INVALID_SOCKET) closesocket(g_sock);
    WSACleanup();
    return 0;
}
