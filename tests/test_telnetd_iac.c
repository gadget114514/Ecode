// test_telnetd_iac.c
// Tests the telnet IAC processing logic.
// Contains a self-contained copy of process_iac for testing.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Stub types matching mytelnetd.c
typedef int SOCKET;
#define INVALID_SOCKET (-1)
typedef struct { int dummy; } CLIENT_CTX;

// Copy of process_iac from mytelnetd.c
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
            i += 3;
        } else if (cmd == TEL_SB) {
            i += 2;
            sb_opt = (i < inlen) ? in[i] : 0;
            if (sb_opt == TELOPT_NAWS) {
                i++;
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
                while (i < inlen) {
                    if (in[i] == TEL_IAC && i + 1 < inlen && in[i + 1] == TEL_SE) { i += 2; break; }
                    i++;
                }
            } else {
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

// Test helpers
#define VERIFY(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
        failures++; \
    } else { \
        passes++; \
    } \
} while(0)

static int failures = 0, passes = 0;

static void test_passthrough() {
    unsigned char in[] = "Hello World";
    unsigned char out[64];
    int outlen;
    int consumed;

    consumed = process_iac(in, 5, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 5, "");
    VERIFY(outlen == 5, "");
    VERIFY(memcmp(out, "Hello", 5) == 0, "");
}

static void test_iac_iac() {
    unsigned char in[] = { TEL_IAC, TEL_IAC, 'A', 'B' };
    unsigned char out[64];
    int outlen;
    int consumed;

    consumed = process_iac(in, 4, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 4, "");
    VERIFY(outlen == 3, "");
    VERIFY(out[0] == TEL_IAC, "");
    VERIFY(out[1] == 'A', "");
    VERIFY(out[2] == 'B', "");
}

static void test_iac_will() {
    unsigned char in[] = { TEL_IAC, TEL_WILL, TELOPT_ECHO, 'X' };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 4, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 4, "");
    VERIFY(outlen == 1, "");
    VERIFY(out[0] == 'X', "");
}

static void test_iac_wont() {
    unsigned char in[] = { TEL_IAC, TEL_WONT, TELOPT_ECHO, 'Y' };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 4, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 4, "");
    VERIFY(outlen == 1, "");
    VERIFY(out[0] == 'Y', "");
}

static void test_iac_do() {
    unsigned char in[] = { TEL_IAC, TEL_DO, TELOPT_ECHO, 'Z' };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 4, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 4, "");
    VERIFY(outlen == 1, "");
    VERIFY(out[0] == 'Z', "");
}

static void test_iac_dont() {
    unsigned char in[] = { TEL_IAC, TEL_DONT, TELOPT_ECHO, '!' };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 4, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 4, "");
    VERIFY(outlen == 1, "");
    VERIFY(out[0] == '!', "");
}

static void test_iac_sb_unknown() {
    unsigned char in[] = { TEL_IAC, TEL_SB, 99, 1, 2, 3, TEL_IAC, TEL_SE, 'A' };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 9, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 9, "");
    VERIFY(outlen == 1, "");
    VERIFY(out[0] == 'A', "");
}

static void test_iac_sb_naws() {
    unsigned char in[] = { TEL_IAC, TEL_SB, TELOPT_NAWS, 0, 80, 0, 24, TEL_IAC, TEL_SE, 'B' };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 10, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 10, "");
    VERIFY(outlen == 1, "");
    VERIFY(out[0] == 'B', "");
}

static void test_iac_sb_naws_too_short() {
    unsigned char in[] = { TEL_IAC, TEL_SB, TELOPT_NAWS, 0, 80 };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 5, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 5, "");
    VERIFY(outlen == 0, "");
}

static void test_iac_unknown_command() {
    unsigned char in[] = { TEL_IAC, 0x01, 'C' };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 3, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 3, "");
    VERIFY(outlen == 1, "");
    VERIFY(out[0] == 'C', "");
}

static void test_mixed_data_and_iac() {
    unsigned char in[] = { 'A', 'B', TEL_IAC, TEL_IAC, 'C', TEL_IAC, TEL_WILL, TELOPT_ECHO, 'D' };
    unsigned char out[64];
    int outlen, consumed;
    unsigned char expected[] = { 'A', 'B', TEL_IAC, 'C', 'D' };
    consumed = process_iac(in, 9, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 9, "");
    VERIFY(outlen == 5, "");
    VERIFY(memcmp(out, expected, 5) == 0, "");
}

static void test_partial_iac_at_end() {
    unsigned char in[] = { 'X', 'Y', TEL_IAC };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 3, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 2, "");
    VERIFY(outlen == 2, "");
    VERIFY(memcmp(out, "XY", 2) == 0, "");
}

static void test_partial_iac_will_at_end() {
    unsigned char in[] = { 'P', TEL_IAC, TEL_WILL };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 3, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 1, "");
    VERIFY(outlen == 1, "");
    VERIFY(out[0] == 'P', "");
}

static void test_empty_input() {
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac((const unsigned char*)"", 0, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 0, "");
    VERIFY(outlen == 0, "");
}

static void test_all_iac_doubled() {
    unsigned char in[512];
    unsigned char out[1024];
    int outlen, consumed;
    int i;
    for (i = 0; i < 256; i++) {
        in[i * 2] = TEL_IAC;
        in[i * 2 + 1] = TEL_IAC;
    }
    consumed = process_iac(in, 512, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 512, "");
    VERIFY(outlen == 256, "");
    for (i = 0; i < 256; i++)
        VERIFY(out[i] == TEL_IAC, "");
}

static void test_sb_with_embedded_iac_iac() {
    unsigned char in[] = { TEL_IAC, TEL_SB, 99, TEL_IAC, TEL_IAC, TEL_IAC, TEL_SE, 'Q' };
    unsigned char out[64];
    int outlen, consumed;
    consumed = process_iac(in, 8, out, &outlen, INVALID_SOCKET, NULL);
    VERIFY(consumed == 8, "");
    VERIFY(outlen == 1, "");
    VERIFY(out[0] == 'Q', "");
}

int main() {
    printf("=== Telnet IAC Processing Tests ===\n\n");

    test_passthrough();
    test_iac_iac();
    test_iac_will();
    test_iac_wont();
    test_iac_do();
    test_iac_dont();
    test_iac_sb_unknown();
    test_iac_sb_naws();
    test_iac_sb_naws_too_short();
    test_iac_unknown_command();
    test_mixed_data_and_iac();
    test_partial_iac_at_end();
    test_partial_iac_will_at_end();
    test_empty_input();
    test_all_iac_doubled();
    test_sb_with_embedded_iac_iac();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
