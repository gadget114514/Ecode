#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFSIZE 4096

#define IAC     255
#define DONT    254
#define DO      253
#define WONT    252
#define WILL    251
#define SB      250
#define SE      240
#define NOP     241
#define AYT     246

#define TELOPT_BINARY 0
#define TELOPT_ECHO  1
#define TELOPT_SGA   3

static SOCKET sock = INVALID_SOCKET;
static int use_ctrlc = 0;
static int force_noecho = 0;
static volatile int running = 1;
static HANDLE hConsole = INVALID_HANDLE_VALUE;
static DWORD original_mode = 0;
static unsigned long bytes_sent = 0, bytes_recv = 0;
static const char* remote_host = "";
static int remote_port = 0;
static int local_echo = 1;

static void restore_console(void) {
    if (hConsole != INVALID_HANDLE_VALUE)
        SetConsoleMode(hConsole, original_mode);
}

static BOOL WINAPI ctrl_handler(DWORD type) {
    if (type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        restore_console();
        printf("\033[0m\033[?25h");
        fflush(stdout);
        running = 0;
        if (sock != INVALID_SOCKET) {
            shutdown(sock, SD_BOTH);
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        return TRUE;
    }
    return FALSE;
}

static void send_iac(SOCKET s, int cmd, int opt) {
    unsigned char buf[3] = {IAC, cmd, (unsigned char)opt};
    send(s, (const char*)buf, 3, 0);
}

static void process_telnet_data(unsigned char* data, int len) {
    static int state = 0;
    unsigned char out[BUFSIZE];
    int out_len = 0;

    for (int i = 0; i < len; i++) {
        unsigned char c = data[i];
        switch (state) {
        case 0:
            if (c == IAC)
                state = 1;
            else if (out_len < BUFSIZE)
                out[out_len++] = c;
            break;
        case 1:
            if (c == IAC) {
                if (out_len < BUFSIZE) out[out_len++] = c;
                state = 0;
            } else if (c == WILL) {
                state = 2;
            } else if (c == WONT) {
                state = 3;
            } else if (c == DO) {
                state = 4;
            } else if (c == DONT) {
                state = 5;
            } else if (c == SB) {
                state = 6;
            } else if (c == AYT) {
                unsigned char resp[2] = {IAC, NOP};
                send(sock, (const char*)resp, 2, 0);
                state = 0;
            } else {
                state = 0;
            }
            break;
        case 2:
            if (c == TELOPT_ECHO) {
                send_iac(sock, DO, c);
                if (!force_noecho) local_echo = 0;
            } else if (c == TELOPT_SGA || c == TELOPT_BINARY) {
                send_iac(sock, DO, c);
            } else {
                send_iac(sock, DONT, c);
            }
            state = 0;
            break;
        case 3:
            if (c == TELOPT_ECHO && !force_noecho) local_echo = 1;
            send_iac(sock, DONT, c);
            state = 0;
            break;
        case 4:
            if (c == TELOPT_ECHO) {
                send_iac(sock, WILL, c);
                if (!force_noecho) local_echo = 1;
            } else if (c == TELOPT_SGA || c == TELOPT_BINARY) {
                send_iac(sock, WILL, c);
            } else {
                send_iac(sock, WONT, c);
            }
            state = 0;
            break;
        case 5:
            if (c == TELOPT_ECHO && !force_noecho) local_echo = 0;
            send_iac(sock, WONT, c);
            state = 0;
            break;
        case 6:
            if (c == IAC) state = 7;
            break;
        case 7:
            if (c == SE)
                state = 0;
            else if (c == IAC)
                state = 6;
            else
                state = 0;
            break;
        }
    }

    if (out_len > 0) {
        fwrite(out, 1, out_len, stdout);
        fflush(stdout);
    }
}

int main(int argc, char* argv[]) {
    remote_host = "127.0.0.1";
    remote_port = 23;
    const char* port_arg = NULL;
    int host_set = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            if (strcmp(argv[i], "--c") == 0) {
                use_ctrlc = 1;
            } else if (strcmp(argv[i], "--noecho") == 0) {
                force_noecho = 1;
                local_echo = 0;
            } else {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                return 1;
            }
        } else if (!host_set) {
            remote_host = argv[i];
            host_set = 1;
        } else if (!port_arg) {
            port_arg = argv[i];
        } else {
            fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
            return 1;
        }
    }

    if (port_arg) {
        remote_port = atoi(port_arg);
        if (remote_port <= 0 || remote_port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", port_arg);
            return 1;
        }
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "Error: WSAStartup failed\n");
        return 1;
    }

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    hConsole = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hConsole, &original_mode);
    SetConsoleMode(hConsole, original_mode & ~(ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", remote_port);

    int ret = getaddrinfo(remote_host, port_str, &hints, &res);
    if (ret != 0 || !res) {
        fprintf(stderr, "Error: could not resolve hostname '%s' (%s)\n",
                remote_host, gai_strerror(ret));
        restore_console();
        WSACleanup();
        return 1;
    }

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "Error: failed to create socket (WSA error %d)\n",
                WSAGetLastError());
        freeaddrinfo(res);
        restore_console();
        WSACleanup();
        return 1;
    }

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "Error: connection to %s:%d failed (WSA error %d)\n",
                remote_host, remote_port, WSAGetLastError());
        closesocket(sock);
        freeaddrinfo(res);
        restore_console();
        WSACleanup();
        return 1;
    }

    freeaddrinfo(res);

    printf("Connected to %s:%d.\n", remote_host, remote_port);
    if (use_ctrlc)
        printf("Press Ctrl+C to exit.\n");
    else
        printf("Escape character is 'Ctrl+]'.\n");
    if (force_noecho)
        printf("Local echo is off.\n");
    fflush(stdout);

    unsigned char buf[BUFSIZE];
    int escape_mode = 0;
    const char* exit_msg = NULL;
    int exit_code = 0;

    while (running) {
        INPUT_RECORD ir;
        DWORD nread;
        if (PeekConsoleInput(hConsole, &ir, 1, &nread) && nread > 0) {
            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) {
                ReadConsoleInput(hConsole, &ir, 1, &nread);
                continue;
            }
            int ch = ir.Event.KeyEvent.uChar.AsciiChar;
            ReadConsoleInput(hConsole, &ir, 1, &nread);
            if (ch == 0) continue;

            if (escape_mode) {
                if (ch == '\r' || ch == '\n') {
                    printf("\r\n");
                    fflush(stdout);
                    escape_mode = 0;
                } else if (ch == 'q' || ch == 'Q') {
                    exit_msg = "Connection closed.";
                    printf("\r\n");
                    fflush(stdout);
                    running = 0;
                    break;
                } else if (ch == 's' || ch == 'S') {
                    printf("\r\n  Connected: %s\r\n",
                           sock != INVALID_SOCKET ? "yes" : "no");
                    printf("  Address: %s:%d\r\n", remote_host, remote_port);
                    printf("  Sent: %lu bytes\r\n", bytes_sent);
                    printf("  Received: %lu bytes\r\n", bytes_recv);
                    printf("telnet> ");
                    fflush(stdout);
                } else {
                    printf("\r\n  Commands: q(uit), s(tatus),"
                           " <enter> to continue\r\n");
                    printf("telnet> ");
                    fflush(stdout);
                }
                continue;
            }

            if (!use_ctrlc && ch == 0x1D) {
                escape_mode = 1;
                printf("\r\ntelnet> ");
                fflush(stdout);
                continue;
            }

            if (use_ctrlc && ch == 0x03) {
                exit_msg = "Connection closed.";
                running = 0;
                break;
            }

            if (ch == '\r') {
                unsigned char crlf[2] = {'\r', '\n'};
                send(sock, (const char*)crlf, 2, 0);
                bytes_sent += 2;
            } else {
                unsigned char c = (unsigned char)ch;
                send(sock, (const char*)&c, 1, 0);
                bytes_sent++;
            }

            if (local_echo) {
                if (ch == '\r') {
                    putchar('\r');
                    putchar('\n');
                } else if (ch == '\b') {
                    putchar('\b');
                    putchar(' ');
                    putchar('\b');
                } else {
                    putchar(ch);
                }
                fflush(stdout);
            }
        }

        if (sock == INVALID_SOCKET) {
            exit_msg = "Connection terminated (Ctrl+Break or Close)";
            exit_code = 1;
            break;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv = {0, 100000};

        ret = select(0, &fds, NULL, NULL, &tv);
        if (ret < 0) {
            exit_code = 1;
            break;
        }

        if (ret > 0 && FD_ISSET(sock, &fds)) {
            int n = recv(sock, (char*)buf, BUFSIZE, 0);
            if (n <= 0) {
                exit_code = 1;
                break;
            }
            bytes_recv += n;
            process_telnet_data(buf, n);
        }
    }

    restore_console();
    FlushConsoleInputBuffer(hConsole);

    printf("\033[0m\033[?25h");
    fflush(stdout);

    if (sock != INVALID_SOCKET) {
        shutdown(sock, SD_SEND);
        closesocket(sock);
    }

    WSACleanup();

    printf("\n");
    if (exit_code) {
        printf("Connection to %s:%d terminated"
               " (sent %lu, received %lu bytes)\n",
               remote_host, remote_port, bytes_sent, bytes_recv);
    } else {
        printf("Connection to %s:%d closed"
               " (sent %lu, received %lu bytes)\n",
               remote_host, remote_port, bytes_sent, bytes_recv);
    }
    fflush(stdout);

    return exit_code;
}
