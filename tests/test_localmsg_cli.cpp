#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>
#include <iostream>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " << msg << std::endl; exit(1); }

// Stubs for CLI functions under test
static std::string g_lastError;

static void PrintError(const std::string& msg) { g_lastError = msg; }
static void PrintJson(const std::string&) {}

// ---------------------------------------------------------------------------
// EscapeJson (from cli/main.cpp)
// ---------------------------------------------------------------------------
static std::string EscapeJson(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c < 0x20) { char buf[8]; snprintf(buf,8,"\\u%04x",c); r += buf; }
        else                r += (char)c;
    }
    return r;
}

void TestEscapeJson() {
    VERIFY(EscapeJson("hello") == "hello", "plain text");
    VERIFY(EscapeJson("hello \"world\"") == "hello \\\"world\\\"", "double quotes");
    VERIFY(EscapeJson("back\\slash") == "back\\\\slash", "backslash");
    VERIFY(EscapeJson("line1\nline2") == "line1\\nline2", "newline");
    VERIFY(EscapeJson("line1\rline2") == "line1\\rline2", "carriage return");
    VERIFY(EscapeJson("col1\tcol2") == "col1\\tcol2", "tab");
    VERIFY(EscapeJson("a\"b\\c\nd\re\tf") == "a\\\"b\\\\c\\nd\\re\\tf", "mixed special chars");
    VERIFY(EscapeJson("") == "", "empty string");
    VERIFY(EscapeJson("unicode \x01\x02") == "unicode \\u0001\\u0002", "control chars");
    VERIFY(EscapeJson("normal text 123!@#") == "normal text 123!@#", "normal text unchanged");
    std::cout << "  EscapeJson: OK\n";
}

// ---------------------------------------------------------------------------
// ws2s / s2ws (from cli/main.cpp)
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

void TestStringConv() {
    VERIFY(s2ws("hello") == L"hello", "s2ws basic");
    VERIFY(ws2s(L"world") == "world", "ws2s basic");
    VERIFY(s2ws("") == L"", "s2ws empty");
    VERIFY(ws2s(L"") == "", "ws2s empty");
    VERIFY(s2ws("a b c") == L"a b c", "s2ws with spaces");
    VERIFY(ws2s(L"a b c") == "a b c", "ws2s with spaces");
    VERIFY(ws2s(s2ws("roundtrip")) == "roundtrip", "s2ws then ws2s roundtrip");
    VERIFY(s2ws(ws2s(L"roundtrip2")) == L"roundtrip2", "ws2s then s2ws roundtrip");
    // UTF-8 multi-byte characters
    std::string utf8 = "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88"; // テスト
    std::wstring wide = L"\u30C6\u30B9\u30C8";
    VERIFY(s2ws(utf8) == wide, "s2ws utf8 japanese");
    VERIFY(ws2s(wide) == utf8, "ws2s japanese");
    std::cout << "  StringConv: OK\n";
}

// ---------------------------------------------------------------------------
// ResolvePort (from cli/main.cpp) — simplified test
// ---------------------------------------------------------------------------
static int ResolvePort(int argc, const char* argv[],
                       const std::string& flag,
                       const char* envVar,
                       int defaultPort) {
    for (int i = 1; i < argc - 1; ++i)
        if (argv[i] == flag) {
            int p = atoi(argv[i+1]);
            if (p > 0 && p < 65536) return p;
        }
    const char* env = getenv(envVar);
    if (env) {
        int p = atoi(env);
        if (p > 0 && p < 65536) return p;
    }
    return defaultPort;
}

void TestResolvePort() {
    const char* args1[] = {"prog", "--httpport", "3000"};
    VERIFY(ResolvePort(3, args1, "--httpport", "LOCALMSG_HTTPPORT", 2426) == 3000,
           "CLI flag overrides default");

    const char* args2[] = {"prog", "--list"};
    VERIFY(ResolvePort(2, args2, "--httpport", "LOCALMSG_HTTPPORT", 2426) == 2426,
           "no flag returns default");

    const char* args3[] = {"prog", "--httpport", "99999"}; // invalid > 65535
    VERIFY(ResolvePort(3, args3, "--httpport", "LOCALMSG_HTTPPORT", 2426) == 2426,
           "invalid port returns default");

    const char* args4[] = {"prog", "--udpport", "3001"};
    VERIFY(ResolvePort(3, args4, "--udpport", "LOCALMSG_UDPPORT", 2425) == 3001,
           "udp port CLI flag");
    std::cout << "  ResolvePort: OK\n";
}

// ---------------------------------------------------------------------------
// HTTP response parsing (inline in CallApi)
// ---------------------------------------------------------------------------
struct TestHttpResp {
    int statusCode;
    std::string body;

    static TestHttpResp Parse(const std::string& resp) {
        TestHttpResp r = {200, ""};
        size_t hdrEnd = resp.find("\r\n\r\n");
        if (hdrEnd == std::string::npos) { r.statusCode = 0; r.body = resp; return r; }
        std::string statusLine = resp.substr(0, resp.find("\r\n"));
        size_t sp1 = statusLine.find(' ');
        size_t sp2 = statusLine.find(' ', sp1 + 1);
        if (sp1 != std::string::npos && sp2 != std::string::npos)
            r.statusCode = atoi(statusLine.substr(sp1 + 1, sp2 - sp1 - 1).c_str());
        r.body = resp.substr(hdrEnd + 4);
        return r;
    }
};

void TestHttpResponse() {
    {
        auto r = TestHttpResp::Parse(
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 15\r\n\r\n{\"ok\":true}"
        );
        VERIFY(r.statusCode == 200, "status 200");
        VERIFY(r.body == "{\"ok\":true}", "body with headers");
    }
    {
        auto r = TestHttpResp::Parse(
            "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"
        );
        VERIFY(r.statusCode == 404, "status 404");
        VERIFY(r.body == "", "empty body");
    }
    {
        auto r = TestHttpResp::Parse(
            "HTTP/1.1 200 OK\r\n\r\n{\"users\":[]}"
        );
        VERIFY(r.statusCode == 200, "minimal headers");
        VERIFY(r.body == "{\"users\":[]}", "body after minimal headers");
    }
    {
        auto r = TestHttpResp::Parse(
            "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\n{\"error\":\"crash\"}"
        );
        VERIFY(r.statusCode == 500, "status 500");
        VERIFY(r.body == "{\"error\":\"crash\"}", "error body");
    }
    {
        auto r = TestHttpResp::Parse(
            "no valid http response"
        );
        VERIFY(r.statusCode == 0, "no valid HTTP");
        VERIFY(r.body == "no valid http response", "raw fallthrough");
    }
    {
        // Multiple digit status
        auto r = TestHttpResp::Parse(
            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello"
        );
        VERIFY(r.statusCode == 200, "status 200 ok");
        VERIFY(r.body == "Hello", "body hello");
    }
    std::cout << "  HttpResponse: OK\n";
}

// ---------------------------------------------------------------------------
// HasFlag / GetFlag
// ---------------------------------------------------------------------------
static bool HasFlag(int argc, const char* argv[], const std::string& flag) {
    for (int i = 1; i < argc; ++i)
        if (argv[i] == flag) return true;
    return false;
}
static std::string GetFlag(int argc, const char* argv[], const std::string& flag) {
    for (int i = 1; i < argc - 1; ++i)
        if (argv[i] == flag) return argv[i+1];
    return {};
}

void TestFlags() {
    const char* args[] = {"prog", "--send", "--agent", "abc", "--to", "xyz", "-V"};
    int argc = 7;
    VERIFY(HasFlag(argc, args, "--send") == true, "has --send");
    VERIFY(HasFlag(argc, args, "--agent") == true, "has --agent");
    VERIFY(HasFlag(argc, args, "--to") == true, "has --to");
    VERIFY(HasFlag(argc, args, "-V") == true, "has -V");
    VERIFY(HasFlag(argc, args, "--wait") == false, "no --wait");
    VERIFY(HasFlag(argc, args, "--list") == false, "no --list");
    VERIFY(GetFlag(argc, args, "--agent") == "abc", "get --agent value");
    VERIFY(GetFlag(argc, args, "--to") == "xyz", "get --to value");
    VERIFY(GetFlag(argc, args, "--httpport") == "", "get missing flag returns empty");
    std::cout << "  Flags: OK\n";
}

int main() {
    std::cout << "localmsg-cli tests:\n";
    TestEscapeJson();
    TestStringConv();
    TestResolvePort();
    TestHttpResponse();
    TestFlags();
    std::cout << "All localmsg-cli tests passed.\n";
    return 0;
}
