// =============================================================================
// LocalMsg — LAN messenger: LocalSend (HTTPS) + IPMsg (UDP) + REST API
// Pure Win32, Winsock2, mbedTLS, WinHTTP
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#pragma warning(disable: 4996)

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <iphlpapi.h>
#include <winhttp.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "winhttp.lib")

#include <string>
#include <vector>
#include <map>
#include <set>
#include <tuple>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <richedit.h>

#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/error.h>
#include <mbedtls/sha256.h>
#pragma comment(lib, "mbedtls.lib")
#pragma comment(lib, "mbedx509.lib")
#pragma comment(lib, "mbedcrypto.lib")

template<class T> static T ls_min(T a, T b) { return a < b ? a : b; }
template<class T> static T ls_max(T a, T b) { return a > b ? a : b; }

static std::string ws2s(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    while (!s.empty() && s.back() == 0) s.pop_back();
    return s;
}
static std::wstring s2ws(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    while (!w.empty() && w.back() == 0) w.pop_back();
    return w;
}

// ---------------------------------------------------------------------------
// [1] LocalSend Constants & IDs
// ---------------------------------------------------------------------------
#define IDC_PEER_LIST       101
#define IDC_STATUS          103
#define IDC_RELOAD_BTN      105
#define IDC_CHAT_VIEW       106
#define IDC_TEXT_INPUT      107
#define IDC_SEND_TEXT_BTN   108
#define IDC_ATTACH_BTN      112
#define IDC_XFER_LIST       109
#define IDC_PEER_NAME_LABEL 111
#define IDC_COMM_LOG        113
#define IDC_DEBUG_LOG       114

#define IDM_SEND_FILES  201
#define IDM_CLEAR_CHAT  202

#define WM_LS_PEER_FOUND    (WM_USER + 1)
#define WM_LS_PEER_GONE     (WM_USER + 2)
#define WM_LS_XFER_PROGRESS (WM_USER + 3)
#define WM_LS_XFER_DONE     (WM_USER + 4)
#define WM_LS_INBOUND_REQ   (WM_USER + 5)
#define WM_LS_STATUS_TEXT   (WM_USER + 6)
#define WM_LS_APPEND_CHAT   (WM_USER + 8)
#define WM_LS_TEXT_RECEIVED (WM_USER + 9)
#define WM_LS_APPEND_COMM   (WM_USER + 10)
#define WM_LS_APPEND_DEBUG      (WM_USER + 11)
#define WM_IPMSG_TEXT_RCVD      (WM_USER + 13)
#define WM_IPMSG_UPDATE_PEERS   (WM_USER + 14)

#define LS_PORT         53317
#define LS_MCAST_ADDR   "224.0.0.167"
#define LS_MAX_CONNS    8
#define LS_CHUNK        (64 * 1024)

// ---------------------------------------------------------------------------
// [2] IPMsg Protocol Constants
// ---------------------------------------------------------------------------
#define IPMSG_PORT          2425
#define GET_MODE(cmd)  (cmd & 0x000000ffUL)
#define GET_OPT(cmd)   (cmd & 0xffffff00UL)

#define IPMSG_BR_ENTRY      0x00000001UL
#define IPMSG_BR_EXIT       0x00000002UL
#define IPMSG_ANSENTRY      0x00000003UL
#define IPMSG_BR_ABSENCE    0x00000004UL
#define IPMSG_SENDMSG       0x00000020UL
#define IPMSG_RECVMSG       0x00000021UL
#define IPMSG_READMSG       0x00000030UL
#define IPMSG_DELMSG        0x00000031UL
#define IPMSG_ANSREADMSG    0x00000032UL
#define IPMSG_GETINFO       0x00000040UL
#define IPMSG_SENDINFO      0x00000041UL
#define IPMSG_GETFILEDATA   0x00000060UL
#define IPMSG_RELEASEFILES  0x00000061UL
#define IPMSG_GETDIRFILES   0x00000062UL

#define IPMSG_SENDCHECKOPT  0x00000100UL
#define IPMSG_SECRETOPT     0x00000200UL
#define IPMSG_BROADCASTOPT  0x00000400UL
#define IPMSG_MULTICASTOPT  0x00000800UL
#define IPMSG_AUTORETOPT    0x00002000UL
#define IPMSG_RETRYOPT      0x00004000UL
#define IPMSG_FILEATTACHOPT 0x00200000UL
#define IPMSG_ENCRYPTOPT    0x00400000UL
#define IPMSG_UTF8OPT       0x00800000UL
#define IPMSG_READCHECKOPT  0x00100000UL
#define IPMSG_CAPUTF8OPT    0x01000000UL

#define IPMSG_FILE_REGULAR  0x00000001UL
#define IPMSG_FILE_DIR      0x00000002UL
#define IPMSG_FILE_RETPARENT 0x00000003UL
#define FILELIST_SEPARATOR  '\a'

// ---------------------------------------------------------------------------
// [3] Data Structures
// ---------------------------------------------------------------------------
enum class Proto { LocalSend, IPMsg };

struct PeerInfo {
    std::wstring alias;
    std::wstring hostname;
    std::wstring deviceType;
    std::wstring fingerprint;
    std::wstring ip;
    int          port       = LS_PORT;
    DWORD        lastSeenMs = 0;
    Proto        protocol   = Proto::LocalSend;
};

enum class XferDir   { Inbound, Outbound };
enum class XferState { Pending, Active, Done, Failed, Cancelled };

struct TransferEntry {
    std::wstring id;
    std::wstring filename;
    std::wstring peerAlias;
    std::wstring localPath;
    int64_t      totalBytes = 0;
    int64_t      doneBytes  = 0;
    XferDir      dir        = XferDir::Outbound;
    XferState    state      = XferState::Pending;
    bool         isAgentTransfer  = false;  // auto-accepted for pseudo-user
    std::wstring destinationAgent;          // which pseudo-user's inbox to store in
};

struct FileDesc {
    std::wstring id;
    std::wstring filename;
    int64_t      size  = 0;
    std::string  token;
};

struct InboundReq {
    std::wstring          senderAlias;
    std::vector<FileDesc> files;
    bool                  textOnly    = false;
    std::wstring          textContent;
    HANDLE                hEvent      = nullptr;
    bool                  accepted    = false;
    std::string           responseBody;
};

struct SendParams {
    std::wstring peerIp;
    int          peerPort   = LS_PORT;
    std::wstring localPath;
    std::wstring filename;
    int          xferIdx    = -1;
    HWND         notifyHwnd = nullptr;
};

struct TextSendParams {
    std::wstring peerIp;
    int          peerPort = LS_PORT;
    std::wstring text;
    std::wstring senderAlias;
    HWND         notifyHwnd = nullptr;
};

struct PseudoUser {
    std::wstring username;
    std::wstring hostname;
    bool active = true;
};

struct Message {
    int id = 0;
    std::wstring from;
    std::wstring to;
    std::wstring text;
    std::wstring time;
};

struct IpMsgPacket {
    int      version = 0;
    DWORD    packetNo = 0;
    std::wstring senderUser;
    std::wstring senderHost;
    DWORD    command = 0;
    std::wstring destUser;
    std::wstring extra;
};

// ---------------------------------------------------------------------------
// [4] Globals
// ---------------------------------------------------------------------------
static HWND g_hwnd         = nullptr;
static HWND g_peerList     = nullptr;
static HWND g_statusBar    = nullptr;
static HWND g_reloadBtn      = nullptr;
static HWND g_peerNameLabel  = nullptr;
static HWND g_chatView       = nullptr;
static HWND g_textInput      = nullptr;
static HWND g_sendTextBtn    = nullptr;
static HWND g_attachBtn      = nullptr;
static std::vector<std::wstring> g_pendingFiles;
static HWND g_xferList       = nullptr;
static HWND g_commLog        = nullptr;
static HWND g_debugLog       = nullptr;

static int  g_dividerX     = 330;
static bool g_dragging     = false;
static int  g_split1Y      = -1;
static int  g_split2Y      = -1;
static int  g_draggingSplit = 0;
static int  g_listenPort   = LS_PORT;

static CRITICAL_SECTION g_peerCs;
static std::vector<PeerInfo> g_peers;

static CRITICAL_SECTION g_xferCs;
static std::vector<TransferEntry> g_transfers;
static std::map<std::string, int> g_tokenToIdx;

static volatile bool g_stopThreads  = false;
static volatile bool g_announceNow  = false;
static HANDLE g_discThread          = nullptr;
static HANDLE g_acceptThread        = nullptr;
static HANDLE g_acceptSem           = nullptr;
static SOCKET g_listenSock          = INVALID_SOCKET;
static SOCKET g_udpSock             = INVALID_SOCKET;

static char   g_fingerprint[64]     = {};
static wchar_t g_localIp[64]        = {};
static wchar_t g_localAlias[256]    = {};
static wchar_t g_myHostname[256]    = {};

// mbedTLS
static mbedtls_x509_crt      g_tlsCert   = {};
static mbedtls_pk_context     g_tlsKey    = {};
static mbedtls_ssl_config     g_tlsCfg    = {};
static mbedtls_entropy_context g_entropy  = {};
static mbedtls_ctr_drbg_context g_drbg   = {};
static bool g_tlsReady = false;

// IPMsg globals
static CRITICAL_SECTION g_pseudoCs;
static std::vector<PseudoUser> g_pseudoUsers;
// Per-peer last received SENDMSG packet number (keyed by IP string).
// Used to drop duplicate deliveries; a wrap/reset is detected by a large backward jump.
static std::map<std::wstring, DWORD> g_ipmsgLastPktNo;
static std::set<uint64_t> g_ipmsgSeenMsgs;
static std::map<std::wstring, std::pair<int, DWORD>> g_ipmsgRateLimit; // senderIp -> (count, windowStart)
static std::map<std::wstring, DWORD> g_ipmsgBatchTime; // senderIp -> lastMsgTime (for batch detection)
static CRITICAL_SECTION g_msgCs;
static std::vector<Message> g_messages;
static int g_nextMsgId = 1;
static std::map<std::wstring, int> g_msgReadPtr; // per-user read pointer (last returned msg id)
static HANDLE g_ipmsgThread  = nullptr;
static SOCKET g_ipmsgSock    = INVALID_SOCKET;
static int  g_restPort = 2426;
static HANDLE g_restThread   = nullptr;
static SOCKET g_restSock     = INVALID_SOCKET;

// Forward declarations
static bool IsPseudoUser(const std::wstring& username);
static std::wstring GetPrimaryUser();

// File inbox for pseudo-user auto-accept transfers (protected by g_xferCs)
struct ReceivedFile {
    std::wstring filename;
    std::wstring localPath;
    std::wstring from;
    std::wstring timestamp;
};
static std::map<std::wstring, std::vector<ReceivedFile>> g_fileInbox;

// ---------------------------------------------------------------------------
// [5] JSON helpers
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
        size_t ve = json.find('"', vs + 1);
        return ve == std::string::npos ? std::string{} : json.substr(vs + 1, ve - vs - 1);
    }
    size_t ve = vs;
    while (ve < json.size() && json[ve] != ',' && json[ve] != '}' && json[ve] != '\n') ++ve;
    std::string v = json.substr(vs, ve - vs);
    while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
    return v;
}

static std::string JsonBuild(std::vector<std::pair<std::string,std::string>> kv) {
    std::string r = "{";
    for (size_t i = 0; i < kv.size(); ++i) {
        if (i) r += ",";
        r += "\"" + kv[i].first + "\":\"" + kv[i].second + "\"";
    }
    return r + "}";
}

static std::string EscapeJson(const std::string& s) {
    std::string r;
    for (auto c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if ((unsigned char)c < 0x20) { char buf[8]; snprintf(buf,8,"\\u%04x",c); r += buf; }
        else r += c;
    }
    return r;
}

static std::string MakeId() {
    static int counter = 0;
    char buf[32]; snprintf(buf, sizeof(buf), "%08X%04X", (unsigned)GetTickCount(), counter++ & 0xFFFF);
    return buf;
}

static std::wstring TimeStamp() {
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t buf[32]; swprintf(buf, 32, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

// ---------------------------------------------------------------------------
// [6] Append helpers
// ---------------------------------------------------------------------------
static void AppendChat(const std::wstring& line) {
    if (!g_chatView) return;
    int len = GetWindowTextLengthW(g_chatView);
    SendMessageW(g_chatView, EM_SETSEL, len, len);
    std::wstring l = line + L"\r\n";
    SendMessageW(g_chatView, EM_REPLACESEL, FALSE, (LPARAM)l.c_str());
    SendMessageW(g_chatView, WM_VSCROLL, SB_BOTTOM, 0);
}

static void AppendToEdit(HWND hEdit, const std::wstring& line) {
    if (!hEdit) return;
    int len = GetWindowTextLengthW(hEdit);
    SendMessageW(hEdit, EM_SETSEL, len, len);
    COLORREF color = GetSysColor(COLOR_WINDOWTEXT);
    if      (line.find(L">>>") != std::wstring::npos) color = RGB(0, 100, 200);
    else if (line.find(L"<<<") != std::wstring::npos) color = RGB(0, 140, 0);
    CHARFORMAT2W cf = { sizeof(cf) };
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    std::wstring l = TimeStamp() + L" " + line + L"\r\n";
    SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)l.c_str());
    SendMessageW(hEdit, WM_VSCROLL, SB_BOTTOM, 0);
}

static void DebugLog(const std::wstring& line) {
    std::wstring tag;
    if (line.find(L"IPMSG") != std::wstring::npos || line.find(L"IPMsg") != std::wstring::npos || line.find(L"Pseudo") != std::wstring::npos)
        tag = L"[ipmsg]";
    else if (line.find(L"REST") != std::wstring::npos || line.find(L"TLS") != std::wstring::npos)
        tag = L"[other]";
    else
        tag = L"[ls]";
    PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0, (LPARAM)new std::wstring(tag + L" " + line));
}

static void AppendCommLog(const std::wstring& line) {
    AppendToEdit(g_commLog, line);
}

// ---------------------------------------------------------------------------
// [7] LocalSend — TLS
// ---------------------------------------------------------------------------
static void TlsLogError(const char* ctx, int ret) {
    char buf[256]; mbedtls_strerror(ret, buf, sizeof(buf));
    std::wstring msg = s2ws(std::string(ctx) + ": " + buf);
    PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0, (LPARAM)new std::wstring(L"TLS ERR " + msg));
}

static bool InitTls() {
    mbedtls_x509_crt_init(&g_tlsCert);
    mbedtls_pk_init(&g_tlsKey);
    mbedtls_ssl_config_init(&g_tlsCfg);
    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_drbg);

    const char* pers = "localmsg";
    int ret = mbedtls_ctr_drbg_seed(&g_drbg, mbedtls_entropy_func, &g_entropy,
                                     (const unsigned char*)pers, strlen(pers));
    if (ret) { TlsLogError("drbg_seed", ret); return false; }

    ret = mbedtls_pk_setup(&g_tlsKey, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret) { TlsLogError("pk_setup", ret); return false; }
    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(g_tlsKey), mbedtls_ctr_drbg_random,
                               &g_drbg, 2048, 65537);
    if (ret) { TlsLogError("rsa_gen_key", ret); return false; }

    mbedtls_x509write_cert crt;
    mbedtls_x509write_crt_init(&crt);
    mbedtls_mpi serial; mbedtls_mpi_init(&serial);
    mbedtls_mpi_read_string(&serial, 10, "1");

    mbedtls_x509write_crt_set_subject_key(&crt, &g_tlsKey);
    mbedtls_x509write_crt_set_issuer_key(&crt, &g_tlsKey);

    char hostname[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD sz = sizeof(hostname); GetComputerNameA(hostname, &sz);
    char dn[256]; snprintf(dn, sizeof(dn), "CN=LocalMsg-%s", hostname);
    mbedtls_x509write_crt_set_subject_name(&crt, dn);
    mbedtls_x509write_crt_set_issuer_name(&crt, dn);
    mbedtls_x509write_crt_set_serial(&crt, &serial);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_validity(&crt, "20240101000000", "20340101000000");

    unsigned char certDer[4096];
    ret = mbedtls_x509write_crt_der(&crt, certDer, sizeof(certDer),
                                     mbedtls_ctr_drbg_random, &g_drbg);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_mpi_free(&serial);
    if (ret < 0) { TlsLogError("crt_der", ret); return false; }

    ret = mbedtls_x509_crt_parse_der(&g_tlsCert, certDer + sizeof(certDer) - ret, ret);
    if (ret) { TlsLogError("crt_parse", ret); return false; }

    unsigned char sha[32];
    mbedtls_sha256(g_tlsCert.raw.p, g_tlsCert.raw.len, sha, 0);
    char fp[65] = {};
    for (int i = 0; i < 32; ++i) snprintf(fp + i*2, 3, "%02x", sha[i]);
    snprintf(g_fingerprint, sizeof(g_fingerprint), "%s", fp);

    ret = mbedtls_ssl_config_defaults(&g_tlsCfg,
        MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret) { TlsLogError("ssl_config", ret); return false; }
    mbedtls_ssl_conf_rng(&g_tlsCfg, mbedtls_ctr_drbg_random, &g_drbg);
    mbedtls_ssl_conf_own_cert(&g_tlsCfg, &g_tlsCert, &g_tlsKey);
    mbedtls_ssl_conf_authmode(&g_tlsCfg, MBEDTLS_SSL_VERIFY_NONE);

    g_tlsReady = true;
    PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0,
        (LPARAM)new std::wstring(L"TLS ready — fingerprint: " + s2ws(std::string(fp).substr(0, 16)) + L"..."));
    return true;
}

static void FreeTls() {
    mbedtls_x509_crt_free(&g_tlsCert);
    mbedtls_pk_free(&g_tlsKey);
    mbedtls_ssl_config_free(&g_tlsCfg);
    mbedtls_entropy_free(&g_entropy);
    mbedtls_ctr_drbg_free(&g_drbg);
}

static int TlsSend(void* ctx, const unsigned char* buf, size_t len) {
    SOCKET s = *(SOCKET*)ctx;
    int r = send(s, (const char*)buf, (int)len, 0);
    return r == SOCKET_ERROR ? MBEDTLS_ERR_NET_SEND_FAILED : r;
}
static int TlsRecv(void* ctx, unsigned char* buf, size_t len) {
    SOCKET s = *(SOCKET*)ctx;
    int r = recv(s, (char*)buf, (int)len, 0);
    if (r == 0)            return MBEDTLS_ERR_NET_CONN_RESET;
    if (r == SOCKET_ERROR) return MBEDTLS_ERR_NET_RECV_FAILED;
    return r;
}

// ---------------------------------------------------------------------------
// [8] LocalSend — BuildAnnounce
// ---------------------------------------------------------------------------
static std::string BuildAnnounce(int port) {
    return JsonBuild({
        {"alias",       ws2s(g_localAlias)},
        {"version",     "2.0"},
        {"deviceModel", "PC"},
        {"deviceType",  "desktop"},
        {"fingerprint", g_fingerprint},
        {"port",        std::to_string(port)},
        {"protocol",    "https"},
        {"download",    "false"}
    });
}

// ---------------------------------------------------------------------------
// [9] LocalSend — UDP Discovery thread
// ---------------------------------------------------------------------------
static DWORD WINAPI DiscoveryThread(LPVOID) {
    g_udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_udpSock == INVALID_SOCKET) return 1;

    BOOL reuse = TRUE;
    setsockopt(g_udpSock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    sockaddr_in local{}; local.sin_family = AF_INET; local.sin_port = htons(LS_PORT);
    local.sin_addr.s_addr = INADDR_ANY;
    bind(g_udpSock, (sockaddr*)&local, sizeof(local));

    ip_mreq mreq{};
    inet_pton(AF_INET, LS_MCAST_ADDR, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(g_udpSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));

    sockaddr_in dest{}; dest.sin_family = AF_INET; dest.sin_port = htons(LS_PORT);
    inet_pton(AF_INET, LS_MCAST_ADDR, &dest.sin_addr);

    for (int i = 0; i < 30 && g_listenPort == LS_PORT && !g_stopThreads; ++i) Sleep(100);
    for (int i = 0; i < 50 && !g_tlsReady && !g_stopThreads; ++i) Sleep(100);

    auto SendAnnounce = [&](const sockaddr_in* target) {
        std::string ann = BuildAnnounce(g_listenPort);
        DebugLog(L"LS >>> multicast (" + s2ws(target ? "unicast" : "multicast") + L"): " + s2ws(ann));
        sendto(g_udpSock, ann.c_str(), (int)ann.size(), 0,
               target ? (const sockaddr*)target : (const sockaddr*)&dest,
               sizeof(sockaddr_in));
    };

    SendAnnounce(nullptr);
    DWORD lastAnnounce = GetTickCount();

    while (!g_stopThreads) {
        fd_set fds; FD_ZERO(&fds); FD_SET(g_udpSock, &fds);
        timeval tv{ 0, 100000 };
        if (select(0, &fds, nullptr, nullptr, &tv) > 0) {
            char buf[4096] = {};
            sockaddr_in from{}; int fromLen = sizeof(from);
            int n = recvfrom(g_udpSock, buf, sizeof(buf)-1, 0, (sockaddr*)&from, &fromLen);
            if (n > 0) {
                std::string json(buf, n);
                char ipbuf[64] = {}; inet_ntop(AF_INET, &from.sin_addr, ipbuf, sizeof(ipbuf));
                DebugLog(L"LS <<< recv from " + s2ws(ipbuf) + L": " + s2ws(json));
                std::string fp = JsonGet(json, "fingerprint");
                if (!fp.empty() && fp != g_fingerprint) {
                    auto* p        = new PeerInfo;
                    p->alias       = s2ws(JsonGet(json, "alias"));
                    p->deviceType  = s2ws(JsonGet(json, "deviceType"));
                    p->fingerprint = s2ws(fp);
                    p->lastSeenMs  = GetTickCount();
                    p->ip = s2ws(std::string(ipbuf));
                    std::string ps = JsonGet(json, "port");
                    p->port = ps.empty() ? LS_PORT : atoi(ps.c_str());
                    p->protocol = Proto::LocalSend;
                    PostMessage(g_hwnd, WM_LS_PEER_FOUND, 0, (LPARAM)p);

                    sockaddr_in reply = from;
                    SendAnnounce(&reply);
                }
            }
        }
        bool force = g_announceNow;
        if (force) g_announceNow = false;
        if (force || GetTickCount() - lastAnnounce > 600000) {
            SendAnnounce(nullptr);
            lastAnnounce = GetTickCount();
        }
    }
    closesocket(g_udpSock); g_udpSock = INVALID_SOCKET;
    return 0;
}

// ---------------------------------------------------------------------------
// [10] LocalSend — HTTP helpers
// ---------------------------------------------------------------------------
struct HttpReq {
    std::string method, path;
    std::map<std::string, std::string> headers;
    int64_t              contentLength = 0;
    SOCKET               sock          = INVALID_SOCKET;
    mbedtls_ssl_context* ssl           = nullptr;
};

static int NetRecv(HttpReq& req, char* buf, int len) {
    if (req.ssl) {
        int r;
        do { r = mbedtls_ssl_read(req.ssl, (unsigned char*)buf, len); }
        while (r == MBEDTLS_ERR_SSL_WANT_READ);
        return r > 0 ? r : -1;
    }
    return recv(req.sock, buf, len, 0);
}

static int NetSend(HttpReq& req, const char* buf, int len) {
    if (req.ssl) {
        int sent = 0;
        while (sent < len) {
            int r = mbedtls_ssl_write(req.ssl, (const unsigned char*)buf + sent, len - sent);
            if (r <= 0) return -1;
            sent += r;
        }
        return sent;
    }
    return send(req.sock, buf, len, 0);
}

static bool RecvLine(HttpReq& req, std::string& line) {
    line.clear(); char c;
    while (true) {
        if (NetRecv(req, &c, 1) <= 0) return false;
        if (c == '\n') break;
        if (c != '\r') line += c;
    }
    return true;
}

static bool ParseHttpReq(HttpReq& req) {
    std::string line;
    if (!RecvLine(req, line)) return false;
    size_t s1 = line.find(' '); if (s1 == std::string::npos) return false;
    req.method = line.substr(0, s1);
    size_t s2  = line.find(' ', s1+1);
    req.path   = s2 == std::string::npos ? line.substr(s1+1) : line.substr(s1+1, s2-s1-1);
    std::wstring debugLine = L"HTTP <<< " + s2ws(line);
    while (RecvLine(req, line) && !line.empty()) {
        size_t col = line.find(':'); if (col == std::string::npos) continue;
        std::string k = line.substr(0, col), v = line.substr(col+1);
        while (!v.empty() && v.front() == ' ') v.erase(v.begin());
        for (auto& ch : k) ch = (char)tolower((unsigned char)ch);
        req.headers[k] = v;
        debugLine += L"\nHTTP <<< " + s2ws(line);
    }
    debugLine += L"\nHTTP <<< ";
    PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0, (LPARAM)new std::wstring(debugLine));
    auto it = req.headers.find("content-length");
    if (it != req.headers.end()) req.contentLength = atoll(it->second.c_str());
    return true;
}

static void SendResponse(HttpReq& req, int code, const std::string& body,
                         const char* ct = "application/json") {
    const char* ph = (code==200)?"OK":(code==403)?"Forbidden":"Error";
    char hdr[512];
    int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
        code, ph, ct, (int)body.size());
    NetSend(req, hdr, hl);
    if (!body.empty()) NetSend(req, body.c_str(), (int)body.size());
    std::wstring dbg = L"HTTP >>> " + s2ws(std::string(hdr, hl));
    if (!body.empty()) {
        std::string b = body.size() > 2000 ? body.substr(0, 2000) + "..." : body;
        dbg += L"\nHTTP >>> " + s2ws(b);
    }
    PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0, (LPARAM)new std::wstring(dbg));
}

static bool RecvBody(HttpReq& req, std::string& out) {
    int64_t n = req.contentLength;
    out.clear(); char buf[4096];
    while (n > 0) {
        int want = (int)ls_min(n, (int64_t)sizeof(buf));
        int r = NetRecv(req, buf, want); if (r <= 0) return false;
        out.append(buf, r); n -= r;
    }
    std::string b = out.size() > 2000 ? out.substr(0, 2000) + "..." : out;
    PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0,
        (LPARAM)new std::wstring(L"HTTP <<< body (" + std::to_wstring(out.size()) + L" bytes): " + s2ws(b)));
    return true;
}

// ---------------------------------------------------------------------------
// [11] LocalSend — HTTP request handlers
// ---------------------------------------------------------------------------
static void HandleInfo(HttpReq& req) {
    std::string resp = JsonBuild({
        {"alias",       ws2s(g_localAlias)},
        {"version",     "2.0"}, {"deviceModel","PC"}, {"deviceType","desktop"},
        {"fingerprint", g_fingerprint}, {"port", std::to_string(g_listenPort)},
        {"protocol",    "https"}, {"download","false"}});
    DebugLog(L"Handler Info: response built (" + std::to_wstring(resp.size()) + L" bytes)");
    SendResponse(req, 200, resp);
}

static void HandlePrepareUpload(HttpReq& req) {
    std::string body;
    if (!RecvBody(req, body)) {
        DebugLog(L"Handler PrepareUpload: RecvBody FAILED");
        SendResponse(req, 500, "{}"); return;
    }
    auto* ir   = new InboundReq;
    ir->hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    ir->senderAlias = s2ws(JsonGet(body, "alias"));
    if (ir->senderAlias.empty()) ir->senderAlias = L"Unknown";

    // Check for agent-to-agent transfer (destinationUser field)
    std::wstring destUser = s2ws(JsonGet(body, "destinationUser"));

    size_t pos = 0;
    while (true) {
        size_t fnp = body.find("\"filename\"", pos); if (fnp == std::string::npos) break;
        size_t ob = body.rfind('{', fnp);
        if (ob != std::string::npos) {
            size_t oe = body.find('}', fnp);
            if (oe != std::string::npos) {
                std::string obj = body.substr(ob, oe - ob + 1);
                FileDesc fd;
                fd.id = s2ws(JsonGet(obj, "id"));
                fd.filename = s2ws(JsonGet(obj, "filename"));
                std::string sz = JsonGet(obj, "size");
                fd.size = sz.empty() ? 0 : (int64_t)atoll(sz.c_str());
                fd.token = MakeId();
                ir->files.push_back(fd);
                pos = oe + 1; continue;
            }
        }
        pos = fnp + 10;
    }

    // Auto-accept if destinationUser is a registered pseudo-user
    bool autoAccept = !destUser.empty() && IsPseudoUser(destUser);
    if (autoAccept) {
        PWSTR dlPath = nullptr;
        SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &dlPath);
        std::wstring dlDir = dlPath ? dlPath : L"C:\\"; if (dlPath) CoTaskMemFree(dlPath);
        std::string sessId = MakeId();
        std::string resp = "{\"sessionId\":\"" + sessId + "\",\"files\":{";
        EnterCriticalSection(&g_xferCs);
        for (size_t i = 0; i < ir->files.size(); ++i) {
            auto& f = ir->files[i];
            if (i) resp += ",";
            resp += "\"" + ws2s(f.id) + "\":\"" + f.token + "\"";
            TransferEntry te;
            te.id = f.id; te.filename = f.filename; te.peerAlias = ir->senderAlias;
            te.localPath = dlDir + L"\\" + f.filename;
            te.totalBytes = f.size; te.dir = XferDir::Inbound; te.state = XferState::Pending;
            te.isAgentTransfer = true; te.destinationAgent = destUser;
            int idx = (int)g_transfers.size(); g_transfers.push_back(te);
            g_tokenToIdx[f.token] = idx;
        }
        LeaveCriticalSection(&g_xferCs);
        resp += "}}"; ir->responseBody = resp; ir->accepted = true;
        DebugLog(L"Handler PrepareUpload: auto-accepted for agent " + destUser);
        SetEvent(ir->hEvent);
        WaitForSingleObject(ir->hEvent, INFINITE);
        CloseHandle(ir->hEvent);
        SendResponse(req, 200, ir->responseBody);
        delete ir; return;
    }

    PostMessage(g_hwnd, WM_LS_INBOUND_REQ, 0, (LPARAM)ir);
    WaitForSingleObject(ir->hEvent, INFINITE);
    CloseHandle(ir->hEvent);
    if (!ir->accepted) {
        DebugLog(L"Handler PrepareUpload: REJECTED by user");
        SendResponse(req, 403, "{}"); delete ir; return;
    }
    DebugLog(L"Handler PrepareUpload: ACCEPTED, response: " + s2ws(ir->responseBody));
    SendResponse(req, 200, ir->responseBody);
    delete ir;
}

static bool StreamToFile(HttpReq& req, const std::wstring& path, int idx, HWND notify) {
    int64_t total = req.contentLength;
    HANDLE hf = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return false;
    char buf[LS_CHUNK]; int64_t done = 0; bool ok = true;
    while (done < total) {
        int want = (int)ls_min((int64_t)sizeof(buf), total - done);
        int r = NetRecv(req, buf, want); if (r <= 0) { ok = false; break; }
        DWORD w; WriteFile(hf, buf, r, &w, nullptr); done += r;
        if (notify && idx >= 0)
            PostMessage(notify, WM_LS_XFER_PROGRESS, (WPARAM)idx, (LPARAM)(LONG)done);
    }
    CloseHandle(hf); return ok;
}

static void HandleUpload(HttpReq& req) {
    std::string token = req.path.find("token=") != std::string::npos
        ? req.path.substr(req.path.find("token=") + 6) : "";
    size_t amp = token.find('&'); if (amp != std::string::npos) token = token.substr(0, amp);
    int idx = -1; std::wstring savePath;
    {
        EnterCriticalSection(&g_xferCs);
        auto it = g_tokenToIdx.find(token);
        if (it != g_tokenToIdx.end()) {
            idx = it->second;
            savePath = g_transfers[idx].localPath;
            g_transfers[idx].state      = XferState::Active;
            g_transfers[idx].totalBytes = req.contentLength;
        }
        LeaveCriticalSection(&g_xferCs);
    }
    if (idx < 0 || savePath.empty()) {
        DebugLog(L"Handler Upload: invalid token, rejecting");
        SendResponse(req, 403, "{}"); return;
    }
    DebugLog(L"Handler Upload: receiving " + std::to_wstring(req.contentLength) + L" bytes to " + savePath);
    bool ok = StreamToFile(req, savePath, idx, g_hwnd);
    {
        EnterCriticalSection(&g_xferCs);
        g_transfers[idx].state = ok ? XferState::Done : XferState::Failed;
        // Store in agent file inbox if this was an auto-accepted pseudo-user transfer
        if (ok && g_transfers[idx].isAgentTransfer) {
            ReceivedFile rf;
            rf.filename  = g_transfers[idx].filename;
            rf.localPath = g_transfers[idx].localPath;
            rf.from      = g_transfers[idx].peerAlias;
            // simple timestamp
            time_t t = time(nullptr); char tsbuf[32] = {};
            strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
            rf.timestamp = s2ws(tsbuf);
            g_fileInbox[g_transfers[idx].destinationAgent].push_back(rf);
        }
        LeaveCriticalSection(&g_xferCs);
    }
    PostMessage(g_hwnd, WM_LS_XFER_DONE, (WPARAM)idx, ok ? 1 : 0);
    DebugLog(ok ? L"Handler Upload: completed OK" : L"Handler Upload: FAILED");
    SendResponse(req, ok ? 200 : 500, "{}");
}

static void HandleCancel(HttpReq& req) {
    EnterCriticalSection(&g_xferCs);
    for (auto& t : g_transfers)
        if (t.state == XferState::Active || t.state == XferState::Pending) t.state = XferState::Cancelled;
    LeaveCriticalSection(&g_xferCs);
    SendResponse(req, 200, "{}");
}

static void HandleTextMessage(HttpReq& req) {
    std::string body;
    if (!RecvBody(req, body)) {
        DebugLog(L"Handler TextMessage: RecvBody FAILED");
        SendResponse(req, 500, "{}"); return;
    }
    std::wstring payload = s2ws(body);
    std::wstring preview = payload;
    for (auto& ch : preview) if (ch < 0x20) ch = L' ';
    if (preview.size() > 80) { preview = preview.substr(0, 80); preview += L"\x2026"; }
    PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0,
        (LPARAM)new std::wstring(L"RECV localmsg payload: " + preview));
    PostMessage(g_hwnd, WM_LS_TEXT_RECEIVED, 0, (LPARAM)new std::wstring(payload));
    SendResponse(req, 200, "{}");
}

// ---------------------------------------------------------------------------
// [12] LocalSend — HTTP connection + accept threads
// ---------------------------------------------------------------------------
struct TlsConnParams {
    SOCKET sock;
    HANDLE sem;
    mbedtls_ssl_context ssl;
};

static DWORD WINAPI HttpConnThread(LPVOID pv) {
    auto* cp = (TlsConnParams*)pv; SOCKET s = cp->sock; HANDLE sem = cp->sem;
    sockaddr_in peer{}; int peerLen = sizeof(peer);
    getpeername(s, (sockaddr*)&peer, &peerLen);
    char peerIp[64] = {}; inet_ntop(AF_INET, &peer.sin_addr, peerIp, sizeof(peerIp));
    int  peerPort = ntohs(peer.sin_port);
    PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0,
        (LPARAM)new std::wstring(L"=== HTTP IN from " + s2ws(peerIp) + L":" + std::to_wstring(peerPort) + L" ==="));
    PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0,
        (LPARAM)new std::wstring(L"TCP connect from " + s2ws(peerIp) + L":" + std::to_wstring(peerPort)));

    for (int i = 0; i < 60 && !g_tlsReady && !g_stopThreads; ++i) Sleep(100);

    mbedtls_ssl_init(&cp->ssl);
    int ret = mbedtls_ssl_setup(&cp->ssl, &g_tlsCfg);
    bool useTls = (ret == 0);
    if (useTls) {
        mbedtls_ssl_set_bio(&cp->ssl, &cp->sock, TlsSend, TlsRecv, nullptr);
        while ((ret = mbedtls_ssl_handshake(&cp->ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                char errbuf[128]; mbedtls_strerror(ret, errbuf, sizeof(errbuf));
                PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0,
                    (LPARAM)new std::wstring(L"TLS setup FAILED from " + s2ws(peerIp) + L": " + s2ws(errbuf)));
                PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0,
                    (LPARAM)new std::wstring(L"TLS handshake FAILED from " + s2ws(peerIp) + L": " + s2ws(errbuf)));
                mbedtls_ssl_free(&cp->ssl);
                closesocket(s); ReleaseSemaphore(sem, 1, nullptr); delete cp; return 0;
            }
        }
        PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0,
            (LPARAM)new std::wstring(L"TLS handshake OK from " + s2ws(peerIp)));
        PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0,
            (LPARAM)new std::wstring(L"TLS handshake OK from " + s2ws(peerIp)));
    } else {
        char errbuf[128]; mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0,
            (LPARAM)new std::wstring(L"TLS setup FAILED: " + s2ws(errbuf)));
        closesocket(s); ReleaseSemaphore(sem, 1, nullptr); delete cp; return 0;
    }

    HttpReq req; req.sock = s; req.ssl = &cp->ssl;
    if (ParseHttpReq(req)) {
        PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0,
            (LPARAM)new std::wstring(s2ws(req.method + " " + req.path.substr(0, 80))));
        if      (req.method=="GET"  && req.path.find("/api/localsend/v2/info")==0)           { PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0, (LPARAM)new std::wstring(L"Handler: Info")); HandleInfo(req); }
        else if (req.method=="POST" && req.path.find("/api/localsend/v2/prepare-upload")==0)  { PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0, (LPARAM)new std::wstring(L"Handler: PrepareUpload")); HandlePrepareUpload(req); }
        else if (req.method=="POST" && req.path.find("/api/localsend/v2/upload")==0)          { PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0, (LPARAM)new std::wstring(L"Handler: Upload")); HandleUpload(req); }
        else if (req.method=="POST" && req.path.find("/api/localsend/v2/cancel")==0)          { PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0, (LPARAM)new std::wstring(L"Handler: Cancel")); HandleCancel(req); }
        else if (req.method=="POST" && req.path.find("/api/localmsg/v1/message")==0)          { PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0, (LPARAM)new std::wstring(L"Handler: TextMessage")); HandleTextMessage(req); }
        else {
            PostMessage(g_hwnd, WM_LS_APPEND_DEBUG, 0,
                (LPARAM)new std::wstring(L"Handler: 404 " + s2ws(req.path)));
            PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0,
                (LPARAM)new std::wstring(L"404 unknown path: " + s2ws(req.path)));
            SendResponse(req, 404, "{}");
        }
    } else {
        PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0,
            (LPARAM)new std::wstring(L"HTTP parse FAILED from " + s2ws(peerIp)));
    }

    mbedtls_ssl_close_notify(&cp->ssl); mbedtls_ssl_free(&cp->ssl);
    closesocket(s); ReleaseSemaphore(sem, 1, nullptr); delete cp; return 0;
}

static DWORD WINAPI HttpAcceptThread(LPVOID) {
    g_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listenSock == INVALID_SOCKET) return 1;
    BOOL reuse = TRUE; setsockopt(g_listenSock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY;
    int boundPort = 0;
    for (int p = LS_PORT; p <= LS_PORT+5; ++p) {
        addr.sin_port = htons((u_short)p);
        if (bind(g_listenSock, (sockaddr*)&addr, sizeof(addr)) == 0) { boundPort = p; break; }
    }
    if (!boundPort) { closesocket(g_listenSock); g_listenSock = INVALID_SOCKET; return 1; }
    g_listenPort = boundPort;
    listen(g_listenSock, SOMAXCONN);
    wchar_t st[256]; swprintf(st, 256, L"Listening on %s:%d (HTTPS)", g_localIp, g_listenPort);
    PostMessage(g_hwnd, WM_LS_STATUS_TEXT, 0, (LPARAM)new std::wstring(st));
    if (boundPort != LS_PORT)
        PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0,
            (LPARAM)new std::wstring(L"WARNING: bound to port " + std::to_wstring(boundPort) +
                                     L" (not " + std::to_wstring(LS_PORT) + L") — other app may occupy 53317"));
    while (!g_stopThreads) {
        fd_set fds; FD_ZERO(&fds); FD_SET(g_listenSock, &fds);
        timeval tv{ 0, 500000 };
        if (select(0, &fds, nullptr, nullptr, &tv) <= 0) continue;
        SOCKET client = accept(g_listenSock, nullptr, nullptr); if (client == INVALID_SOCKET) continue;
        WaitForSingleObject(g_acceptSem, INFINITE);
        auto* cp = new TlsConnParams; cp->sock = client; cp->sem = g_acceptSem;
        HANDLE h = CreateThread(nullptr, 0, HttpConnThread, cp, 0, nullptr);
        if (h) CloseHandle(h); else { closesocket(client); delete cp; ReleaseSemaphore(g_acceptSem, 1, nullptr); }
    }
    closesocket(g_listenSock); g_listenSock = INVALID_SOCKET; return 0;
}

// ---------------------------------------------------------------------------
// [13] LocalSend — Send file thread (WinHTTP)
// ---------------------------------------------------------------------------
static DWORD WINAPI SendFileThread(LPVOID pv) {
    auto* sp = (SendParams*)pv; bool ok = false;
    HANDLE hf = CreateFileW(sp->localPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hf == INVALID_HANDLE_VALUE) { delete sp; return 1; }
    LARGE_INTEGER fsize{}; GetFileSizeEx(hf, &fsize);

    HINTERNET hSess = WinHttpOpen(L"LocalMsg/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConn = hSess ? WinHttpConnect(hSess, sp->peerIp.c_str(), (INTERNET_PORT)sp->peerPort, 0) : nullptr;
    HINTERNET hReq  = nullptr;
    std::string fileId = MakeId(), sessionId, token;

    if (hConn) {
        std::string prepBody =
            "{\"info\":{\"alias\":\"" + ws2s(g_localAlias) + "\",\"version\":\"2.0\","
            "\"deviceModel\":\"PC\",\"deviceType\":\"desktop\",\"fingerprint\":\"" + g_fingerprint + "\"},"
            "\"files\":{\"" + fileId + "\":{\"id\":\"" + fileId + "\","
            "\"filename\":\"" + ws2s(sp->filename) + "\","
            "\"size\":" + std::to_string(fsize.QuadPart) + ",\"fileType\":\"application/octet-stream\"}}}";
        DebugLog(L"=== HTTP OUT to " + sp->peerIp + L":" + std::to_wstring(sp->peerPort) + L" ===");
        DebugLog(L"HTTP >>> POST /api/localsend/v2/prepare-upload");
        DebugLog(L"HTTP >>> body (" + std::to_wstring(prepBody.size()) + L" bytes): " + s2ws(prepBody));
        hReq = WinHttpOpenRequest(hConn, L"POST", L"/api/localsend/v2/prepare-upload",
                                   nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   WINHTTP_FLAG_SECURE);
        if (hReq) {
            DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                          SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
        }
        if (hReq) {
            WinHttpAddRequestHeaders(hReq, L"Content-Type: application/json\r\n", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            DWORD bl = (DWORD)prepBody.size();
            if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)prepBody.c_str(), bl, bl, 0)
                && WinHttpReceiveResponse(hReq, nullptr)) {
                DWORD avail = 0; WinHttpQueryDataAvailable(hReq, &avail);
                if (avail > 0) {
                    std::string resp(avail, 0); DWORD read = 0;
                    WinHttpReadData(hReq, &resp[0], avail, &read); resp.resize(read);
                    DebugLog(L"HTTP <<< prepare-upload response: " + s2ws(resp));
                    sessionId = JsonGet(resp, "sessionId");
                    size_t fp = resp.find("\"files\"");
                    if (fp != std::string::npos) {
                        size_t ob = resp.find('{', fp + 7);
                        if (ob != std::string::npos) {
                            size_t oe = resp.find('}', ob);
                            if (oe != std::string::npos)
                                token = JsonGet(resp.substr(ob, oe - ob + 1), fileId);
                        }
                    }
                }
            } else {
                DebugLog(L"HTTP <<< prepare-upload FAILED (WinHttpSendRequest/ReceiveResponse)");
            }
            WinHttpCloseHandle(hReq); hReq = nullptr;
        }
    }

    if (hConn && !token.empty()) {
        if (sessionId.empty()) sessionId = fileId;
        std::string up = "/api/localsend/v2/upload?sessionId=" + sessionId + "&fileId=" + fileId + "&token=" + token;
        DebugLog(L"HTTP >>> POST " + s2ws(up));
        DebugLog(L"HTTP >>> body: " + std::to_wstring(fsize.QuadPart) + L" bytes (file data)");
        hReq = WinHttpOpenRequest(hConn, L"POST", s2ws(up).c_str(),
                                   nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   WINHTTP_FLAG_SECURE);
        if (hReq) {
            DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                          SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
            WinHttpAddRequestHeaders(hReq, L"Content-Type: application/octet-stream\r\n", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, (DWORD)fsize.QuadPart, 0)) {
                char buf[LS_CHUNK]; int64_t sent = 0; BOOL ro = TRUE;
                while (ro && sent < fsize.QuadPart) {
                    DWORD r = 0; ro = ReadFile(hf, buf, sizeof(buf), &r, nullptr); if (!r) break;
                    DWORD w = 0; WinHttpWriteData(hReq, buf, r, &w); sent += w;
                    if (sp->xferIdx >= 0) {
                        EnterCriticalSection(&g_xferCs);
                        g_transfers[sp->xferIdx].doneBytes = sent;
                        LeaveCriticalSection(&g_xferCs);
                        PostMessage(sp->notifyHwnd, WM_LS_XFER_PROGRESS, (WPARAM)sp->xferIdx, (LPARAM)(LONG)sent);
                    }
                }
                WinHttpReceiveResponse(hReq, nullptr);
                ok = (sent >= fsize.QuadPart);
                DebugLog(ok ? L"HTTP <<< upload OK" : L"HTTP <<< upload FAILED (incomplete)");
            } else {
                DebugLog(L"HTTP <<< upload FAILED (WinHttpSendRequest)");
            }
            WinHttpCloseHandle(hReq);
        }
    } else {
        DebugLog(L"HTTP <<< upload SKIPPED (no token from prepare-upload)");
    }
    CloseHandle(hf);
    if (hConn)  WinHttpCloseHandle(hConn);
    if (hSess)  WinHttpCloseHandle(hSess);
    if (sp->xferIdx >= 0) {
        EnterCriticalSection(&g_xferCs);
        g_transfers[sp->xferIdx].state = ok ? XferState::Done : XferState::Failed;
        LeaveCriticalSection(&g_xferCs);
        PostMessage(sp->notifyHwnd, WM_LS_XFER_DONE, (WPARAM)sp->xferIdx, ok ? 1 : 0);
    }
    delete sp; return 0;
}

// ---------------------------------------------------------------------------
// [14] LocalSend — Send text message thread (WinHTTP)
// ---------------------------------------------------------------------------
static DWORD WINAPI SendTextThread(LPVOID pv) {
    auto* sp = (TextSendParams*)pv;
    std::string body = ws2s(sp->senderAlias) + "\x01" + ws2s(sp->text);
    {
        std::wstring preview = s2ws(body);
        for (auto& ch : preview) if (ch < 0x20) ch = L' ';
        if (preview.size() > 80) { preview = preview.substr(0, 80); preview += L"\x2026"; }
        PostMessage(sp->notifyHwnd, WM_LS_APPEND_COMM, 0,
            (LPARAM)new std::wstring(L"SEND localmsg payload: " + preview));
    }
    DebugLog(L"=== HTTP OUT to " + sp->peerIp + L":" + std::to_wstring(sp->peerPort) + L" ===");
    DebugLog(L"HTTP >>> POST /api/localmsg/v1/message");
    DebugLog(L"HTTP >>> body (" + std::to_wstring(body.size()) + L" bytes): " + s2ws(body));
    HINTERNET hSess = WinHttpOpen(L"LocalMsg/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConn = hSess ? WinHttpConnect(hSess, sp->peerIp.c_str(), (INTERNET_PORT)sp->peerPort, 0) : nullptr;
    if (hConn) {
        HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", L"/api/localmsg/v1/message",
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (hReq) {
            DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                          SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
            WinHttpAddRequestHeaders(hReq, L"Content-Type: text/plain; charset=utf-8\r\n", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            DWORD bl = (DWORD)body.size();
            if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), bl, bl, 0)
                && WinHttpReceiveResponse(hReq, nullptr)) {
                DebugLog(L"HTTP <<< text message sent OK");
            } else {
                DebugLog(L"HTTP <<< text message FAILED");
            }
            WinHttpCloseHandle(hReq);
        }
        WinHttpCloseHandle(hConn);
    }
    if (hSess) WinHttpCloseHandle(hSess);
    delete sp; return 0;
}

// ---------------------------------------------------------------------------
// [15] IPMsg Protocol Helpers
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
    pkt.packetNo = (DWORD)atoll(parts[1].c_str());
    pkt.senderUser = s2ws(parts[2]);
    pkt.senderHost = s2ws(parts[3]);
    pkt.command = (DWORD)atoll(parts[4].c_str());
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

static std::wstring IpMsgDirStr(DWORD cmd) {
    DWORD mode = GET_MODE(cmd);
    if (mode == IPMSG_BR_ENTRY)  return L"BR_ENTRY";
    if (mode == IPMSG_BR_EXIT)   return L"BR_EXIT";
    if (mode == IPMSG_ANSENTRY)  return L"ANSENTRY";
    if (mode == IPMSG_SENDMSG)   return L"SENDMSG";
    if (mode == IPMSG_RECVMSG)   return L"RECVMSG";
    if (mode == IPMSG_READMSG)   return L"READMSG";
    if (mode == IPMSG_DELMSG)    return L"DELMSG";
    if (mode == IPMSG_ANSREADMSG) return L"ANSREADMSG";
    char buf[16]; snprintf(buf,16,"0x%02lx",(unsigned long)mode);
    return s2ws(buf);
}

// ---------------------------------------------------------------------------
// [16] IPMsg — Pseudo User Management
// ---------------------------------------------------------------------------
static std::wstring GetPrimaryUser() {
    std::wstring user;
    EnterCriticalSection(&g_pseudoCs);
    if (!g_pseudoUsers.empty()) user = g_pseudoUsers[0].username;
    LeaveCriticalSection(&g_pseudoCs);
    return user;
}

static bool IsPseudoUser(const std::wstring& username) {
    bool found = false;
    EnterCriticalSection(&g_pseudoCs);
    for (auto& u : g_pseudoUsers) if (u.username == username) { found = true; break; }
    LeaveCriticalSection(&g_pseudoCs);
    return found;
}

static bool AddPseudoUser(const std::wstring& username) {
    EnterCriticalSection(&g_pseudoCs);
    for (auto& u : g_pseudoUsers)
        if (u.username == username) { LeaveCriticalSection(&g_pseudoCs); return false; }
    PseudoUser pu; pu.username = username; pu.hostname = g_myHostname; pu.active = true;
    g_pseudoUsers.push_back(pu);
    LeaveCriticalSection(&g_pseudoCs);
    DebugLog(L"Pseudo user added: " + username);
    return true;
}

static bool RemovePseudoUser(const std::wstring& username) {
    EnterCriticalSection(&g_pseudoCs);
    for (size_t i = 0; i < g_pseudoUsers.size(); ++i)
        if (g_pseudoUsers[i].username == username) { g_pseudoUsers.erase(g_pseudoUsers.begin()+i); LeaveCriticalSection(&g_pseudoCs); DebugLog(L"Pseudo user removed: " + username); return true; }
    LeaveCriticalSection(&g_pseudoCs);
    return false;
}

// ---------------------------------------------------------------------------
// [17] IPMsg — Message Inbox
// ---------------------------------------------------------------------------
static void PushMessage(const std::wstring& from, const std::wstring& to, const std::wstring& text) {
    Message m; m.id = g_nextMsgId++; m.from = from; m.to = to; m.text = text; m.time = TimeStamp();
    EnterCriticalSection(&g_msgCs);
    g_messages.push_back(m);
    if (g_messages.size() > 1000) g_messages.erase(g_messages.begin());
    LeaveCriticalSection(&g_msgCs);
}

static std::wstring GetMessagesJson(const std::wstring& user, bool advancePtr) {
    std::string r = "[";
    int lastId = 0;
    EnterCriticalSection(&g_msgCs);
    int ptr = 0;
    if (!user.empty()) {
        auto it = g_msgReadPtr.find(user);
        if (it != g_msgReadPtr.end()) ptr = it->second;
    }
    bool first = true;
    for (auto& m : g_messages) {
        if (!user.empty() && m.to != user) continue;
        if (m.id <= ptr) continue;
        if (!first) r += ","; first = false;
        r += "{\"id\":" + std::to_string(m.id) +
             ",\"from\":\"" + EscapeJson(ws2s(m.from)) +
             "\",\"to\":\"" + EscapeJson(ws2s(m.to)) +
             "\",\"text\":\"" + EscapeJson(ws2s(m.text)) +
             "\",\"time\":\"" + EscapeJson(ws2s(m.time)) + "\"}";
        if (m.id > lastId) lastId = m.id;
    }
    if (advancePtr && !user.empty() && lastId > 0)
        g_msgReadPtr[user] = lastId;
    LeaveCriticalSection(&g_msgCs);
    r += "]"; return s2ws(r);
}

// ---------------------------------------------------------------------------
// [18a] IPMsg — Pending sends (retry on no ACK)
// ---------------------------------------------------------------------------
struct PendingSend {
    DWORD    packetNo = 0;
    std::wstring fromUser;
    std::wstring toUser;
    std::wstring text;
    sockaddr_in dest = {};
    int      retries = 0;
    DWORD    sentAt = 0;
};

static CRITICAL_SECTION g_pendCs;
static std::vector<PendingSend> g_pendingSends;
static bool g_pendInit = false;

static void StorePendingSend(const PendingSend& ps) {
    if (!g_pendInit) { InitializeCriticalSection(&g_pendCs); g_pendInit = true; }
    EnterCriticalSection(&g_pendCs);
    g_pendingSends.push_back(ps);
    LeaveCriticalSection(&g_pendCs);
}

static void AckPendingSend(DWORD packetNo) {
    if (!g_pendInit) return;
    EnterCriticalSection(&g_pendCs);
    for (size_t i = 0; i < g_pendingSends.size(); ++i) {
        if (g_pendingSends[i].packetNo == packetNo) {
            g_pendingSends.erase(g_pendingSends.begin() + i);
            break;
        }
    }
    LeaveCriticalSection(&g_pendCs);
}

static void RetryPendingSends() {
    if (!g_pendInit || g_ipmsgSock == INVALID_SOCKET) return;
    DWORD now = GetTickCount();
    EnterCriticalSection(&g_pendCs);
    for (size_t i = 0; i < g_pendingSends.size(); ) {
        auto& ps = g_pendingSends[i];
        if (now - ps.sentAt >= 3000) { // 3s timeout
            if (ps.retries >= 3) {
                DebugLog(L"IPMSG send FAILED (no ACK) to " + ps.toUser + L" pkt=" + std::to_wstring(ps.packetNo));
                g_pendingSends.erase(g_pendingSends.begin() + i);
                continue;
            }
            // Retransmit with RETRYOPT
            IpMsgPacket pkt;
            pkt.version = 1; pkt.packetNo = ps.packetNo;
            pkt.senderUser = ps.fromUser; pkt.senderHost = g_myHostname;
            pkt.command = IPMSG_SENDMSG | IPMSG_SENDCHECKOPT | IPMSG_RETRYOPT | IPMSG_CAPUTF8OPT | IPMSG_UTF8OPT;
            pkt.destUser = ps.toUser; pkt.extra = ps.text;
            std::string raw = ComposeIpMsgPacket(pkt);
            sendto(g_ipmsgSock, raw.c_str(), (int)raw.size(), 0, (const sockaddr*)&ps.dest, sizeof(ps.dest));
            ps.retries++;
            ps.sentAt = now;
            DebugLog(L"IPMSG retry " + std::to_wstring(ps.retries) + L"/3 for pkt " + std::to_wstring(ps.packetNo) + L" to " + ps.toUser);
            i++;
        } else {
            i++;
        }
    }
    LeaveCriticalSection(&g_pendCs);
}

// ---------------------------------------------------------------------------
// [18] IPMsg — Send via UDP
// ---------------------------------------------------------------------------
static void SendIpMsg(const IpMsgPacket& pkt, const sockaddr_in& dest) {
    std::string raw = ComposeIpMsgPacket(pkt);
    if (g_ipmsgSock == INVALID_SOCKET) return;
    sendto(g_ipmsgSock, raw.c_str(), (int)raw.size(), 0, (const sockaddr*)&dest, sizeof(dest));
}

static bool SendTextIpMsg(const std::wstring& fromUser, const std::wstring& toUser,
                          const std::wstring& text) {
    PeerInfo target;
    bool isIp = toUser.find(L'.') != std::wstring::npos;
    EnterCriticalSection(&g_peerCs);
    if (isIp) {
        for (auto& p : g_peers)
            if (p.ip == toUser && p.protocol == Proto::IPMsg) { target = p; break; }
    } else {
        for (auto& p : g_peers)
            if (p.alias == toUser && p.protocol == Proto::IPMsg) { target = p; break; }
    }
    LeaveCriticalSection(&g_peerCs);
    if (target.ip.empty()) return false;

    IpMsgPacket pkt;
    static DWORD pktNo = 1;
    pkt.version = 1; pkt.packetNo = pktNo++;
    pkt.senderUser = fromUser; pkt.senderHost = g_myHostname;
    pkt.command = IPMSG_SENDMSG | IPMSG_SENDCHECKOPT | IPMSG_CAPUTF8OPT | IPMSG_UTF8OPT;
    pkt.destUser = toUser; pkt.extra = text;

    sockaddr_in dest = {};
    dest.sin_family = AF_INET; dest.sin_port = htons(IPMSG_PORT);
    inet_pton(AF_INET, ws2s(target.ip).c_str(), &dest.sin_addr);
    SendIpMsg(pkt, dest);
    DebugLog(L"IPMSG >>> SENDMSG to " + toUser + L": " + text);
    // Track for retry on no ACK
    PendingSend ps;
    ps.packetNo = pkt.packetNo; ps.fromUser = fromUser; ps.toUser = toUser;
    ps.text = text; ps.dest = dest; ps.sentAt = GetTickCount();
    StorePendingSend(ps);
    return true;
}

// ---------------------------------------------------------------------------
// [19] IPMsg — UDP Discovery & Message Thread
// ---------------------------------------------------------------------------
static void AnnouncePseudoUsers(const sockaddr_in* target = nullptr) {
    if (g_ipmsgSock == INVALID_SOCKET) return;
    EnterCriticalSection(&g_pseudoCs);
    for (auto& pu : g_pseudoUsers) {
        if (!pu.active) continue;
        IpMsgPacket pkt;
        static DWORD pktNo = 1;
        pkt.version = 1; pkt.packetNo = pktNo++;
        pkt.senderUser = pu.username; pkt.senderHost = pu.hostname;
        pkt.command = IPMSG_BR_ENTRY | IPMSG_CAPUTF8OPT;
        pkt.extra = pu.username;
        std::string raw = ComposeIpMsgPacket(pkt);
        if (target) {
            sendto(g_ipmsgSock, raw.c_str(), (int)raw.size(), 0, (const sockaddr*)target, sizeof(*target));
        } else {
            sockaddr_in bc = {}; bc.sin_family = AF_INET; bc.sin_port = htons(IPMSG_PORT); bc.sin_addr.s_addr = INADDR_BROADCAST;
            sendto(g_ipmsgSock, raw.c_str(), (int)raw.size(), 0, (const sockaddr*)&bc, sizeof(bc));
            DebugLog(L"IPMSG >>> BR_ENTRY for " + pu.username);
        }
    }
    LeaveCriticalSection(&g_pseudoCs);
}

static void SendBrExit(const std::wstring& username) {
    if (g_ipmsgSock == INVALID_SOCKET) return;
    IpMsgPacket pkt;
    static DWORD pktNo = 1;
    pkt.version = 1; pkt.packetNo = pktNo++;
    pkt.senderUser = username; pkt.senderHost = g_myHostname;
    pkt.command = IPMSG_BR_EXIT | IPMSG_CAPUTF8OPT;
    pkt.extra = username;
    sockaddr_in bc = {}; bc.sin_family = AF_INET; bc.sin_port = htons(IPMSG_PORT); bc.sin_addr.s_addr = INADDR_BROADCAST;
    std::string raw = ComposeIpMsgPacket(pkt);
    sendto(g_ipmsgSock, raw.c_str(), (int)raw.size(), 0, (const sockaddr*)&bc, sizeof(bc));
    DebugLog(L"IPMSG >>> BR_EXIT for " + username);
}

static DWORD WINAPI IpMsgThread(LPVOID) {
    g_ipmsgSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_ipmsgSock == INVALID_SOCKET) return 1;
    BOOL broadcast = TRUE; setsockopt(g_ipmsgSock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast));
    BOOL reuse = TRUE; setsockopt(g_ipmsgSock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    sockaddr_in local = {}; local.sin_family = AF_INET; local.sin_port = htons(IPMSG_PORT); local.sin_addr.s_addr = INADDR_ANY;
    if (bind(g_ipmsgSock, (sockaddr*)&local, sizeof(local)) != 0) {
        DebugLog(L"IPMSG: bind port " + std::to_wstring(IPMSG_PORT) + L" FAILED");
        closesocket(g_ipmsgSock); g_ipmsgSock = INVALID_SOCKET; return 1;
    }
    DebugLog(L"IPMSG: listening on port " + std::to_wstring(IPMSG_PORT));
    Sleep(500);
    // Auto-create hostname-based identity if no pseudo user exists
    EnterCriticalSection(&g_pseudoCs);
    bool hasUsers = !g_pseudoUsers.empty();
    LeaveCriticalSection(&g_pseudoCs);
    if (!hasUsers) {
        std::wstring hostIdent = g_myHostname;
        AddPseudoUser(hostIdent);
        DebugLog(L"IPMSG: auto-identity set to " + hostIdent);
    }
    AnnouncePseudoUsers();
    DWORD lastAnnounce = GetTickCount();

    while (!g_stopThreads) {
        fd_set fds; FD_ZERO(&fds); FD_SET(g_ipmsgSock, &fds);
        timeval tv = { 0, 100000 };
        if (select(0, &fds, nullptr, nullptr, &tv) > 0) {
            char buf[8192] = {}; sockaddr_in from = {}; int fromLen = sizeof(from);
            int n = recvfrom(g_ipmsgSock, buf, sizeof(buf)-1, 0, (sockaddr*)&from, &fromLen);
            if (n > 0) {
                std::string raw(buf, n);
                char ipbuf[64] = {}; inet_ntop(AF_INET, &from.sin_addr, ipbuf, sizeof(ipbuf));
                std::wstring fromIp = s2ws(ipbuf);
                IpMsgPacket pkt;
                if (!ParseIpMsgPacket(raw, pkt)) { DebugLog(L"IPMSG <<< parse FAILED from " + fromIp); continue; }
                DWORD mode = GET_MODE(pkt.command);
                DebugLog(L"IPMSG <<< " + IpMsgDirStr(pkt.command) + L" from " + pkt.senderUser + L"@" + fromIp + L" dst=" + pkt.destUser);

                switch (mode) {
                case IPMSG_BR_ENTRY: {
                    bool self = false;
                    EnterCriticalSection(&g_pseudoCs);
                    for (auto& pu : g_pseudoUsers) if (pu.username == pkt.senderUser) { self = true; break; }
                    LeaveCriticalSection(&g_pseudoCs);
                    if (self) break;
                    EnterCriticalSection(&g_peerCs);
                    bool found = false;
                    for (auto& e : g_peers)
                        if (e.alias == pkt.senderUser && e.ip == fromIp && e.protocol == Proto::IPMsg) { e.lastSeenMs = GetTickCount(); e.hostname = pkt.senderHost; found = true; break; }
                    if (!found) {
                        PeerInfo pi; pi.alias = pkt.senderUser; pi.hostname = pkt.senderHost; pi.ip = fromIp; pi.lastSeenMs = GetTickCount(); pi.protocol = Proto::IPMsg;
                        g_peers.push_back(pi);
                    }
                    LeaveCriticalSection(&g_peerCs);
                    PostMessage(g_hwnd, WM_IPMSG_UPDATE_PEERS, 0, 0);
                    PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0, (LPARAM)new std::wstring(L"IPMSG peer found: " + pkt.senderUser + L" " + fromIp));
                    // Reply ANSENTRY for each pseudo user
                    EnterCriticalSection(&g_pseudoCs);
                    for (auto& pu : g_pseudoUsers) {
                        if (!pu.active) continue;
                        IpMsgPacket ans; static DWORD ansNo = 1;
                        ans.version = 1; ans.packetNo = ansNo++;
                        ans.senderUser = pu.username; ans.senderHost = pu.hostname;
                        ans.command = IPMSG_ANSENTRY | IPMSG_CAPUTF8OPT;
                        ans.destUser = pkt.senderUser; ans.extra = pu.username;
                        sockaddr_in dest = from; SendIpMsg(ans, dest);
                    }
                    LeaveCriticalSection(&g_pseudoCs);
                    break;
                }
                case IPMSG_ANSENTRY: {
                    EnterCriticalSection(&g_peerCs);
                    bool found = false;
                    for (auto& e : g_peers)
                        if (e.alias == pkt.senderUser && e.ip == fromIp && e.protocol == Proto::IPMsg) { e.lastSeenMs = GetTickCount(); found = true; break; }
                    if (!found) {
                        PeerInfo pi; pi.alias = pkt.senderUser; pi.hostname = pkt.senderHost; pi.ip = fromIp; pi.lastSeenMs = GetTickCount(); pi.protocol = Proto::IPMsg;
                        g_peers.push_back(pi);
                    }
                    LeaveCriticalSection(&g_peerCs);
                    PostMessage(g_hwnd, WM_IPMSG_UPDATE_PEERS, 0, 0);
                    break;
                }
                case IPMSG_SENDMSG: {
                    // Duplicate / replay detection per sender IP.
                    // Accept if: first packet from this peer, or packetNo advanced forward,
                    // or packetNo wrapped/reset (new value is much smaller than last → new session).
                    bool isDuplicate = false;
                    {
                        auto it = g_ipmsgLastPktNo.find(fromIp);
                        if (it != g_ipmsgLastPktNo.end()) {
                            DWORD last = it->second;
                            bool reset = (pkt.packetNo < last) && ((last - pkt.packetNo) > 10000);
                            if (!reset && pkt.packetNo <= last)
                                isDuplicate = true;
                            else
                                it->second = pkt.packetNo;
                        } else {
                            g_ipmsgLastPktNo[fromIp] = pkt.packetNo;
                        }
                    }
                    if (isDuplicate) {
                        // Still send RECVMSG so sender stops retransmitting
                        std::wstring myName = GetPrimaryUser();
                        if (myName.empty()) myName = g_myHostname;
                        IpMsgPacket recv; recv.version = 1; recv.packetNo = pkt.packetNo;
                        recv.senderUser = myName; recv.senderHost = g_myHostname;
                        recv.command = IPMSG_RECVMSG;
                        recv.extra = std::to_wstring(pkt.packetNo);
                        sockaddr_in dest = from; SendIpMsg(recv, dest);
                        DebugLog(L"IPMSG dup pkt " + std::to_wstring(pkt.packetNo) + L" from " + fromIp + L" — RECVMSG resent");
                        break;
                    }
                    // Extract text before first \0
                    std::wstring msgText = pkt.extra;
                    size_t nulPos = msgText.find(L'\0');
                    if (nulPos != std::wstring::npos) msgText = msgText.substr(0, nulPos);
                    bool isSecret = (GET_OPT(pkt.command) & IPMSG_SECRETOPT) != 0;
                    std::wstring toUser = pkt.destUser.empty() ? L"*" : pkt.destUser;
                    // Deduplicate delayed messages by content hash
                    std::wstring msgKey = pkt.senderUser + L"\x01" + msgText;
                    uint64_t hash = 14695981039346656037ULL;
                    for (auto& ch : msgKey) { hash ^= (unsigned char)ch; hash *= 1099511628211ULL; }
                    bool isDelayed = (GET_OPT(pkt.command) & IPMSG_RETRYOPT) != 0;
                    if (isDelayed) {
                        if (g_ipmsgSeenMsgs.find(hash) != g_ipmsgSeenMsgs.end()) {
                            DebugLog(L"IPMSG delayed dup (content seen) from " + pkt.senderUser + L" — skipped");
                            // Still send ACK
                            std::wstring myName = GetPrimaryUser();
                            if (myName.empty()) myName = g_myHostname;
                            IpMsgPacket recv; recv.version = 1; recv.packetNo = pkt.packetNo;
                            recv.senderUser = myName; recv.senderHost = g_myHostname;
                            recv.command = IPMSG_RECVMSG;
                            recv.extra = std::to_wstring(pkt.packetNo);
                            sockaddr_in dest = from; SendIpMsg(recv, dest);
                            DebugLog(L"IPMSG >>> RECVMSG (delayed dup) for pkt " + std::to_wstring(pkt.packetNo));
                            break;
                        }
                        g_ipmsgSeenMsgs.insert(hash);
                        if (g_ipmsgSeenMsgs.size() > 500) g_ipmsgSeenMsgs.erase(g_ipmsgSeenMsgs.begin());
                    }
                    // Batch detection: if >2 msgs from same IP arrive within 1s, suppress rest
                    bool suppressDisplay = false;
                    {
                        DWORD now = GetTickCount();
                        auto& bt = g_ipmsgBatchTime[fromIp];
                        auto& rl = g_ipmsgRateLimit[fromIp];
                        if (now - rl.second > 60000) { rl.first = 0; rl.second = now; }
                        rl.first++;
                        if (now - bt <= 1000 && rl.first > 1) suppressDisplay = true;
                        bt = now;
                        if (suppressDisplay && rl.first == 2) DebugLog(L"IPMSG batch detected from " + fromIp + L" — suppressing rest");
                        if (rl.first > 10) suppressDisplay = true; // hard cap
                    }
                    if (suppressDisplay) {
                        DebugLog(L"IPMSG suppressed (rate-limit) from " + pkt.senderUser + L": " + msgText);
                    } else {
                        std::wstring display = isSecret ? L"[envelop] " + msgText : msgText;
                        PushMessage(pkt.senderUser, toUser, display);
                        PostMessage(g_hwnd, WM_IPMSG_TEXT_RCVD, 0, (LPARAM)new std::wstring(pkt.senderUser + L"\x01" + toUser + L"\x01" + display));
                    }
                    // Always send RECVMSG ack
                    std::wstring myName = GetPrimaryUser();
                    if (myName.empty()) myName = g_myHostname;
                    {
                        IpMsgPacket recv; recv.version = 1; recv.packetNo = pkt.packetNo;
                        recv.senderUser = myName; recv.senderHost = g_myHostname;
                        recv.command = IPMSG_RECVMSG;
                        recv.extra = std::to_wstring(pkt.packetNo);
                        sockaddr_in dest = from; SendIpMsg(recv, dest);
                        DebugLog(L"IPMSG >>> RECVMSG ack for packet " + std::to_wstring(pkt.packetNo));
                    }
                    // For sealed messages, send READMSG
                    if (isSecret) {
                        IpMsgPacket read; read.version = 1; read.packetNo = pkt.packetNo;
                        read.senderUser = myName; read.senderHost = g_myHostname;
                        read.command = IPMSG_READMSG;
                        if (GET_OPT(pkt.command) & IPMSG_READCHECKOPT) read.command |= IPMSG_READCHECKOPT;
                        read.extra = std::to_wstring(pkt.packetNo);
                        sockaddr_in dest = from; SendIpMsg(read, dest);
                        DebugLog(L"IPMSG >>> READMSG for sealed packet " + std::to_wstring(pkt.packetNo));
                    }
                    break;
                }
                case IPMSG_RECVMSG: {
                    DebugLog(L"IPMSG <<< RECVMSG ack for packet " + pkt.extra);
                    // Match by extra (packetNo string) or packetNo
                    DWORD ackPktNo = (DWORD)_wtoi64(pkt.extra.c_str());
                    if (ackPktNo > 0) AckPendingSend(ackPktNo);
                    break;
                }
                case IPMSG_READMSG: {
                    DebugLog(L"IPMSG <<< READMSG (envelope opened) for packet " + pkt.extra);
                    // If READCHECKOPT is set, send ANSREADMSG back
                    if (GET_OPT(pkt.command) & IPMSG_READCHECKOPT) {
                        std::wstring myName = GetPrimaryUser();
                        if (myName.empty()) myName = g_myHostname;
                        IpMsgPacket ans; ans.version = 1; ans.packetNo = pkt.packetNo;
                        ans.senderUser = myName; ans.senderHost = g_myHostname;
                        ans.command = IPMSG_ANSREADMSG;
                        ans.extra = std::to_wstring(pkt.packetNo);
                        sockaddr_in dest = from; SendIpMsg(ans, dest);
                        DebugLog(L"IPMSG >>> ANSREADMSG for packet " + std::to_wstring(pkt.packetNo));
                    }
                    break;
                }
                case IPMSG_DELMSG:
                    DebugLog(L"IPMSG <<< DELMSG (envelope discarded) for packet " + pkt.extra);
                    break;
                case IPMSG_ANSREADMSG:
                    DebugLog(L"IPMSG <<< ANSREADMSG (read confirmed) for packet " + pkt.extra);
                    break;
                case IPMSG_BR_EXIT: {
                    EnterCriticalSection(&g_peerCs);
                    for (size_t i = 0; i < g_peers.size(); ++i)
                        if (g_peers[i].alias == pkt.senderUser && g_peers[i].ip == fromIp && g_peers[i].protocol == Proto::IPMsg) {
                            DebugLog(L"IPMSG removed by BR_EXIT: " + g_peers[i].alias + L" @" + g_peers[i].ip);
                            g_peers.erase(g_peers.begin()+i); break;
                        }
                    LeaveCriticalSection(&g_peerCs);
                    PostMessage(g_hwnd, WM_IPMSG_UPDATE_PEERS, 0, 0);
                    PostMessage(g_hwnd, WM_LS_APPEND_COMM, 0, (LPARAM)new std::wstring(L"IPMSG peer gone: " + pkt.senderUser));
                    break;
                }
                }
            }
        }
        RetryPendingSends();
        if (GetTickCount() - lastAnnounce > 600000) { AnnouncePseudoUsers(); lastAnnounce = GetTickCount(); }
    }
    closesocket(g_ipmsgSock); g_ipmsgSock = INVALID_SOCKET;
    return 0;
}

// ---------------------------------------------------------------------------
// [20] REST API HTTP Server Thread
// ---------------------------------------------------------------------------
static void SendRestResp(SOCKET s, int code, const std::string& body, const char* ct = "application/json") {
    const char* ph = (code==200)?"OK":(code==400)?"Bad Request":(code==404)?"Not Found":"Error";
    char hdr[512]; int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        code, ph, ct, (int)body.size());
    send(s, hdr, hl, 0); if (!body.empty()) send(s, body.c_str(), (int)body.size(), 0);
}

static void HandleRestRequest(SOCKET s) {
    std::string line, method, path;
    // Parse request line
    { char c; while (recv(s, &c, 1, 0) > 0) { if (c == '\n') break; if (c != '\r') line += c; } }
    if (line.empty()) { closesocket(s); return; }
    size_t s1 = line.find(' '); if (s1 == std::string::npos) { closesocket(s); return; }
    method = line.substr(0, s1);
    size_t s2 = line.find(' ', s1+1);
    path = s2 == std::string::npos ? line.substr(s1+1) : line.substr(s1+1, s2-s1-1);
    // Parse headers
    std::map<std::string,std::string> hdrs;
    int contentLen = 0;
    while (true) {
        line.clear(); char c;
        while (recv(s, &c, 1, 0) > 0) { if (c == '\n') break; if (c != '\r') line += c; }
        if (line.empty()) break;
        size_t col = line.find(':'); if (col == std::string::npos) continue;
        std::string k = line.substr(0,col), v = line.substr(col+1);
        while (!v.empty() && v.front()==' ') v.erase(v.begin());
        for (auto& ch:k) ch=(char)tolower((unsigned char)ch);
        hdrs[k]=v;
    }
    auto it = hdrs.find("content-length");
    if (it != hdrs.end()) contentLen = atoi(it->second.c_str());
    // Read body
    std::string body;
    while (contentLen > 0) { char buf[4096]; int want = ls_min(contentLen,(int)sizeof(buf)); int r = recv(s,buf,want,0); if(r<=0)break; body.append(buf,r); contentLen-=r; }

    std::string resp;
    if (method=="GET" && path=="/api/users") {
        resp = "[";
        EnterCriticalSection(&g_pseudoCs);
        for (size_t i=0;i<g_pseudoUsers.size();++i) {
            if (i) resp+=",";
            resp+="{\"username\":\""+EscapeJson(ws2s(g_pseudoUsers[i].username))+"\",\"hostname\":\""+EscapeJson(ws2s(g_pseudoUsers[i].hostname))+"\",\"ip\":\"\",\"is_me\":true}";
        }
        LeaveCriticalSection(&g_pseudoCs);
        EnterCriticalSection(&g_peerCs);
        for (auto& p : g_peers) {
            resp += (resp.size()>1?",":"");
            std::string proto = (p.protocol == Proto::IPMsg) ? "IPMsg" : "LS";
            resp += "{\"username\":\""+EscapeJson(ws2s(p.alias))+"\",\"hostname\":\""+EscapeJson(ws2s(p.hostname))+"\",\"ip\":\""+EscapeJson(ws2s(p.ip))+"\",\"protocol\":\""+proto+"\",\"is_me\":false}";
        }
        LeaveCriticalSection(&g_peerCs);
        resp += "]";
        SendRestResp(s,200,resp);
    } else if (method=="GET" && path.find("/api/messages")==0) {
        std::wstring filter;
        bool advancePtr = true;
        size_t q = path.find("?user=");
        if (q != std::string::npos) {
            std::string rest = path.substr(q+6);
            size_t a = rest.find('&');
            if (a != std::string::npos) {
                filter = s2ws(rest.substr(0, a));
                if (rest.find("peek") != std::string::npos || rest.find("advance=0") != std::string::npos)
                    advancePtr = false;
            } else {
                filter = s2ws(rest);
            }
            if (!IsPseudoUser(filter)) filter = GetPrimaryUser();
        }
        SendRestResp(s,200,ws2s(GetMessagesJson(filter, advancePtr)));
    } else if (method=="POST" && path=="/api/send") {
        std::string from = JsonGet(body,"from");
        std::string to = JsonGet(body,"to");
        std::string text = JsonGet(body,"text");
        if (from.empty()) { from = ws2s(GetPrimaryUser()); if (from.empty()) from = ws2s(g_myHostname); }
        if (to.empty()||text.empty()) { SendRestResp(s,400,"{\"ok\":false,\"error\":\"missing to/text\"}"); closesocket(s); return; }
        bool ok = false;
        if (IsPseudoUser(s2ws(to))) {
            PushMessage(s2ws(from), s2ws(to), s2ws(text));
            ok = true;
        } else {
            ok = SendTextIpMsg(s2ws(from), s2ws(to), s2ws(text));
        }
        SendRestResp(s,ok?200:404,ok?"{\"ok\":true}":"{\"ok\":false,\"error\":\"peer not found\"}");
    } else if (method=="POST" && path=="/api/login") {
        std::string username = JsonGet(body,"username");
        if (username.empty()) { SendRestResp(s,400,"{\"ok\":false,\"error\":\"missing username\"}"); closesocket(s); return; }
        std::wstring uname = s2ws(username);
        if (AddPseudoUser(uname)) {
            sockaddr_in bc = {}; bc.sin_family=AF_INET; bc.sin_port=htons(IPMSG_PORT); bc.sin_addr.s_addr=INADDR_BROADCAST;
            IpMsgPacket pkt; static DWORD pktNo=1;
            pkt.version=1; pkt.packetNo=pktNo++; pkt.senderUser=uname; pkt.senderHost=g_myHostname;
            pkt.command=IPMSG_BR_ENTRY|IPMSG_CAPUTF8OPT; pkt.extra=uname;
            std::string raw = ComposeIpMsgPacket(pkt);
            if (g_ipmsgSock != INVALID_SOCKET) sendto(g_ipmsgSock,raw.c_str(),(int)raw.size(),0,(const sockaddr*)&bc,sizeof(bc));
            DebugLog(L"IPMSG >>> BR_ENTRY for " + uname + L" (via login)");
            SendRestResp(s,200,"{\"ok\":true}");
        } else {
            SendRestResp(s,409,"{\"ok\":false,\"error\":\"username already exists\"}");
        }
    } else if (method=="POST" && path=="/api/logout") {
        std::string username = JsonGet(body,"username");
        if (username.empty()) { SendRestResp(s,400,"{\"ok\":false,\"error\":\"missing username\"}"); closesocket(s); return; }
        std::wstring uname = s2ws(username);
        SendBrExit(uname);
        RemovePseudoUser(uname);
        SendRestResp(s,200,"{\"ok\":true}");
    } else if (method=="GET" && path.find("/api/files")==0) {
        std::wstring user;
        bool peek = false;
        size_t q = path.find("?");
        if (q != std::string::npos) {
            std::string qs = path.substr(q+1);
            size_t u = qs.find("user=");
            if (u != std::string::npos) {
                std::string rest = qs.substr(u+5);
                size_t amp = rest.find('&');
                user = s2ws(amp != std::string::npos ? rest.substr(0,amp) : rest);
            }
            if (qs.find("peek") != std::string::npos) peek = true;
        }
        if (user.empty()) user = GetPrimaryUser();
        std::string filesResp = "[";
        EnterCriticalSection(&g_xferCs);
        auto fit = g_fileInbox.find(user);
        if (fit != g_fileInbox.end()) {
            for (size_t i = 0; i < fit->second.size(); ++i) {
                auto& rf = fit->second[i];
                if (i) filesResp += ",";
                filesResp += "{\"filename\":\"" + EscapeJson(ws2s(rf.filename)) + "\""
                           + ",\"path\":\""     + EscapeJson(ws2s(rf.localPath)) + "\""
                           + ",\"from\":\""     + EscapeJson(ws2s(rf.from))      + "\""
                           + ",\"timestamp\":\"" + EscapeJson(ws2s(rf.timestamp)) + "\"}";
            }
            if (!peek) fit->second.clear();
        }
        LeaveCriticalSection(&g_xferCs);
        filesResp += "]";
        SendRestResp(s, 200, filesResp);
    } else if (method=="GET" && path.find("/api/wait")==0) {
        std::wstring filter; int timeoutSec = 30;
        size_t q = path.find("?");
        if (q != std::string::npos) {
            std::string qs = path.substr(q+1);
            size_t u = qs.find("user=");
            if (u != std::string::npos) {
                std::string rest = qs.substr(u+5);
                size_t amp = rest.find('&');
                filter = s2ws(amp != std::string::npos ? rest.substr(0,amp) : rest);
            }
            size_t t = qs.find("timeout=");
            if (t != std::string::npos) timeoutSec = atoi(qs.substr(t+8).c_str());
        }
        if (filter.empty() || !IsPseudoUser(filter)) filter = GetPrimaryUser();
        if (timeoutSec < 1)   timeoutSec = 1;
        if (timeoutSec > 120) timeoutSec = 120;
        int soTO = (timeoutSec + 5) * 1000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&soTO, sizeof(soTO));
        DWORD deadline = GetTickCount() + (DWORD)timeoutSec * 1000;
        std::string result;
        while (GetTickCount() < deadline) {
            std::wstring j = GetMessagesJson(filter, false);
            if (j != L"[]") { result = ws2s(GetMessagesJson(filter, true)); break; }
            Sleep(250);
        }
        SendRestResp(s, 200, result.empty() ? "{\"ok\":false,\"empty\":true}" : result);
    } else {
        SendRestResp(s,404,"{\"error\":\"not found\"}");
    }
    closesocket(s);
}

static DWORD WINAPI RestApiThread(LPVOID) {
    g_restSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_restSock == INVALID_SOCKET) return 1;
    BOOL reuse = TRUE; setsockopt(g_restSock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    sockaddr_in addr = {}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(0x7f000001);
    int boundPort = 0;
    for (int p = g_restPort; p <= g_restPort + 5; ++p) {
        addr.sin_port = htons((u_short)p);
        if (bind(g_restSock, (sockaddr*)&addr, sizeof(addr)) == 0) { boundPort = p; break; }
    }
    if (!boundPort) { DebugLog(L"REST API: bind FAILED"); closesocket(g_restSock); g_restSock=INVALID_SOCKET; return 1; }
    g_restPort = boundPort;
    listen(g_restSock, SOMAXCONN);
    DebugLog(L"REST API: listening on 127.0.0.1:" + std::to_wstring(g_restPort));
    while (!g_stopThreads) {
        fd_set fds; FD_ZERO(&fds); FD_SET(g_restSock, &fds);
        timeval tv = {0,500000};
        if (select(0,&fds,nullptr,nullptr,&tv)<=0) continue;
        SOCKET client = accept(g_restSock,nullptr,nullptr);
        if (client == INVALID_SOCKET) continue;
        HandleRestRequest(client);
    }
    closesocket(g_restSock); g_restSock = INVALID_SOCKET;
    return 0;
}

// ---------------------------------------------------------------------------
// [21] UI Helpers
// ---------------------------------------------------------------------------
static void UpdatePeerList() {
    // Build combined list: pseudo users first, then discovered peers
    std::vector<std::tuple<std::wstring,std::wstring,std::wstring,bool>> items;
    EnterCriticalSection(&g_pseudoCs);
    for (auto& pu : g_pseudoUsers)
        items.push_back({pu.username, L"127.0.0.1", L"IpMsgP", true});
    LeaveCriticalSection(&g_pseudoCs);
    EnterCriticalSection(&g_peerCs);
    int peerCount = (int)g_peers.size();
    for (auto& p : g_peers) {
        std::wstring proto = (p.protocol == Proto::IPMsg) ? L"IPMsg" : L"LS";
        items.push_back({p.alias, p.ip, proto, false});
    }
    LeaveCriticalSection(&g_peerCs);

    int itemCount = (int)items.size();
    int listCount = ListView_GetItemCount(g_peerList);
    if (itemCount != listCount || peerCount > 0) {
        DebugLog(L"UI update: " + std::to_wstring(itemCount) + L" items ("
                 + std::to_wstring(peerCount) + L" peers, "
                 + std::to_wstring(listCount) + L" in listview)");
    }

    while (itemCount > listCount) {
        LVITEM lvi{}; lvi.mask = LVIF_TEXT; lvi.iItem = listCount; lvi.pszText = (LPWSTR)L"";
        ListView_InsertItem(g_peerList, &lvi); listCount++;
    }
    while (listCount > itemCount) { ListView_DeleteItem(g_peerList, --listCount); }
    // Count name occurrences to detect duplicates
    std::map<std::wstring,int> nameCount;
    for (auto& item : items) nameCount[std::get<0>(item)]++;
    for (int i = 0; i < itemCount; ++i) {
        std::wstring name = std::get<0>(items[i]);
        std::wstring ip   = std::get<1>(items[i]);
        if (nameCount[name] > 1)
            name += L"@" + ip;
        ListView_SetItemText(g_peerList, i, 0, (LPWSTR)name.c_str());
        ListView_SetItemText(g_peerList, i, 1, (LPWSTR)ip.c_str());
        ListView_SetItemText(g_peerList, i, 2, (LPWSTR)std::get<2>(items[i]).c_str());
    }
}

static void UpdateXferList() {
    EnterCriticalSection(&g_xferCs);
    std::vector<TransferEntry> xfers = g_transfers;
    LeaveCriticalSection(&g_xferCs);
    int cur = ListView_GetItemCount(g_xferList);
    while ((int)xfers.size() > cur) {
        LVITEM lvi{}; lvi.mask = LVIF_TEXT; lvi.iItem = cur; lvi.pszText = (LPWSTR)L"";
        ListView_InsertItem(g_xferList, &lvi); cur++;
    }
    while (cur > (int)xfers.size()) { ListView_DeleteItem(g_xferList, --cur); }
    for (int i = 0; i < (int)xfers.size(); ++i) {
        const wchar_t* dir = xfers[i].dir == XferDir::Inbound ? L"\u2193" : L"\u2191";
        ListView_SetItemText(g_xferList, i, 0, (LPWSTR)dir);
        ListView_SetItemText(g_xferList, i, 1, (LPWSTR)xfers[i].filename.c_str());
        ListView_SetItemText(g_xferList, i, 2, (LPWSTR)xfers[i].peerAlias.c_str());
        wchar_t prog[64];
        if      (xfers[i].state == XferState::Done)      wcscpy(prog, L"Done");
        else if (xfers[i].state == XferState::Failed)     wcscpy(prog, L"Failed");
        else if (xfers[i].state == XferState::Cancelled)  wcscpy(prog, L"Cancelled");
        else if (xfers[i].totalBytes > 0)
            swprintf(prog, 64, L"%d%%", (int)(xfers[i].doneBytes * 100 / xfers[i].totalBytes));
        else wcscpy(prog, L"Pending");
        ListView_SetItemText(g_xferList, i, 3, prog);
    }
}

static void DrawXferProgress(NMLVCUSTOMDRAW* cd) {
    if (cd->iSubItem != 3) return;
    int i = (int)cd->nmcd.dwItemSpec;
    int64_t total=0,done=0; XferState st=XferState::Pending;
    EnterCriticalSection(&g_xferCs);
    if (i<(int)g_transfers.size()) { total=g_transfers[i].totalBytes; done=g_transfers[i].doneBytes; st=g_transfers[i].state; }
    LeaveCriticalSection(&g_xferCs);
    if (total<=0) return;
    RECT rc; ListView_GetSubItemRect(g_xferList,i,3,LVIR_BOUNDS,&rc);
    int pct = (int)(done*100/total);
    RECT fill=rc; fill.right=rc.left+(rc.right-rc.left)*pct/100;
    COLORREF col=(st==XferState::Done)?RGB(0,180,80):RGB(0,120,215);
    HBRUSH bar=CreateSolidBrush(col); FillRect(cd->nmcd.hdc,&fill,bar); DeleteObject(bar);
    RECT rest=rc; rest.left=fill.right; FillRect(cd->nmcd.hdc,&rest,(HBRUSH)(COLOR_WINDOW+1));
    wchar_t txt[16]; swprintf(txt,16,L"%d%%",pct);
    SetBkMode(cd->nmcd.hdc,TRANSPARENT); DrawTextW(cd->nmcd.hdc,txt,-1,&rc,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
}

// ---------------------------------------------------------------------------
// [22] Layout
// ---------------------------------------------------------------------------
static void DoLayout(HWND hwnd) {
    RECT cr; GetClientRect(hwnd, &cr);
    int W = cr.right, H = cr.bottom;
    RECT sr; GetWindowRect(g_statusBar, &sr);
    int sh = sr.bottom - sr.top;
    int ch = H - sh;
    SendMessageW(g_statusBar, WM_SIZE, 0, 0);

    int divW  = 4, lw = g_dividerX, rw = W - lw - divW;
    int btnH = 26, nameH = 22, inputH = 200, rightX = lw + divW;
    int attachW = 80, sendW = lw - attachW;

    MoveWindow(g_reloadBtn,   0,0,                    lw, btnH, TRUE);
    MoveWindow(g_peerList,    0,btnH,                 lw, ch-btnH-inputH-btnH, TRUE);
    MoveWindow(g_textInput,   0,ch-inputH-btnH,       lw, inputH, TRUE);
    MoveWindow(g_attachBtn,   0,ch-btnH,              attachW, btnH, TRUE);
    MoveWindow(g_sendTextBtn, attachW,ch-btnH,        sendW, btnH, TRUE);

    MoveWindow(g_peerNameLabel, rightX, 0, rw, nameH, TRUE);

    int paneTop = nameH, paneH = ch - nameH, sepH = 5, minH = 30;
    if (g_split1Y < 0) g_split1Y = paneTop + paneH * 25 / 100;
    if (g_split2Y < 0) g_split2Y = paneTop + paneH * 55 / 100;

    if (g_split1Y < paneTop + minH) g_split1Y = paneTop + minH;
    if (g_split1Y > paneTop + paneH - 2 * (minH + sepH)) g_split1Y = paneTop + paneH - 2 * (minH + sepH);
    if (g_split2Y < g_split1Y + minH + sepH) g_split2Y = g_split1Y + minH + sepH;
    if (g_split2Y > paneTop + paneH - minH) g_split2Y = paneTop + paneH - minH;

    int h1 = g_split1Y - paneTop;
    int h2 = g_split2Y - (g_split1Y + sepH);
    int h3 = (paneTop + paneH) - (g_split2Y + sepH);

    MoveWindow(g_chatView, rightX, paneTop,         rw, h1, TRUE);
    MoveWindow(g_xferList, rightX, g_split1Y+sepH,  rw, h2, TRUE);
    MoveWindow(g_commLog,  rightX, g_split2Y+sepH,  rw, h3, TRUE);
}

static void QueueSend(const std::wstring& path, const PeerInfo& peer) {
    auto* sp       = new SendParams;
    sp->peerIp     = peer.ip;
    sp->peerPort   = peer.port;
    sp->localPath  = path;
    sp->filename   = PathFindFileNameW(path.c_str());
    sp->notifyHwnd = g_hwnd;

    TransferEntry te;
    te.id        = s2ws(MakeId());
    te.filename  = sp->filename;
    te.peerAlias = peer.alias;
    te.localPath = path;
    te.dir       = XferDir::Outbound;
    te.state     = XferState::Pending;

    EnterCriticalSection(&g_xferCs);
    g_transfers.push_back(te);
    sp->xferIdx = (int)g_transfers.size() - 1;
    LeaveCriticalSection(&g_xferCs);

    std::wstring chatLine = TimeStamp() + L" \u2191 Sending " + sp->filename + L" to " + peer.alias + L"...";
    AppendChat(chatLine);
    HANDLE h = CreateThread(nullptr, 0, SendFileThread, sp, 0, nullptr);
    if (h) CloseHandle(h); else delete sp;
    UpdateXferList();
}

static PeerInfo GetSelectedPeer() {
    int sel = ListView_GetNextItem(g_peerList, -1, LVNI_SELECTED);
    if (sel < 0) return {};
    EnterCriticalSection(&g_pseudoCs);
    int pcount = (int)g_pseudoUsers.size();
    LeaveCriticalSection(&g_pseudoCs);
    int peerIdx = sel - pcount;
    if (peerIdx < 0) return {}; // pseudo user selected, not a remote peer
    EnterCriticalSection(&g_peerCs);
    PeerInfo p = (peerIdx < (int)g_peers.size()) ? g_peers[peerIdx] : PeerInfo{};
    LeaveCriticalSection(&g_peerCs);
    return p;
}

static void UpdateAttachLabel() {
    if (!g_attachBtn) return;
    wchar_t label[64];
    if (g_pendingFiles.empty()) wcscpy(label, L"Attach");
    else swprintf(label, 64, L"Attach (%d)", (int)g_pendingFiles.size());
    SetWindowTextW(g_attachBtn, label);
}

static void DoAttach(HWND hwnd) {
    OPENFILENAMEW ofn{}; wchar_t buf[MAX_PATH*8] = {};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd; ofn.lpstrFile = buf;
    ofn.nMaxFile = (DWORD)(sizeof(buf)/sizeof(buf[0]));
    ofn.lpstrTitle = L"Attach file(s)";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return;
    wchar_t* p = buf; std::wstring dir = p; p += dir.size() + 1;
    if (*p == 0) g_pendingFiles.push_back(dir);
    else { while (*p) { std::wstring fn = p; p += fn.size()+1; g_pendingFiles.push_back(dir + L"\\" + fn); } }
    UpdateAttachLabel();
}

static void FlushAttachedFiles(const PeerInfo& peer) {
    for (auto& path : g_pendingFiles) QueueSend(path, peer);
    g_pendingFiles.clear(); UpdateAttachLabel();
}

static void DoSendFiles(HWND hwnd) {
    PeerInfo peer = GetSelectedPeer();
    if (peer.ip.empty()) { MessageBoxW(hwnd, L"Select a peer first.", L"LocalMsg", MB_ICONINFORMATION); return; }
    DoAttach(hwnd);
    FlushAttachedFiles(peer);
}

// ---------------------------------------------------------------------------
// [23] Window Procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        g_hwnd = hwnd;
        WSADATA wsa{}; WSAStartup(MAKEWORD(2,2), &wsa);
        InitializeCriticalSection(&g_peerCs);
        InitializeCriticalSection(&g_xferCs);
        InitializeCriticalSection(&g_pseudoCs);
        InitializeCriticalSection(&g_msgCs);

        DWORD sz = MAX_COMPUTERNAME_LENGTH + 1;
        GetComputerNameW(g_myHostname, &sz);

        ULONG bufLen = sizeof(IP_ADAPTER_INFO)*8;
        auto* buf = (IP_ADAPTER_INFO*)malloc(bufLen);
        wcscpy(g_localIp, L"0.0.0.0");
        uint64_t hash = 14695981039346656037ULL;
        if (buf && GetAdaptersInfo(buf, &bufLen)==ERROR_SUCCESS) {
            for (auto* a=buf; a; a=a->Next) {
                if (a->Type==MIB_IF_TYPE_LOOPBACK) continue;
                const char* ip = a->IpAddressList.IpAddress.String;
                if (strcmp(ip,"0.0.0.0")!=0) MultiByteToWideChar(CP_ACP,0,ip,-1,g_localIp,64);
                for (UINT i=0;i<a->AddressLength;++i) { hash ^= a->Address[i]; hash *= 1099511628211ULL; }
                break;
            }
        }
        free(buf);
        snprintf(g_fingerprint, sizeof(g_fingerprint), "%016llX", (unsigned long long)hash);
        DWORD sz2 = MAX_COMPUTERNAME_LENGTH+1;
        wchar_t host[MAX_COMPUTERNAME_LENGTH+1]={}; GetComputerNameW(host, &sz2);
        swprintf(g_localAlias, 256, L"ecode@%s", host);

        g_statusBar = CreateWindowExW(0, STATUSCLASSNAME, nullptr,
            WS_CHILD|WS_VISIBLE|SBARS_SIZEGRIP, 0,0,0,0, hwnd, (HMENU)IDC_STATUS, nullptr, nullptr);

        g_peerList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
            WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL,
            0,0,10,10, hwnd, (HMENU)IDC_PEER_LIST, nullptr, nullptr);
        ListView_SetExtendedListViewStyle(g_peerList, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        { LVCOLUMN c{}; c.mask=LVCF_TEXT|LVCF_WIDTH; c.cx=120; c.pszText=(LPWSTR)L"Name"; ListView_InsertColumn(g_peerList,0,&c);
          c.cx=130; c.pszText=(LPWSTR)L"Address"; ListView_InsertColumn(g_peerList,1,&c);
          c.cx=60;  c.pszText=(LPWSTR)L"Protocol";   ListView_InsertColumn(g_peerList,2,&c); }

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        g_reloadBtn = CreateWindowExW(0,L"BUTTON",L"Reload", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 0,0,10,10, hwnd, (HMENU)IDC_RELOAD_BTN, nullptr, nullptr);

        g_peerNameLabel = CreateWindowExW(WS_EX_CLIENTEDGE,L"STATIC",L"(no peer selected)", WS_CHILD|WS_VISIBLE|SS_LEFT|SS_CENTERIMAGE, 0,0,10,10, hwnd, (HMENU)IDC_PEER_NAME_LABEL, nullptr, nullptr);
        SendMessageW(g_peerNameLabel, WM_SETFONT, (WPARAM)hFont, FALSE);

        g_chatView = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",nullptr, WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL, 0,0,10,10, hwnd, (HMENU)IDC_CHAT_VIEW, nullptr, nullptr);
        SendMessageW(g_chatView, WM_SETFONT, (WPARAM)hFont, FALSE);

        g_textInput = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",nullptr, WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN, 0,0,10,10, hwnd, (HMENU)IDC_TEXT_INPUT, nullptr, nullptr);
        SendMessageW(g_textInput, WM_SETFONT, (WPARAM)hFont, FALSE);

        g_attachBtn = CreateWindowExW(0,L"BUTTON",L"Attach", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 0,0,10,10, hwnd, (HMENU)IDC_ATTACH_BTN, nullptr, nullptr);
        g_sendTextBtn = CreateWindowExW(0,L"BUTTON",L"Send", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 0,0,10,10, hwnd, (HMENU)IDC_SEND_TEXT_BTN, nullptr, nullptr);

        g_xferList = CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEW,nullptr, WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SHOWSELALWAYS, 0,0,10,10, hwnd, (HMENU)IDC_XFER_LIST, nullptr, nullptr);
        ListView_SetExtendedListViewStyle(g_xferList, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        { LVCOLUMN c{}; c.mask=LVCF_TEXT|LVCF_WIDTH; c.cx=22;  c.pszText=(LPWSTR)L"";         ListView_InsertColumn(g_xferList,0,&c);
          c.cx=160; c.pszText=(LPWSTR)L"File";     ListView_InsertColumn(g_xferList,1,&c);
          c.cx=110; c.pszText=(LPWSTR)L"Peer";     ListView_InsertColumn(g_xferList,2,&c);
          c.cx=90;  c.pszText=(LPWSTR)L"Progress"; ListView_InsertColumn(g_xferList,3,&c); }

        // Combined log (rich edit: comm + debug with [ls]/[ipmsg]/[other] tags)
        LoadLibraryW(L"Msftedit.dll");
        g_commLog = CreateWindowExW(WS_EX_CLIENTEDGE,L"RICHEDIT50W",nullptr, WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_NOHIDESEL, 0,0,10,10, hwnd, (HMENU)IDC_COMM_LOG, nullptr, nullptr);
        SendMessageW(g_commLog, WM_SETFONT, (WPARAM)hFont, FALSE);
        g_debugLog = g_commLog; // both feed into the same control

        DragAcceptFiles(hwnd, TRUE);
        g_acceptSem = CreateSemaphoreW(nullptr, LS_MAX_CONNS, LS_MAX_CONNS, nullptr);
        g_acceptThread = CreateThread(nullptr, 0, HttpAcceptThread, nullptr, 0, nullptr);
        g_discThread   = CreateThread(nullptr, 0, DiscoveryThread,  nullptr, 0, nullptr);
        CreateThread(nullptr, 0, [](LPVOID)->DWORD{ InitTls(); return 0; }, nullptr, 0, nullptr);
        g_ipmsgThread  = CreateThread(nullptr, 0, IpMsgThread, nullptr, 0, nullptr);
        g_restThread   = CreateThread(nullptr, 0, RestApiThread, nullptr, 0, nullptr);
        SetTimer(hwnd, 1, 500, nullptr);
        return 0;
    }

    case WM_SIZE: DoLayout(hwnd); return 0;

    case WM_LBUTTONDOWN: {
        int x=GET_X_LPARAM(lp), y=GET_Y_LPARAM(lp);
        if (abs(x-g_dividerX)<=5) { g_dragging=true; SetCapture(hwnd); }
        else if (x>g_dividerX+4) {
            if (abs(y-g_split1Y)<=5) { g_draggingSplit=1; SetCapture(hwnd); }
            else if (abs(y-g_split2Y)<=5) { g_draggingSplit=2; SetCapture(hwnd); }
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x=GET_X_LPARAM(lp), y=GET_Y_LPARAM(lp);
        if (g_dragging) { RECT rc; GetClientRect(hwnd,&rc); g_dividerX=ls_max<int>(100,ls_min<int>(x,rc.right-150)); DoLayout(hwnd); }
        else if (g_draggingSplit) {
            if (g_draggingSplit==1) g_split1Y=y;
            else g_split2Y=y;
            DoLayout(hwnd);
        } else {
            LPCWSTR cur = IDC_ARROW;
            if (abs(x-g_dividerX)<=5) cur=IDC_SIZEWE;
            else if (x>g_dividerX+4 && (abs(y-g_split1Y)<=5||abs(y-g_split2Y)<=5)) cur=IDC_SIZENS;
            SetCursor(LoadCursorW(nullptr,cur));
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (g_dragging) { g_dragging=false; ReleaseCapture(); }
        if (g_draggingSplit) { g_draggingSplit=0; ReleaseCapture(); }
        return 0;

    case WM_NOTIFY: {
        auto* hdr = (NMHDR*)lp;
        if (hdr->hwndFrom==g_peerList && hdr->code==LVN_ITEMCHANGED) {
            auto* nmlv=(NMLISTVIEW*)lp;
            if (nmlv->uNewState&LVIS_SELECTED) {
                // Get selected item from combined list (pseudo + peers)
                std::wstring selName;
                EnterCriticalSection(&g_pseudoCs);
                int pcount = (int)g_pseudoUsers.size();
                LeaveCriticalSection(&g_pseudoCs);
                int idx = nmlv->iItem;
                if (idx >= 0) {
                    if (idx < pcount) {
                        EnterCriticalSection(&g_pseudoCs);
                        selName = g_pseudoUsers[idx].username + L" [127.0.0.1]";
                        LeaveCriticalSection(&g_pseudoCs);
                    } else {
                        EnterCriticalSection(&g_peerCs);
                        int peerIdx = idx - pcount;
                        if (peerIdx < (int)g_peers.size())
                            selName = g_peers[peerIdx].alias + L" [" + g_peers[peerIdx].ip + L"]";
                        LeaveCriticalSection(&g_peerCs);
                    }
                }
                SetWindowTextW(g_peerNameLabel, selName.empty()?L"(no peer selected)":selName.c_str());
            }
            return 0;
        }
        if (hdr->code==NM_CUSTOMDRAW) {
            auto* cd=(NMLVCUSTOMDRAW*)lp;
            if (hdr->hwndFrom==g_xferList) {
                if (cd->nmcd.dwDrawStage==CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (cd->nmcd.dwDrawStage==CDDS_ITEMPREPAINT) return CDRF_NOTIFYSUBITEMDRAW;
                if (cd->nmcd.dwDrawStage==(CDDS_ITEMPREPAINT|CDDS_SUBITEM)) { DrawXferProgress(cd); return CDRF_DODEFAULT; }
            }
        }
        return CDRF_DODEFAULT;
    }

    case WM_CONTEXTMENU: {
        if ((HWND)wp==g_peerList) {
            HMENU hm=CreatePopupMenu();
            AppendMenuW(hm,MF_STRING,IDM_SEND_FILES,L"Send File(s)...");
            AppendMenuW(hm,MF_SEPARATOR,0,nullptr);
            AppendMenuW(hm,MF_STRING,IDM_CLEAR_CHAT,L"Clear Chat");
            TrackPopupMenu(hm,TPM_RIGHTBUTTON|TPM_TOPALIGN|TPM_LEFTALIGN, GET_X_LPARAM(lp),GET_Y_LPARAM(lp),0,hwnd,nullptr);
            DestroyMenu(hm);
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id==IDC_RELOAD_BTN) {
            g_announceNow=true;
            AnnouncePseudoUsers();
            DebugLog(L"[ls] Reload: re-sending multicast announces");
            return 0;
        }
        if (id==IDC_SEND_TEXT_BTN) {
            PeerInfo peer = GetSelectedPeer();
            if (peer.ip.empty()) { MessageBoxW(hwnd,L"Select a peer first.",L"LocalMsg",MB_ICONINFORMATION); return 0; }

                if (peer.protocol == Proto::IPMsg) {
                    wchar_t buf[4096]={}; GetWindowTextW(g_textInput,buf,4096);
                    if (!buf[0]) return 0;
                    std::wstring text = buf;
                    std::wstring fromUser = GetPrimaryUser();
                    if (fromUser.empty()) fromUser = g_myHostname;
                    if (SendTextIpMsg(fromUser, peer.alias, text)) {
                    AppendChat(TimeStamp()+L" "+fromUser+L" \u2192 "+peer.alias+L": "+text);
                    AppendCommLog(L"IPMSG SEND to "+peer.alias+L": "+text);
                } else { MessageBoxW(hwnd,L"Failed to send.",L"LocalMsg",MB_ICONERROR); }
                SetWindowTextW(g_textInput,L""); SetFocus(g_textInput);
            } else {
                // LocalSend: send text + attached files
                wchar_t buf[4096]={}; GetWindowTextW(g_textInput,buf,4096);
                if (buf[0]) {
                    std::wstring text = buf;
                    AppendChat(TimeStamp()+L" You \u2192 "+peer.alias+L": "+text);
                    AppendCommLog(L"SEND text to "+peer.alias+L" ("+peer.ip+L"): "+text.substr(0,80)+(text.size()>80?L"\x2026":L""));
                    auto* sp = new TextSendParams;
                    sp->peerIp=peer.ip; sp->peerPort=peer.port; sp->text=text; sp->senderAlias=g_localAlias; sp->notifyHwnd=hwnd;
                    HANDLE h = CreateThread(nullptr,0,SendTextThread,sp,0,nullptr);
                    if (h) CloseHandle(h); else delete sp;
                    SetWindowTextW(g_textInput,L"");
                }
                FlushAttachedFiles(peer);
                SetFocus(g_textInput);
            }
            return 0;
        }
        if (id==IDC_ATTACH_BTN) { DoAttach(hwnd); return 0; }
        if (id==IDM_SEND_FILES) { DoSendFiles(hwnd); return 0; }
        if (id==IDM_CLEAR_CHAT) { SetWindowTextW(g_chatView,L""); return 0; }
        return 0;
    }

    case WM_DROPFILES: {
        HDROP hd=(HDROP)wp; PeerInfo peer=GetSelectedPeer();
        UINT count=DragQueryFileW(hd,0xFFFFFFFF,nullptr,0);
        for (UINT i=0;i<count;++i) { wchar_t path[MAX_PATH]; DragQueryFileW(hd,i,path,MAX_PATH); if(!peer.ip.empty()) QueueSend(path,peer); }
        DragFinish(hd);
        if(peer.ip.empty()) MessageBoxW(hwnd,L"Select a peer first.",L"LocalMsg",MB_ICONINFORMATION);
        return 0;
    }

    case WM_TIMER: {
        DWORD now=GetTickCount(); bool changed=false;
        EnterCriticalSection(&g_peerCs);
        size_t before=g_peers.size();
        for (auto it = g_peers.begin(); it != g_peers.end(); ) {
            if ((now - it->lastSeenMs) > 120000) {
                DebugLog(L"Peer timeout (120s): " + it->alias + L" @" + it->ip + L" proto=" + (it->protocol==Proto::IPMsg?L"IPMsg":L"LS"));
                it = g_peers.erase(it);
            } else {
                ++it;
            }
        }
        if(g_peers.size()!=before) changed=true;
        LeaveCriticalSection(&g_peerCs);
        if(changed) UpdatePeerList();

        wchar_t s[256];
        swprintf(s,256,L"LS:%d IPMsg:%d | %d peers | REST 127.0.0.1:%d",
                 g_listenPort, IPMSG_PORT, (int)g_peers.size(), g_restPort);
        SendMessageW(g_statusBar,SB_SETTEXT,0,(LPARAM)s);
        return 0;
    }

    case WM_LS_PEER_FOUND: {
        auto* p=(PeerInfo*)lp;
        p->protocol = Proto::LocalSend;
        EnterCriticalSection(&g_peerCs);
        bool found=false;
        for(auto& e:g_peers) if(e.fingerprint==p->fingerprint) { e.lastSeenMs=p->lastSeenMs; e.alias=p->alias; e.ip=p->ip; found=true; break; }
    if(!found) { g_peers.push_back(*p); PostMessage(hwnd,WM_LS_APPEND_CHAT,0,(LPARAM)new std::wstring(L"\u2015\u2015 "+p->alias+L" joined \u2015\u2015")); AppendCommLog(L"LS peer found: "+p->alias+L" "+p->ip); DebugLog(L"LS peer added to list: " + p->alias + L" @" + p->ip); }
    else { AppendCommLog(L"LS peer seen: "+p->alias); DebugLog(L"LS peer updated: " + p->alias); }
    LeaveCriticalSection(&g_peerCs);
    delete p; UpdatePeerList(); return 0;
    }

    case WM_LS_PEER_GONE: {
        auto* fp=(std::wstring*)lp;
        std::wstring alias;
        EnterCriticalSection(&g_peerCs);
        for(auto& e:g_peers) if(e.fingerprint==*fp) { alias=e.alias; break; }
        g_peers.erase(std::remove_if(g_peers.begin(),g_peers.end(),[fp](const PeerInfo& p){return p.fingerprint==*fp;}),g_peers.end());
        LeaveCriticalSection(&g_peerCs);
        delete fp;
        if(!alias.empty()) AppendChat(L"\u2015\u2015 "+alias+L" left \u2015\u2015");
        UpdatePeerList(); return 0;
    }

    case WM_LS_XFER_PROGRESS: UpdateXferList(); return 0;

    case WM_LS_XFER_DONE: {
        int idx=(int)wp; bool ok=(lp!=0);
        EnterCriticalSection(&g_xferCs);
        std::wstring fn,peer; XferDir dir=XferDir::Outbound;
        if(idx<(int)g_transfers.size()) { fn=g_transfers[idx].filename; peer=g_transfers[idx].peerAlias; dir=g_transfers[idx].dir; }
        LeaveCriticalSection(&g_xferCs);
        const wchar_t* arrow=dir==XferDir::Inbound?L"\u2193":L"\u2191";
        wchar_t line[512]; swprintf(line,512,L"%s %s %s %s %s",TimeStamp().c_str(),arrow,fn.c_str(),dir==XferDir::Inbound?L"from":L"to",peer.c_str());
        AppendChat(std::wstring(line)+(ok?L" \u2014 Done":L" \u2014 Failed"));
        AppendCommLog(std::wstring(L"XFER ")+(ok?L"OK":L"FAIL")+L" "+fn+L" "+peer);
        UpdateXferList(); return 0;
    }

    case WM_LS_APPEND_CHAT: { auto* s=(std::wstring*)lp; AppendChat(*s); delete s; return 0; }

    case WM_LS_TEXT_RECEIVED: {
        auto* payload=(std::wstring*)lp;
        size_t sep=payload->find(L'\x01');
        std::wstring alias=sep==std::wstring::npos?L"?":payload->substr(0,sep);
        std::wstring text=sep==std::wstring::npos?*payload:payload->substr(sep+1);
        AppendChat(TimeStamp()+L" "+alias+L": "+text);
        delete payload; return 0;
    }

    case WM_IPMSG_TEXT_RCVD: {
        auto* payload=(std::wstring*)lp;
        size_t sep1=payload->find(L'\x01'); size_t sep2=payload->find(L'\x01',sep1+1);
        std::wstring from=(sep1==std::wstring::npos)?L"?":payload->substr(0,sep1);
        std::wstring to=(sep2==std::wstring::npos)?L"*":payload->substr(sep1+1,sep2-sep1-1);
        std::wstring text=(sep2==std::wstring::npos)?*payload:payload->substr(sep2+1);
        AppendChat(TimeStamp()+L" "+from+L" \u2192 "+to+L": "+text);
        AppendCommLog(L"IPMSG RECV from "+from+L": "+text);
        delete payload; return 0;
    }

    case WM_IPMSG_UPDATE_PEERS: UpdatePeerList(); return 0;

    case WM_LS_INBOUND_REQ: {
        auto* ir=(InboundReq*)lp;
        std::wstring dlg=L"Accept files from: "+ir->senderAlias+L"\n\nFiles:\n";
        for(auto& f:ir->files) dlg+=L"  "+f.filename+L"\n";
        ir->accepted=(MessageBoxW(hwnd,dlg.c_str(),L"Incoming Transfer",MB_YESNO|MB_ICONQUESTION)==IDYES);
        if(ir->accepted) {
            PWSTR dlPath=nullptr; SHGetKnownFolderPath(FOLDERID_Downloads,0,nullptr,&dlPath);
            std::wstring dlDir=dlPath?dlPath:L"C:\\"; if(dlPath) CoTaskMemFree(dlPath);
            std::string sessId=MakeId();
            std::string resp="{\"sessionId\":\""+sessId+"\",\"files\":{";
            EnterCriticalSection(&g_xferCs);
            for(size_t i=0;i<ir->files.size();++i) {
                auto& f=ir->files[i];
                if(i) resp+=",";
                resp+="\""+ws2s(f.id)+"\":\""+f.token+"\"";
                TransferEntry te; te.id=f.id; te.filename=f.filename; te.peerAlias=ir->senderAlias; te.localPath=dlDir+L"\\"+f.filename;
                te.totalBytes=f.size; te.dir=XferDir::Inbound; te.state=XferState::Pending;
                int idx=(int)g_transfers.size(); g_transfers.push_back(te); g_tokenToIdx[f.token]=idx;
            }
            LeaveCriticalSection(&g_xferCs);
            resp+="}}"; ir->responseBody=resp;
            AppendChat(L"\u2015\u2015 Accepting "+std::to_wstring(ir->files.size())+L" file(s) from "+ir->senderAlias+L" \u2015\u2015");
            UpdateXferList();
        }
        SetEvent(ir->hEvent); return 0;
    }

    case WM_LS_STATUS_TEXT: {
        auto* s=(std::wstring*)lp;
        SendMessageW(g_statusBar,SB_SETTEXT,0,(LPARAM)s->c_str());
        AppendCommLog(L"HTTP server: "+*s);
        delete s; return 0;
    }

    case WM_LS_APPEND_COMM:
    case WM_LS_APPEND_DEBUG: { auto* s=(std::wstring*)lp; AppendToEdit(g_commLog, *s); delete s; return 0; }

    case WM_DESTROY:
        g_stopThreads=true; KillTimer(hwnd,1);
        if(g_listenSock!=INVALID_SOCKET) closesocket(g_listenSock);
        if(g_udpSock!=INVALID_SOCKET) closesocket(g_udpSock);
        if(g_ipmsgSock!=INVALID_SOCKET) closesocket(g_ipmsgSock);
        if(g_restSock!=INVALID_SOCKET) closesocket(g_restSock);
        if(g_discThread) { WaitForSingleObject(g_discThread,3000); CloseHandle(g_discThread); }
        if(g_acceptThread) { WaitForSingleObject(g_acceptThread,3000); CloseHandle(g_acceptThread); }
        if(g_ipmsgThread) { WaitForSingleObject(g_ipmsgThread,3000); CloseHandle(g_ipmsgThread); }
        if(g_restThread) { WaitForSingleObject(g_restThread,3000); CloseHandle(g_restThread); }
        if(g_acceptSem) CloseHandle(g_acceptSem);
        DeleteCriticalSection(&g_peerCs); DeleteCriticalSection(&g_xferCs);
        DeleteCriticalSection(&g_pseudoCs); DeleteCriticalSection(&g_msgCs);
        if (g_pendInit) DeleteCriticalSection(&g_pendCs);
        FreeTls(); WSACleanup(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// [24] CLI Dispatch & wWinMain
// ---------------------------------------------------------------------------
static void PrintJson(const std::string& s) {
    DWORD written=0; HANDLE hOut=GetStdHandle(STD_OUTPUT_HANDLE);
    if(hOut&&hOut!=INVALID_HANDLE_VALUE) { std::string out=s+"\n"; WriteFile(hOut,out.c_str(),(DWORD)out.size(),&written,nullptr); }
}

static int RunCLI(int argc, wchar_t** argv) {
    auto TryApi = [&](const std::string& method, const std::string& path, const std::string& body) -> std::string {
        HINTERNET hSess=WinHttpOpen(L"LocalMsgCLI/1.0",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
        if(!hSess) return "{\"error\":\"WinHttpOpen failed\"}";
        HINTERNET hConn=WinHttpConnect(hSess,L"127.0.0.1",(INTERNET_PORT)g_restPort,0);
        if(!hConn) { WinHttpCloseHandle(hSess); return "{\"error\":\"API not running\"}"; }
        HINTERNET hReq=WinHttpOpenRequest(hConn,s2ws(method).c_str(),s2ws(path).c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0);
        if(!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return "{\"error\":\"OpenRequest failed\"}"; }
        std::wstring hdrs=L"Content-Type: application/json\r\n";
        WinHttpAddRequestHeaders(hReq,hdrs.c_str(),(DWORD)-1,WINHTTP_ADDREQ_FLAG_ADD);
        DWORD bl=(DWORD)body.size();
        bool sent=WinHttpSendRequest(hReq,WINHTTP_NO_ADDITIONAL_HEADERS,0,body.empty()?WINHTTP_NO_REQUEST_DATA:(LPVOID)body.c_str(),bl,bl,0);
        std::string result;
        if(sent&&WinHttpReceiveResponse(hReq,nullptr)) {
            DWORD avail=0;
            while(WinHttpQueryDataAvailable(hReq,&avail)&&avail>0) { std::string buf(avail,0); DWORD read=0; WinHttpReadData(hReq,&buf[0],avail,&read); buf.resize(read); result+=buf; }
        } else result="{\"error\":\"API call failed\"}";
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
        return result;
    };

    std::string cmd=argc>1?ws2s(argv[1]):"";
    if(cmd=="--login"&&argc>=3) { PrintJson(TryApi("POST","/api/login","{\"username\":\""+EscapeJson(ws2s(argv[2]))+"\"}")); return 0; }
    if(cmd=="--logout"&&argc>=3) { PrintJson(TryApi("POST","/api/logout","{\"username\":\""+EscapeJson(ws2s(argv[2]))+"\"}")); return 0; }
    if(cmd=="--send"&&argc>=4) {
        std::string from,to,text;
        if(argc>=5) { from=ws2s(argv[2]); to=ws2s(argv[3]); text=ws2s(argv[4]); }
        else { from=ws2s(GetPrimaryUser()); if (from.empty()) from=ws2s(g_myHostname); to=ws2s(argv[2]); text=ws2s(argv[3]); }
        PrintJson(TryApi("POST","/api/send","{\"from\":\""+EscapeJson(from)+"\",\"to\":\""+EscapeJson(to)+"\",\"text\":\""+EscapeJson(text)+"\"}"));
        return 0;
    }
    if(cmd=="--list") { PrintJson(TryApi("GET","/api/users","")); return 0; }
    if(cmd=="--receive"&&argc>=3) {
        bool peek = (argc>=4 && ws2s(argv[3])=="--peek");
        std::string path = "/api/messages?user="+ws2s(argv[2]);
        if (peek) path += "&peek";
        PrintJson(TryApi("GET",path,"")); return 0;
    }
    PrintJson("{\"error\":\"Usage: --login <user> | --logout <user> | --send [<from>] <to> <text> | --list | --receive <user>\"}");
    return 1;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    int argc=0; LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argc>1&&argv) {
        std::wstring a1=argv[1];
        if(a1==L"--login"||a1==L"--logout"||a1==L"--send"||a1==L"--list"||a1==L"--receive") { int r=RunCLI(argc,argv); if(argv) LocalFree(argv); return r; }
    }
    if(argv) LocalFree(argv);

    INITCOMMONCONTROLSEX ice{sizeof(ice),ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES};
    InitCommonControlsEx(&ice);
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.style=CS_HREDRAW|CS_VREDRAW; wc.lpfnWndProc=WndProc;
    wc.hInstance=hInst; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName=L"EcodeLocalMsgWindow"; RegisterClassExW(&wc);
    HWND hwnd=CreateWindowExW(0,L"EcodeLocalMsgWindow",L"LocalMsg - IPMsg + LocalSend",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,900,600,nullptr,nullptr,hInst,nullptr);
    if(!hwnd) return 1;
    ShowWindow(hwnd,nCmdShow); UpdateWindow(hwnd);
    MSG m;
    while(GetMessageW(&m,nullptr,0,0)) {
        if(m.hwnd==g_textInput&&m.message==WM_KEYDOWN&&m.wParam==VK_RETURN&&(GetKeyState(VK_CONTROL)&0x8000)) { SendMessageW(g_hwnd,WM_COMMAND,IDC_SEND_TEXT_BTN,0); continue; }
        TranslateMessage(&m); DispatchMessageW(&m);
    }
    return (int)m.wParam;
}
