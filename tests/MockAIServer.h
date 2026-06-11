#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>

// ── URL encode / decode ─────────────────────────────────────────────

static inline std::string MockAI_UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped << std::hex;
    for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << (char)c;
        } else {
            escaped << '%' << std::setw(2) << std::setfill('0') << (int)c;
        }
    }
    return escaped.str();
}

static inline std::string MockAI_UrlDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            int hexVal = 0;
            std::istringstream hs(value.substr(i + 1, 2));
            if (hs >> std::hex >> hexVal) { result += (char)hexVal; i += 2; }
            else { result += '%'; }
        } else if (value[i] == '+') {
            result += ' ';
        } else {
            result += value[i];
        }
    }
    return result;
}

// ── Base64 ─────────────────────────────────────────────────────────

static inline std::string MockAI_Base64Encode(const std::string& in) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c; valb += 8;
        while (valb >= 0) { out.push_back(T[(val >> valb) & 0x3F]); valb -= 6; }
    }
    if (valb > -6) out.push_back(T[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

static inline std::string MockAI_Base64Decode(const std::string& in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++)
        T[(unsigned char)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c]; valb += 6;
        if (valb >= 0) { out.push_back((char)((val >> valb) & 0xFF)); valb -= 8; }
    }
    return out;
}

// ── Multipart parser ───────────────────────────────────────────────

struct MockAI_MultipartPart {
    std::string name;
    std::string filename;
    std::string contentType;
    std::string data;
};

static inline std::vector<MockAI_MultipartPart> MockAI_ParseMultipart(
    const std::string& body, const std::string& boundary)
{
    std::vector<MockAI_MultipartPart> parts;
    std::string delim = "--" + boundary;
    size_t pos = 0;
    while (true) {
        size_t start = body.find(delim, pos);
        if (start == std::string::npos) break;
        start += delim.size();
        if (start + 2 <= body.size() && body.substr(start, 2) == "--") break;
        if (start + 2 <= body.size() && body.substr(start, 2) == "\r\n") start += 2;
        else break;
        size_t next = body.find(delim, start);
        if (next == std::string::npos) break;
        std::string block = body.substr(start, next - start - 2);
        pos = next;

        size_t headerEnd = block.find("\r\n\r\n");
        if (headerEnd == std::string::npos) continue;
        std::string hdrs = block.substr(0, headerEnd);
        MockAI_MultipartPart part;
        part.data = block.substr(headerEnd + 4);

        size_t dp = hdrs.find("Content-Disposition:");
        if (dp != std::string::npos) {
            size_t np = hdrs.find("name=\"", dp);
            if (np != std::string::npos && np < hdrs.find("\r\n", dp)) {
                np += 6; part.name = hdrs.substr(np, hdrs.find('"', np) - np);
            }
            size_t fp = hdrs.find("filename=\"", dp);
            if (fp != std::string::npos && fp < hdrs.find("\r\n", dp)) {
                fp += 10; part.filename = hdrs.substr(fp, hdrs.find('"', fp) - fp);
            }
        }
        size_t cp = hdrs.find("Content-Type:");
        if (cp != std::string::npos) {
            size_t cs = cp + 13;
            while (cs < hdrs.size() && hdrs[cs] == ' ') cs++;
            size_t ce = hdrs.find("\r\n", cs);
            part.contentType = hdrs.substr(cs, ce - cs);
        }
        parts.push_back(part);
    }
    return parts;
}

// ── MockHTTPAIServer ───────────────────────────────────────────────
//
// Lightweight in-process HTTP server for testing AI client code.
// Starts a background thread on loopback; use GetPort() to connect.
//
// Endpoints:
//   POST /recipe/text-to-text          query: ?prompt=...  or JSON {"prompt":"..."}
//   POST /recipe/image-to-image        multipart: field "image"
//   POST /recipe/multi-image-to-image  multipart: field "fixed_image" + "input_images"[]
//
// Usage:
//   MockHTTPAIServer srv;
//   srv.Start();           // binds to a random loopback port
//   int port = srv.GetPort();
//   // ... run tests that connect to 127.0.0.1:port ...
//   srv.Stop();

class MockHTTPAIServer {
public:
    MockHTTPAIServer() : m_listenSock(INVALID_SOCKET), m_port(0), m_running(false), m_hThread(nullptr) {}
    ~MockHTTPAIServer() { Stop(); }

    bool Start(int port = 0) {
        m_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSock == INVALID_SOCKET) return false;

        sockaddr_in addr = {};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = htons((u_short)port);

        if (bind(m_listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(m_listenSock); return false;
        }
        int len = sizeof(addr);
        if (getsockname(m_listenSock, (sockaddr*)&addr, &len) == 0)
            m_port = ntohs(addr.sin_port);
        if (listen(m_listenSock, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(m_listenSock); return false;
        }

        m_running = true;
        m_hThread = CreateThread(nullptr, 0, ServerThreadProc, this, 0, nullptr);
        return m_hThread != nullptr;
    }

    void Stop() {
        m_running = false;
        if (m_listenSock != INVALID_SOCKET) {
            closesocket(m_listenSock);
            m_listenSock = INVALID_SOCKET;
        }
        if (m_hThread) {
            WaitForSingleObject(m_hThread, INFINITE);
            CloseHandle(m_hThread);
            m_hThread = nullptr;
        }
    }

    int GetPort() const { return m_port; }

private:
    static DWORD WINAPI ServerThreadProc(LPVOID p) {
        static_cast<MockHTTPAIServer*>(p)->ServerLoop(); return 0;
    }

    void ServerLoop() {
        while (m_running) {
            fd_set fds; FD_ZERO(&fds); FD_SET(m_listenSock, &fds);
            timeval tv = {0, 100000};
            if (select(0, &fds, nullptr, nullptr, &tv) <= 0) continue;
            SOCKET client = accept(m_listenSock, nullptr, nullptr);
            if (client != INVALID_SOCKET) HandleClient(client);
        }
    }

    void HandleClient(SOCKET s) {
        std::string req;
        char buf[4096];
        int bytes;
        while ((bytes = recv(s, buf, sizeof(buf) - 1, 0)) > 0) {
            buf[bytes] = '\0';
            req += buf;
            if (req.find("\r\n\r\n") != std::string::npos) {
                size_t clPos = req.find("Content-Length:");
                if (clPos != std::string::npos) {
                    int cl = atoi(req.c_str() + clPos + 15);
                    size_t bodyPos = req.find("\r\n\r\n") + 4;
                    if (req.size() - bodyPos >= (size_t)cl) break;
                } else {
                    break;
                }
            }
        }

        size_t fle = req.find("\r\n");
        if (fle == std::string::npos) { closesocket(s); return; }
        std::string fl = req.substr(0, fle);
        size_t s1 = fl.find(' '), s2 = fl.find(' ', s1 + 1);
        if (s1 == std::string::npos || s2 == std::string::npos) { closesocket(s); return; }

        std::string path = fl.substr(s1 + 1, s2 - s1 - 1);
        size_t bodyPos = req.find("\r\n\r\n") + 4;
        std::string body = req.substr(bodyPos);

        // Parse headers
        std::string hdrsPart = req.substr(fle + 2, bodyPos - fle - 6);
        std::map<std::string, std::string> hdrs;
        for (size_t hp = 0; hp < hdrsPart.size(); ) {
            size_t nl = hdrsPart.find("\r\n", hp);
            if (nl == std::string::npos) nl = hdrsPart.size();
            std::string line = hdrsPart.substr(hp, nl - hp);
            hp = nl + 2;
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string k = line.substr(0, colon);
            std::string v = line.substr(colon + 1);
            while (!v.empty() && v.front() == ' ') v.erase(v.begin());
            for (char& c : k) c = (char)tolower((unsigned char)c);
            hdrs[k] = v;
        }

        // Parse query string
        std::map<std::string, std::string> query;
        size_t qp = path.find('?');
        if (qp != std::string::npos) {
            std::string qs = path.substr(qp + 1);
            path = path.substr(0, qp);
            for (size_t p2 = 0; p2 < qs.size(); ) {
                size_t amp = qs.find('&', p2);
                if (amp == std::string::npos) amp = qs.size();
                std::string kv = qs.substr(p2, amp - p2);
                p2 = amp + 1;
                size_t eq = kv.find('=');
                if (eq != std::string::npos)
                    query[MockAI_UrlDecode(kv.substr(0, eq))] = MockAI_UrlDecode(kv.substr(eq + 1));
            }
        }

        std::string respBody;
        if (path == "/recipe/text-to-text") {
            std::string prompt = "Default prompt";
            if (query.count("prompt")) {
                prompt = query["prompt"];
            } else {
                size_t pp = body.find("\"prompt\"");
                if (pp != std::string::npos) {
                    size_t col = body.find(':', pp);
                    size_t q1  = body.find('"', col + 1);
                    size_t q2  = body.find('"', q1 + 1);
                    prompt = body.substr(q1 + 1, q2 - q1 - 1);
                }
            }
            respBody = "{\"output\": \"Response to: " + prompt + "\"}";
        }
        else if (path == "/recipe/image-to-image") {
            std::string boundary = GetBoundary(hdrs);
            std::string imageB64;
            if (!boundary.empty()) {
                for (auto& p : MockAI_ParseMultipart(body, boundary))
                    if (p.name == "image") { imageB64 = MockAI_Base64Encode(p.data); break; }
            }
            respBody = "{\"output_image\": \"processed:" + imageB64 + "\"}";
        }
        else if (path == "/recipe/multi-image-to-image") {
            std::string boundary = GetBoundary(hdrs);
            std::string fixedB64;
            std::vector<std::string> inputsB64;
            if (!boundary.empty()) {
                for (auto& p : MockAI_ParseMultipart(body, boundary)) {
                    if (p.name == "fixed_image") fixedB64 = MockAI_Base64Encode(p.data);
                    else if (p.name == "input_images") inputsB64.push_back(MockAI_Base64Encode(p.data));
                }
            }
            std::string result = fixedB64;
            if (!inputsB64.empty()) result += "_" + inputsB64[0];
            respBody = "{\"output_image\": \"processed:" + result + "\"}";
        }
        else {
            respBody = "{\"error\": \"not_found\"}";
        }

        std::string resp = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(respBody.size()) + "\r\n"
                           "Connection: close\r\n\r\n" + respBody;
        send(s, resp.data(), (int)resp.size(), 0);
        closesocket(s);
    }

    static std::string GetBoundary(const std::map<std::string, std::string>& hdrs) {
        auto it = hdrs.find("content-type");
        if (it == hdrs.end()) return "";
        size_t bp = it->second.find("boundary=");
        return bp != std::string::npos ? it->second.substr(bp + 9) : "";
    }

    SOCKET  m_listenSock;
    int     m_port;
    bool    m_running;
    HANDLE  m_hThread;
};

// ── MockAIHTTPClient ───────────────────────────────────────────────
//
// HTTP client that talks to MockHTTPAIServer (or any compatible server).
// Named MockAIHTTPClient to avoid confusion with the JS-side MockAIProvider.

class MockAIHTTPClient {
public:
    explicit MockAIHTTPClient(int port) : m_port(port) {}

    // POST /recipe/text-to-text?prompt=<prompt>
    // Returns the "output" field from the JSON response.
    std::string TextToText(const std::string& prompt) {
        std::string path = "/recipe/text-to-text?prompt=" + MockAI_UrlEncode(prompt);
        std::string resp = Send("POST", path, "application/json", "{}");
        return ExtractJsonString(resp, "output");
    }

    // POST /recipe/image-to-image  (multipart, field "image")
    // Returns the decoded output image bytes.
    std::string ImageToImage(const std::string& imageRaw) {
        std::string bnd = "----MockAIBnd12345";
        std::string body;
        body += "--" + bnd + "\r\n";
        body += "Content-Disposition: form-data; name=\"image\"; filename=\"input.png\"\r\n";
        body += "Content-Type: image/png\r\n\r\n";
        body += imageRaw + "\r\n";
        body += "--" + bnd + "--\r\n";
        std::string resp = Send("POST", "/recipe/image-to-image",
                                "multipart/form-data; boundary=" + bnd, body);
        std::string b64 = ExtractJsonString(resp, "output_image");
        if (b64.find("processed:") == 0) b64 = b64.substr(10);
        return MockAI_Base64Decode(b64);
    }

    // POST /recipe/multi-image-to-image  (multipart, fields "fixed_image" + "input_images"[])
    // Returns the decoded output image bytes.
    std::string MultiImageToImage(const std::string& fixedRaw,
                                  const std::vector<std::string>& inputsRaw)
    {
        std::string bnd = "----MockAIBnd67890";
        std::string body;
        body += "--" + bnd + "\r\n";
        body += "Content-Disposition: form-data; name=\"fixed_image\"; filename=\"fixed.png\"\r\n";
        body += "Content-Type: image/png\r\n\r\n";
        body += fixedRaw + "\r\n";
        for (size_t i = 0; i < inputsRaw.size(); i++) {
            body += "--" + bnd + "\r\n";
            body += "Content-Disposition: form-data; name=\"input_images\"; filename=\"input_"
                  + std::to_string(i) + ".png\"\r\n";
            body += "Content-Type: image/png\r\n\r\n";
            body += inputsRaw[i] + "\r\n";
        }
        body += "--" + bnd + "--\r\n";
        std::string resp = Send("POST", "/recipe/multi-image-to-image",
                                "multipart/form-data; boundary=" + bnd, body);
        std::string b64 = ExtractJsonString(resp, "output_image");
        if (b64.find("processed:") == 0) b64 = b64.substr(10);
        return MockAI_Base64Decode(b64);
    }

    // Low-level HTTP send — returns response body only.
    std::string Send(const std::string& method, const std::string& path,
                     const std::string& contentType, const std::string& body)
    {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) return "";
        sockaddr_in addr = {};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = htons((u_short)m_port);
        if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(s); return "";
        }
        std::string req = method + " " + path + " HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Content-Type: " + contentType + "\r\n"
                          "Content-Length: " + std::to_string(body.size()) + "\r\n"
                          "Connection: close\r\n\r\n" + body;
        send(s, req.data(), (int)req.size(), 0);
        std::string resp;
        char buf[4096];
        int r;
        while ((r = recv(s, buf, sizeof(buf) - 1, 0)) > 0) { buf[r] = '\0'; resp += buf; }
        closesocket(s);
        size_t bp = resp.find("\r\n\r\n");
        return bp != std::string::npos ? resp.substr(bp + 4) : "";
    }

private:
    static std::string ExtractJsonString(const std::string& json, const std::string& key) {
        size_t kp = json.find("\"" + key + "\"");
        if (kp == std::string::npos) return "";
        size_t col = json.find(':', kp);
        size_t q1  = json.find('"', col + 1);
        size_t q2  = json.find('"', q1 + 1);
        return json.substr(q1 + 1, q2 - q1 - 1);
    }

    int m_port;
};
