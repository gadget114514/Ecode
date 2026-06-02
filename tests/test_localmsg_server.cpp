#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <iostream>
#include <cstdint>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " << msg << std::endl; exit(1); }

// ---------------------------------------------------------------------------
// IPMsg constants (from src/main.cpp)
// ---------------------------------------------------------------------------
#define IPMSG_BR_ENTRY      0x00000001UL
#define IPMSG_BR_EXIT       0x00000002UL
#define IPMSG_ANSENTRY      0x00000003UL
#define IPMSG_SENDMSG       0x00000020UL
#define IPMSG_RECVMSG       0x00000021UL
#define IPMSG_READMSG       0x00000030UL
#define IPMSG_DELMSG        0x00000031UL
#define IPMSG_ANSREADMSG    0x00000032UL
#define GET_MODE(cmd)  (cmd & 0x000000ffUL)
#define GET_OPT(cmd)   (cmd & 0xffffff00UL)
#define IPMSG_SECRETOPT  0x00000200UL
#define IPMSG_CAPUTF8OPT 0x00010000UL
#define IPMSG_SENDCHECKOPT 0x00000100UL
#define IPMSG_READCHECKOPT 0x00001000UL

// ---------------------------------------------------------------------------
// IpMsgPacket (from src/main.cpp)
// ---------------------------------------------------------------------------
struct IpMsgPacket {
    int      version = 0;
    uint32_t packetNo = 0;
    std::wstring senderUser;
    std::wstring senderHost;
    uint32_t command = 0;
    std::wstring destUser;
    std::wstring extra;
};

// ---------------------------------------------------------------------------
// ws2s / s2ws (reused from CLI)
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

// ---------------------------------------------------------------------------
// EscapeJson (from src/main.cpp)
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

// ---------------------------------------------------------------------------
// JsonGet (from src/main.cpp)
// ---------------------------------------------------------------------------
static std::string JsonGet(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t kp = json.find(needle);
    if (kp == std::string::npos) return {};
    size_t col = json.find(':', kp + needle.size());
    if (col == std::string::npos) return {};
    size_t vs = col + 1;
    while (vs < json.size() && (json[vs] == ' ' || json[vs] == '\t')) ++vs;
    if (vs >= json.size()) return {};
    if (json[vs] == '"') {
        size_t ve = std::string::npos;
        for (size_t i = vs + 1; i < json.size(); ++i) {
            if (json[i] == '\\') {
                ++i;
            } else if (json[i] == '"') {
                ve = i;
                break;
            }
        }
        return ve == std::string::npos ? std::string{} : json.substr(vs + 1, ve - vs - 1);
    }

    size_t ve = vs;
    while (ve < json.size() && json[ve] != ',' && json[ve] != '}' && json[ve] != '\n') ++ve;
    std::string v = json.substr(vs, ve - vs);
    while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
    return v;
}

void TestJsonGet() {
    // String value
    std::string j1 = "{\"name\":\"alice\",\"age\":\"30\"}";
    VERIFY(JsonGet(j1, "name") == "alice", "string value");
    VERIFY(JsonGet(j1, "age") == "30", "numeric as string");

    // Missing key
    VERIFY(JsonGet(j1, "nonexistent") == "", "missing key");

    // Nested (raw value, not string-quoted)
    std::string j2 = "{\"active\":true,\"count\":42}";
    VERIFY(JsonGet(j2, "active") == "true", "boolean value");
    VERIFY(JsonGet(j2, "count") == "42", "numeric value");

    // Value with unescaped content
    std::string j3 = "{\"msg\":\"simple text\"}";
    VERIFY(JsonGet(j3, "msg") == "simple text", "unescaped text value");

    // Empty string
    VERIFY(JsonGet("{}", "key") == "", "empty object");
    VERIFY(JsonGet("", "key") == "", "empty string");

    // Multiple keys
    std::string j4 = "{\"a\":\"1\",\"b\":\"2\",\"c\":\"3\"}";
    VERIFY(JsonGet(j4, "a") == "1", "first key");
    VERIFY(JsonGet(j4, "b") == "2", "middle key");
    VERIFY(JsonGet(j4, "c") == "3", "last key");

    // Numeric value (not quoted)
    std::string j5 = "{\"count\":42,\"name\":\"test\"}";
    VERIFY(JsonGet(j5, "count") == "42", "numeric value");
    VERIFY(JsonGet(j5, "name") == "test", "string after numeric");

    // Escaped quotes in value
    std::string j6 = "{\"text\":\"hello \\\"world\\\" test\"}";
    VERIFY(JsonGet(j6, "text") == "hello \\\"world\\\" test", "escaped quotes");
    std::cout << "  JsonGet: OK\n";
}


// ---------------------------------------------------------------------------
// IPMsg Packet parsing & composition (from src/main.cpp)
// ---------------------------------------------------------------------------
static bool ParseIpMsgPacket(const std::string& raw, IpMsgPacket& pkt) {
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i <= raw.size(); ++i) {
        if (i == raw.size() || raw[i] == ':') {
            parts.push_back(cur); cur.clear();
        } else cur += raw[i];
    }
    if (parts.size() < 5) return false;
    pkt.version = atoi(parts[0].c_str());
    pkt.packetNo = (uint32_t)atoll(parts[1].c_str());
    pkt.senderUser = s2ws(parts[2]);
    pkt.senderHost = s2ws(parts[3]);
    pkt.command = (uint32_t)atoll(parts[4].c_str());
    pkt.destUser = (parts.size() > 5) ? s2ws(parts[5]) : L"";
    if (parts.size() > 6) {
        std::string ex = parts[6];
        for (size_t i = 7; i < parts.size(); ++i) ex += ":" + parts[i];
        pkt.extra = s2ws(ex);
    }
    return true;
}

static std::string ComposeIpMsgPacket(const IpMsgPacket& pkt) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%d:%lu:", pkt.version, (unsigned long)pkt.packetNo);
    std::string r = buf;
    r += ws2s(pkt.senderUser) + ":" + ws2s(pkt.senderHost) + ":";
    char cmd[16]; snprintf(cmd, sizeof(cmd), "%lu", (unsigned long)pkt.command);
    r += cmd;
    if (!pkt.destUser.empty()) {
        r += ":" + ws2s(pkt.destUser);
        if (!pkt.extra.empty()) r += ":" + ws2s(pkt.extra);
    } else if (!pkt.extra.empty()) {
        r += ":" + ws2s(pkt.extra);
    }
    return r;
}

static std::string IpMsgDirStr(uint32_t cmd) {
    uint32_t mode = GET_MODE(cmd);
    if (mode == IPMSG_BR_ENTRY)  return "BR_ENTRY";
    if (mode == IPMSG_BR_EXIT)   return "BR_EXIT";
    if (mode == IPMSG_ANSENTRY)  return "ANSENTRY";
    if (mode == IPMSG_SENDMSG)   return "SENDMSG";
    if (mode == IPMSG_RECVMSG)   return "RECVMSG";
    if (mode == IPMSG_READMSG)   return "READMSG";
    if (mode == IPMSG_DELMSG)    return "DELMSG";
    if (mode == IPMSG_ANSREADMSG) return "ANSREADMSG";
    char buf[16]; snprintf(buf,16,"0x%02lx",(unsigned long)mode);
    return buf;
}

void TestIpMsgParse() {
    {
        IpMsgPacket p;
        bool ok = ParseIpMsgPacket("1:42:alice:myhost:16777216:target:hello", p);
        VERIFY(ok, "basic parse");
        VERIFY(p.version == 1, "version");
        VERIFY(p.packetNo == 42, "packetNo");
        VERIFY(ws2s(p.senderUser) == "alice", "senderUser");
        VERIFY(ws2s(p.senderHost) == "myhost", "senderHost");
        VERIFY(p.command == 0x01000000, "command with opt flags");
        VERIFY(ws2s(p.destUser) == "target", "destUser");
        VERIFY(ws2s(p.extra) == "hello", "extra");
    }
    {
        IpMsgPacket p;
        bool ok = ParseIpMsgPacket("1:1:user:host:1", p);
        VERIFY(ok, "minimal 5-part packet");
        VERIFY(p.command == IPMSG_BR_ENTRY, "BR_ENTRY");
        VERIFY(ws2s(p.destUser) == "", "no destUser");
        VERIFY(ws2s(p.extra) == "", "no extra");
    }
    {
        IpMsgPacket p;
        bool ok = ParseIpMsgPacket("1:2:sender:host:2", p);
        VERIFY(ok, "BR_EXIT");
        VERIFY(p.command == IPMSG_BR_EXIT, "command is BR_EXIT");
    }
    {
        IpMsgPacket p;
        bool ok = ParseIpMsgPacket("bad", p);
        VERIFY(!ok, "too few parts");
    }
    {
        // SENDMSG with UTF8 option and extra text containing colons
        IpMsgPacket p;
        uint32_t cmd = IPMSG_SENDMSG | IPMSG_CAPUTF8OPT | IPMSG_SENDCHECKOPT;
        char cmdstr[16]; snprintf(cmdstr, sizeof(cmdstr), "%lu", (unsigned long)cmd);
        std::string raw = std::string("1:3:sender:host:") + cmdstr + ":dest:hello:world";
        bool ok = ParseIpMsgPacket(raw, p);
        VERIFY(ok, "SENDMSG with colons in extra");
        VERIFY(GET_MODE(p.command) == IPMSG_SENDMSG, "mode is SENDMSG");
        VERIFY(ws2s(p.extra) == "hello:world", "extra with colons preserved");
    }
    {
        // Malformed: too few parts (4 parts)
        IpMsgPacket p;
        bool ok = ParseIpMsgPacket("1:2:sender:host", p);
        VERIFY(!ok, "4 parts invalid");
    }
    {
        // Malformed: empty components
        IpMsgPacket p;
        bool ok = ParseIpMsgPacket("::::", p);
        VERIFY(ok, "empty parts parsed");
        VERIFY(p.version == 0, "empty version is 0");
        VERIFY(p.packetNo == 0, "empty packetNo is 0");
        VERIFY(ws2s(p.senderUser) == "", "empty senderUser");
        VERIFY(ws2s(p.senderHost) == "", "empty senderHost");
        VERIFY(p.command == 0, "empty command is 0");
    }
    {
        // Non-numeric command and packetNo
        IpMsgPacket p;
        bool ok = ParseIpMsgPacket("1:abc:sender:host:xyz", p);
        VERIFY(ok, "non-numeric parsed safely");
        VERIFY(p.packetNo == 0, "invalid packetNo falls back to 0");
        VERIFY(p.command == 0, "invalid command falls back to 0");
    }
    std::cout << "  IpMsgParse: OK\n";
}

void TestIpMsgCompose() {
    {
        IpMsgPacket p;
        p.version = 1; p.packetNo = 100;
        p.senderUser = L"alice"; p.senderHost = L"host";
        p.command = IPMSG_BR_ENTRY | IPMSG_CAPUTF8OPT;
        std::string raw = ComposeIpMsgPacket(p);
        VERIFY(raw == "1:100:alice:host:65537", "BR_ENTRY with UTF8 opt");
    }
    {
        IpMsgPacket p;
        p.version = 1; p.packetNo = 200;
        p.senderUser = L"bob"; p.senderHost = L"remote";
        p.command = IPMSG_SENDMSG;
        p.destUser = L"alice";
        p.extra = L"Hello!";
        std::string raw = ComposeIpMsgPacket(p);
        VERIFY(raw == "1:200:bob:remote:32:alice:Hello!", "SENDMSG to alice");
    }
    {
        IpMsgPacket p;
        p.version = 1; p.packetNo = 300;
        p.senderUser = L"user"; p.senderHost = L"host";
        p.command = IPMSG_BR_EXIT;
        std::string raw = ComposeIpMsgPacket(p);
        VERIFY(raw == "1:300:user:host:2", "BR_EXIT minimal");
    }
    // Roundtrip test
    {
        IpMsgPacket orig;
        orig.version = 1; orig.packetNo = 500;
        orig.senderUser = L"test"; orig.senderHost = L"box";
        orig.command = IPMSG_SENDMSG | IPMSG_SECRETOPT;
        orig.destUser = L"target";
        orig.extra = L"secret message: with colons";
        std::string raw = ComposeIpMsgPacket(orig);
        IpMsgPacket parsed;
        VERIFY(ParseIpMsgPacket(raw, parsed), "roundtrip parse");
        VERIFY(parsed.version == 1, "roundtrip version");
        VERIFY(parsed.packetNo == 500, "roundtrip packetNo");
        VERIFY(parsed.command == orig.command, "roundtrip command");
        VERIFY(ws2s(parsed.destUser) == "target", "roundtrip destUser");
        VERIFY(ws2s(parsed.extra) == "secret message: with colons", "roundtrip extra");
    }
    std::cout << "  IpMsgCompose: OK\n";
}

void TestIpMsgDirStr() {
    VERIFY(IpMsgDirStr(IPMSG_BR_ENTRY) == "BR_ENTRY", "BR_ENTRY");
    VERIFY(IpMsgDirStr(IPMSG_BR_EXIT) == "BR_EXIT", "BR_EXIT");
    VERIFY(IpMsgDirStr(IPMSG_ANSENTRY) == "ANSENTRY", "ANSENTRY");
    VERIFY(IpMsgDirStr(IPMSG_SENDMSG) == "SENDMSG", "SENDMSG");
    VERIFY(IpMsgDirStr(IPMSG_RECVMSG) == "RECVMSG", "RECVMSG");
    VERIFY(IpMsgDirStr(IPMSG_READMSG) == "READMSG", "READMSG");
    VERIFY(IpMsgDirStr(IPMSG_DELMSG) == "DELMSG", "DELMSG");
    VERIFY(IpMsgDirStr(IPMSG_ANSREADMSG) == "ANSREADMSG", "ANSREADMSG");
    // With option flags
    uint32_t withOpt = IPMSG_SENDMSG | IPMSG_SECRETOPT | IPMSG_CAPUTF8OPT;
    VERIFY(IpMsgDirStr(withOpt) == "SENDMSG", "SENDMSG with options");
    std::cout << "  IpMsgDirStr: OK\n";
}

void TestEscapeJson() {
    VERIFY(EscapeJson("hello") == "hello", "plain");
    VERIFY(EscapeJson("a\"b") == "a\\\"b", "quote");
    VERIFY(EscapeJson("a\\b") == "a\\\\b", "backslash");
    VERIFY(EscapeJson("\n") == "\\n", "newline");
    std::cout << "  EscapeJson: OK\n";
}

int main() {
    std::cout << "localmsg server tests:\n";
    TestEscapeJson();
    TestJsonGet();
    TestIpMsgParse();
    TestIpMsgCompose();
    TestIpMsgDirStr();
    std::cout << "All localmsg server tests passed.\n";
    return 0;
}
