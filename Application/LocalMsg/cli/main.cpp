// =============================================================================
// localmsg-cli — Cross-agent messaging CLI for ecode
//
// Agent identity: --agent <name> (required on most commands)
//   Fallback: reads .agent-identity file from CWD
//
// Usage:
//   localmsg-cli --login   --agent <name>
//   localmsg-cli --logout  --agent <name>
//   localmsg-cli --send    --agent <name> --to <peer> <text>
//   localmsg-cli --send    --agent <name> --to <peer> --file <path> [--file <path2>...]
//   localmsg-cli --receive --agent <name> [--peek]
//   localmsg-cli --wait    --agent <name> [--timeout <sec>]
//   localmsg-cli --files   --agent <name> [--peek]
//   localmsg-cli --list
//
// LocalMsg.exe must be running (started by ecode as a plugin).
// Exit codes: 0=ok, 1=empty/timeout, 2=error
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")

#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <vector>
#include <cstdio>
#include <ctime>

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------
static std::wstring s2ws(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    while (!w.empty() && w.back() == 0) w.pop_back();
    return w;
}
static std::string ws2s(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    while (!s.empty() && s.back() == 0) s.pop_back();
    return s;
}
static std::string EscapeJson(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else                r += (char)c;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------
static void PrintJson(const std::string& s) {
    DWORD written = 0;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    std::string out = s + "\n";
    WriteFile(hOut, out.c_str(), (DWORD)out.size(), &written, nullptr);
}
static void PrintError(const std::string& msg) {
    PrintJson("{\"error\":\"" + EscapeJson(msg) + "\"}");
}
static void PrintUsage() {
    PrintJson("{\"usage\":["
        "\"localmsg-cli --login   --agent <name>\","
        "\"localmsg-cli --logout  --agent <name>\","
        "\"localmsg-cli --send    --agent <name> --to <peer> <text>\","
        "\"localmsg-cli --send    --agent <name> --to <peer> --file <path> ...\","
        "\"localmsg-cli --receive --agent <name> [--peek]\","
        "\"localmsg-cli --wait    --agent <name> [--timeout <sec>]\","
        "\"localmsg-cli --files   --agent <name> [--peek]\","
        "\"localmsg-cli --list\""
        "]}");
}

// ---------------------------------------------------------------------------
// Server availability check
// LocalMsg.exe is started by ecode's plugin system — we never start it here.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Port resolution:
//   1. --httpport / --udpport CLI flag
//   2. LOCALMSG_HTTPPORT / LOCALMSG_UDPPORT environment variable
//   3. Default (2426 / 2425)
// ---------------------------------------------------------------------------
static int ResolvePort(int argc, LPWSTR* argv,
                       const std::string& flag,
                       const char* envVar,
                       int defaultPort) {
    // 1. CLI flag
    for (int i = 1; i < argc - 1; ++i)
        if (ws2s(argv[i]) == flag) {
            int p = atoi(ws2s(argv[i+1]).c_str());
            if (p > 0 && p < 65536) return p;
        }
    // 2. Environment variable
    char buf[16] = {};
    if (GetEnvironmentVariableA(envVar, buf, sizeof(buf)) > 0) {
        int p = atoi(buf);
        if (p > 0 && p < 65536) return p;
    }
    // 3. Default
    return defaultPort;
}

static bool EnsureServer(int port = 2426) {
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    // non-blocking connect with 500ms timeout
    u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
    sockaddr_in a = {}; a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(0x7f000001);
    a.sin_port = htons((u_short)port);
    connect(s, (sockaddr*)&a, sizeof(a));
    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds);
    timeval tv = {1, 0};   // 1 second timeout
    bool alive = (select(0, nullptr, &fds, nullptr, &tv) > 0);
    closesocket(s);
    return alive;
}

// ---------------------------------------------------------------------------
// Agent name resolution: --agent flag → .agent-identity in CWD → error
// ---------------------------------------------------------------------------
static std::string ResolveAgent(int argc, LPWSTR* argv) {
    for (int i = 1; i < argc - 1; ++i)
        if (ws2s(argv[i]) == "--agent") return ws2s(argv[i+1]);
    // fallback: .agent-identity in CWD
    HANDLE h = CreateFileW(L".agent-identity", GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        char buf[64] = {}; DWORD r = 0;
        ReadFile(h, buf, (DWORD)sizeof(buf)-1, &r, nullptr);
        CloseHandle(h);
        std::string name(buf, r);
        while (!name.empty() && (name.back()=='\r'||name.back()=='\n'||name.back()==' '))
            name.pop_back();
        if (!name.empty()) return name;
    }
    return {};
}

// Retrieve a named flag value from argv (e.g. "--to" "codex")
static std::string GetFlag(int argc, LPWSTR* argv, const std::string& flag) {
    for (int i = 1; i < argc - 1; ++i)
        if (ws2s(argv[i]) == flag) return ws2s(argv[i+1]);
    return {};
}

// Check if a boolean flag is present
static bool HasFlag(int argc, LPWSTR* argv, const std::string& flag) {
    for (int i = 1; i < argc; ++i)
        if (ws2s(argv[i]) == flag) return true;
    return false;
}

// Collect all --file <path> values
static std::vector<std::string> GetFiles(int argc, LPWSTR* argv) {
    std::vector<std::string> files;
    for (int i = 1; i < argc - 1; ++i)
        if (ws2s(argv[i]) == "--file") files.push_back(ws2s(argv[i+1]));
    return files;
}

// ---------------------------------------------------------------------------
// HTTP REST call helper (localhost)
// ---------------------------------------------------------------------------
static std::string CallApi(const std::string& method, const std::string& path,
                           const std::string& body, int port = 2426,
                           int timeoutMs = 30000) {
    HINTERNET hSess = WinHttpOpen(L"localmsg-cli/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return "{\"error\":\"WinHttpOpen failed\"}";

    HINTERNET hConn = WinHttpConnect(hSess, L"127.0.0.1", (INTERNET_PORT)port, 0);
    if (!hConn) {
        WinHttpCloseHandle(hSess);
        return "{\"error\":\"Cannot connect to LocalMsg REST API on port "
               + std::to_string(port) + "\"}";
    }

    HINTERNET hReq = WinHttpOpenRequest(hConn, s2ws(method).c_str(), s2ws(path).c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hReq) {
        WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
        return "{\"error\":\"WinHttpOpenRequest failed\"}";
    }

    // Set timeouts: resolve, connect, send, receive
    WinHttpSetTimeouts(hReq, 5000, 5000, 5000, timeoutMs);

    std::wstring hdrs = L"Content-Type: application/json\r\n";
    WinHttpAddRequestHeaders(hReq, hdrs.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    DWORD bl = (DWORD)body.size();
    bool sent = WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(), bl, bl, 0);

    std::string result;
    if (sent && WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
            std::string buf(avail, 0); DWORD read = 0;
            WinHttpReadData(hReq, &buf[0], avail, &read);
            buf.resize(read); result += buf;
        }
    } else {
        DWORD err = GetLastError();
        char ebuf[64]; snprintf(ebuf, sizeof(ebuf), "%lu", (unsigned long)err);
        result = "{\"error\":\"API call failed (WinHTTP error " + std::string(ebuf) + ")\"}";
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return result;
}

// ---------------------------------------------------------------------------
// LocalSend HTTPS file transfer (mirrors SendFileThread in src/main.cpp)
// Sends a file to LocalMsg.exe's LocalSend HTTPS listener (port 53317)
// with "destinationUser" so the server auto-accepts it into the agent's inbox.
// ---------------------------------------------------------------------------
static std::string MakeId16() {
    static DWORD counter = 0;
    char buf[32];
    snprintf(buf, sizeof(buf), "%016I64x", (unsigned long long)GetTickCount64() ^ (++counter));
    return buf;
}

static bool SendFileLocalSend(const std::string& fromAgent,
                               const std::string& toAgent,
                               const std::string& filePath,
                               int lsPort = 53317) {
    // Open file
    std::wstring wPath = s2ws(filePath);
    HANDLE hf = CreateFileW(wPath.c_str(), GENERIC_READ,
                            FILE_SHARE_READ|FILE_SHARE_WRITE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        PrintError("Cannot open file: " + filePath); return false;
    }
    LARGE_INTEGER fsize = {}; GetFileSizeEx(hf, &fsize);

    // Extract filename from path
    size_t slash = filePath.find_last_of("/\\");
    std::string filename = (slash != std::string::npos) ? filePath.substr(slash+1) : filePath;

    HINTERNET hSess = WinHttpOpen(L"localmsg-cli/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConn = hSess ? WinHttpConnect(hSess, L"127.0.0.1", (INTERNET_PORT)lsPort, 0) : nullptr;

    std::string fileId   = MakeId16();
    std::string sessionId, token;

    // Step 1: prepare-upload
    if (hConn) {
        std::string prepBody =
            "{\"info\":{\"alias\":\"" + EscapeJson(fromAgent) + "\","
            "\"version\":\"2.0\",\"deviceModel\":\"PC\",\"deviceType\":\"desktop\","
            "\"fingerprint\":\"localmsg-cli\"},"
            "\"destinationUser\":\"" + EscapeJson(toAgent) + "\","
            "\"files\":{\"" + fileId + "\":{\"id\":\"" + fileId + "\","
            "\"filename\":\"" + EscapeJson(filename) + "\","
            "\"size\":" + std::to_string(fsize.QuadPart) + ","
            "\"fileType\":\"application/octet-stream\"}}}";

        HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST",
            L"/api/localsend/v2/prepare-upload",
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (hReq) {
            DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA
                        | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
                        | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
                        | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
            WinHttpSetTimeouts(hReq, 5000, 5000, 10000, 30000);
            WinHttpAddRequestHeaders(hReq,
                L"Content-Type: application/json\r\n", (DWORD)-1,
                WINHTTP_ADDREQ_FLAG_ADD);
            DWORD bl = (DWORD)prepBody.size();
            if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   (LPVOID)prepBody.c_str(), bl, bl, 0)
                && WinHttpReceiveResponse(hReq, nullptr)) {
                DWORD avail = 0; WinHttpQueryDataAvailable(hReq, &avail);
                if (avail > 0) {
                    std::string resp(avail, 0); DWORD read = 0;
                    WinHttpReadData(hReq, &resp[0], avail, &read); resp.resize(read);
                    // parse sessionId
                    size_t sp = resp.find("\"sessionId\"");
                    if (sp != std::string::npos) {
                        size_t vs = resp.find('"', sp+12);
                        if (vs != std::string::npos) {
                            size_t ve = resp.find('"', vs+1);
                            if (ve != std::string::npos) sessionId = resp.substr(vs+1, ve-vs-1);
                        }
                    }
                    // parse token: look for "files":{...fileId:token...}
                    size_t fp = resp.find("\"files\"");
                    if (fp != std::string::npos) {
                        size_t ob = resp.find('{', fp+7);
                        if (ob != std::string::npos) {
                            size_t oe = resp.find('}', ob);
                            if (oe != std::string::npos) {
                                std::string fobj = resp.substr(ob, oe-ob+1);
                                size_t tp = fobj.find(fileId);
                                if (tp != std::string::npos) {
                                    size_t colon = fobj.find(':', tp + fileId.size());
                                    if (colon != std::string::npos) {
                                        size_t vs2 = fobj.find('"', colon);
                                        if (vs2 != std::string::npos) {
                                            size_t ve2 = fobj.find('"', vs2+1);
                                            if (ve2 != std::string::npos)
                                                token = fobj.substr(vs2+1, ve2-vs2-1);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            WinHttpCloseHandle(hReq);
        }
    }

    bool ok = false;
    // Step 2: upload
    if (hConn && !token.empty()) {
        if (sessionId.empty()) sessionId = fileId;
        std::string upPath = "/api/localsend/v2/upload?sessionId=" + sessionId
                           + "&fileId=" + fileId + "&token=" + token;
        HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", s2ws(upPath).c_str(),
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (hReq) {
            DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA
                        | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
                        | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
                        | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
            WinHttpSetTimeouts(hReq, 5000, 5000, 30000, 300000);
            WinHttpAddRequestHeaders(hReq,
                L"Content-Type: application/octet-stream\r\n", (DWORD)-1,
                WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0,
                                   (DWORD)fsize.QuadPart, 0)) {
                char buf[65536]; int64_t sent = 0; BOOL ro = TRUE;
                while (ro && sent < fsize.QuadPart) {
                    DWORD r = 0;
                    ro = ReadFile(hf, buf, sizeof(buf), &r, nullptr);
                    if (!r) break;
                    DWORD w = 0;
                    WinHttpWriteData(hReq, buf, r, &w);
                    sent += w;
                }
                WinHttpReceiveResponse(hReq, nullptr);
                ok = (sent >= fsize.QuadPart);
            }
            WinHttpCloseHandle(hReq);
        }
    }

    CloseHandle(hf);
    if (hConn) WinHttpCloseHandle(hConn);
    if (hSess) WinHttpCloseHandle(hSess);
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2) { PrintUsage(); return 2; }

    std::string cmd = ws2s(argv[1]);

    // Resolve ports: --httpport / LOCALMSG_HTTPPORT / 2426
    //                --udpport  / LOCALMSG_UDPPORT  / 2425
    int httpPort = ResolvePort(argc, argv, "--httpport", "LOCALMSG_HTTPPORT", 2426);
    int udpPort  = ResolvePort(argc, argv, "--udpport",  "LOCALMSG_UDPPORT",  2425);
    (void)udpPort; // reserved for future direct UDP use

    // --list does not require agent name or server check
    if (cmd == "--list") {
        if (!EnsureServer(httpPort)) { PrintError("LocalMsg is not running. Please start ecode first."); return 2; }
        PrintJson(CallApi("GET", "/api/users", "", httpPort));
        LocalFree(argv); return 0;
    }

    // All other commands need agent name
    std::string agent = ResolveAgent(argc, argv);
    if (agent.empty() && cmd != "--list") {
        PrintError("missing --agent <name>. Pass --agent <name> or create .agent-identity in CWD.");
        LocalFree(argv); return 2;
    }

    // Validate --send arguments BEFORE server check (no network needed for validation)
    if (cmd == "--send") {
        std::string to = GetFlag(argc, argv, "--to");
        if (to.empty()) { PrintError("--send requires --to <peer>"); LocalFree(argv); return 2; }
    }

    // Check server (except --list already handled above)
    if (!EnsureServer(httpPort)) {
        PrintError("LocalMsg is not running. Please start ecode first.");
        LocalFree(argv); return 2;
    }

    std::string result;
    int exitCode = 0;

    if (cmd == "--login") {
        result = CallApi("POST", "/api/login",
                         "{\"username\":\"" + EscapeJson(agent) + "\"}", httpPort);
        if (result.find("\"error\"") != std::string::npos) exitCode = 2;

    } else if (cmd == "--logout") {
        result = CallApi("POST", "/api/logout",
                         "{\"username\":\"" + EscapeJson(agent) + "\"}", httpPort);
        if (result.find("\"error\"") != std::string::npos) exitCode = 2;

    } else if (cmd == "--send") {
        std::string to = GetFlag(argc, argv, "--to");
        if (to.empty()) { PrintError("--send requires --to <peer>"); LocalFree(argv); return 2; }

        std::vector<std::string> files = GetFiles(argc, argv);
        if (!files.empty()) {
            // File transfer via LocalSend HTTPS
            int failed = 0;
            for (auto& fp : files) {
                if (!SendFileLocalSend(agent, to, fp)) {
                    PrintError("File send failed: " + fp);
                    ++failed;
                }
            }
            result = failed == 0
                ? "{\"ok\":true,\"files\":" + std::to_string((int)files.size()) + "}"
                : "{\"ok\":false,\"failed\":" + std::to_string(failed) + "}";
        } else {
            // Text message — last non-flag argument is the text
            std::string text;
            for (int i = 2; i < argc; ++i) {
                std::string a = ws2s(argv[i]);
                if (a == "--agent" || a == "--to" || a == "--httpport" || a == "--udpport") { ++i; continue; }
                if (!a.empty() && a[0] != '-') { text = a; break; }
            }
            if (text.empty()) { PrintError("--send requires text or --file <path>"); LocalFree(argv); return 2; }
            result = CallApi("POST", "/api/send",
                             "{\"from\":\"" + EscapeJson(agent) +
                             "\",\"to\":\""   + EscapeJson(to)    +
                             "\",\"text\":\"" + EscapeJson(text)  + "\"}", httpPort);
        }

    } else if (cmd == "--receive") {
        bool peek = HasFlag(argc, argv, "--peek");
        std::string path = "/api/messages?user=" + agent;
        if (peek) path += "&peek";
        result = CallApi("GET", path, "", httpPort);
        if (result.find("\"empty\":true") != std::string::npos ||
            result == "[]" || result.empty()) exitCode = 1;

    } else if (cmd == "--wait") {
        std::string timeoutStr = GetFlag(argc, argv, "--timeout");
        int timeoutSec = timeoutStr.empty() ? 30 : atoi(timeoutStr.c_str());
        if (timeoutSec < 1)   timeoutSec = 1;
        if (timeoutSec > 120) timeoutSec = 120;
        std::string path = "/api/wait?user=" + agent
                         + "&timeout=" + std::to_string(timeoutSec);
        int httpTimeoutMs = (timeoutSec + 10) * 1000;
        result = CallApi("GET", path, "", httpPort, httpTimeoutMs);
        if (result.find("\"empty\":true") != std::string::npos) exitCode = 1;

    } else if (cmd == "--files") {
        bool peek = HasFlag(argc, argv, "--peek");
        std::string path = "/api/files?user=" + agent;
        if (peek) path += "&peek";
        result = CallApi("GET", path, "", httpPort);

    } else {
        PrintUsage();
        LocalFree(argv); return 2;
    }

    PrintJson(result);
    LocalFree(argv);
    return exitCode;
}
