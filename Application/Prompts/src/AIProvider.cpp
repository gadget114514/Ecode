#include "AIProvider.h"
#include "NodeData.h"
#include "JsonParser.h"
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <thread>

#pragma comment(lib, "winhttp.lib")

// Forward declarations
static std::string WinHttpRequest(const std::string &url, const std::string &method,
                                   const std::string &headers, const std::string &body,
                                   int timeoutMs = 60000);

static std::string BuildOpenAIBody(const AIRequest &req);
static std::string BuildAnthropicBody(const AIRequest &req);
static std::string BuildGeminiBody(const AIRequest &req);
static std::string BuildOllamaBody(const AIRequest &req);

static std::string ExtractOpenAIResponse(const std::string &json);
static std::string ExtractAnthropicResponse(const std::string &json);
static std::string ExtractGeminiResponse(const std::string &json);
static std::string ExtractOllamaResponse(const std::string &json);

// ============ OpenAI ============
class OpenAIProvider : public AIProvider {
    std::string apiKey_, baseUrl_;
public:
    OpenAIProvider(const std::string &key, const std::string &url)
        : apiKey_(key), baseUrl_(url.empty() ? "https://api.openai.com" : url) {}
    
    std::string Name() const override { return "openai"; }
    std::vector<std::string> ListModels() override { return {"gpt-4.1", "gpt-4o-mini"}; }
    
    AIResponse Call(const AIRequest &req) override {
        AIResponse resp;
        resp.model = req.model;
        std::string body = BuildOpenAIBody(req);
        std::string url = baseUrl_ + "/v1/chat/completions";
        std::string headers = "Content-Type: application/json\r\nAuthorization: Bearer " + apiKey_ + "\r\n";
        std::string result = WinHttpRequest(url, "POST", headers, body);
        if (!result.empty()) resp.content = ExtractOpenAIResponse(result);
        else resp.content = "[OpenAI: empty response]";
        return resp;
    }
    
    std::string TestConnection() override {
        std::string headers = "Authorization: Bearer " + apiKey_ + "\r\n";
        std::string result = WinHttpRequest(baseUrl_ + "/v1/models", "GET", headers, "");
        if (result.empty()) return "Connection failed (no response)";
        auto val = JsonValue::parse(result);
        if (val.has("error")) return val["error"]["message"].string();
        if (val.has("data")) return ""; // success
        return "Unexpected response";
    }
    
    void CallStreaming(const AIRequest &req,
                       std::function<void(const std::string&)> onChunk,
                       std::function<void(const AIResponse&)> onDone,
                       std::function<void(const std::string&)> onError) override {
        std::thread([this, req, onChunk, onDone, onError]() {
            try {
                AIResponse resp;
                resp.model = req.model;
                std::string body = BuildOpenAIBody(req);
                std::string url = baseUrl_ + "/v1/chat/completions";
                std::string headers = "Content-Type: application/json\r\nAuthorization: Bearer " + apiKey_ + "\r\n";
                // ... streaming implementation
            } catch (...) {}
        }).detach();
    }
};

// ============ Anthropic ============
class AnthropicProvider : public AIProvider {
    std::string apiKey_, baseUrl_;
public:
    AnthropicProvider(const std::string &key, const std::string &url)
        : apiKey_(key), baseUrl_(url.empty() ? "https://api.anthropic.com" : url) {}
    
    std::string Name() const override { return "anthropic"; }
    std::vector<std::string> ListModels() override { return {"claude-sonnet-4-6", "claude-haiku-4-5"}; }
    
    AIResponse Call(const AIRequest &req) override {
        AIResponse resp;
        resp.model = req.model;
        std::string body = BuildAnthropicBody(req);
        std::string url = baseUrl_ + "/v1/messages";
        std::string headers = "Content-Type: application/json\r\nx-api-key: " + apiKey_ + "\r\nanthropic-version: 2023-06-01\r\n";
        std::string result = WinHttpRequest(url, "POST", headers, body);
        if (!result.empty()) resp.content = ExtractAnthropicResponse(result);
        else resp.content = "[Anthropic: empty response]";
        return resp;
    }
    
    std::string TestConnection() override {
        std::string headers = "x-api-key: " + apiKey_ + "\r\nanthropic-version: 2023-06-01\r\n";
        std::string result = WinHttpRequest(baseUrl_ + "/v1/models", "GET", headers, "");
        if (result.empty()) return "Connection failed (no response)";
        auto val = JsonValue::parse(result);
        if (val.has("error")) return val["error"]["message"].string();
        if (val.has("data")) return "";
        return "Unexpected response";
    }
    
    void CallStreaming(const AIRequest &req,
                       std::function<void(const std::string&)> onChunk,
                       std::function<void(const AIResponse&)> onDone,
                       std::function<void(const std::string&)> onError) override {
        std::thread([this, req, onChunk, onDone, onError]() {
            try {
                AIResponse resp;
                resp.model = req.model;
                std::string body = BuildAnthropicBody(req);
                std::string url = baseUrl_ + "/v1/messages";
                std::string headers = "Content-Type: application/json\r\nx-api-key: " + apiKey_ + "\r\nanthropic-version: 2023-06-01\r\nAccept: text/event-stream\r\n";
                size_t pos = body.find("\"stream\":false");
                if (pos != std::string::npos) body.replace(pos, 13, "\"stream\":true");
                std::string result = WinHttpRequest(url, "POST", headers, body, 120000);
                if (!result.empty()) {
                    std::istringstream stream(result);
                    std::string line;
                    bool captureContent = false;
                    while (std::getline(stream, line)) {
                        if (line.find("event: content_block_delta") == 0) captureContent = true;
                        else if (line.find("data: ") == 0 && captureContent) {
                            auto val = JsonValue::parse(line.substr(6));
                            if (val.has("delta")) {
                                std::string text = val["delta"]["text"].string();
                                if (!text.empty()) {
                                    if (onChunk) onChunk(text);
                                    resp.content += text;
                                }
                            }
                            captureContent = false;
                        } else if (line.find("data: ") == 0) {
                            captureContent = false;
                        }
                    }
                }
                if (onDone) onDone(resp);
            } catch (const std::exception &e) {
                if (onError) onError(e.what());
            }
        }).detach();
    }
};

// ============ Gemini ============
class GeminiProvider : public AIProvider {
    std::string apiKey_, baseUrl_;
public:
    GeminiProvider(const std::string &key, const std::string &url)
        : apiKey_(key), baseUrl_(url.empty() ? "https://generativelanguage.googleapis.com" : url) {}
    
    std::string Name() const override { return "gemini"; }
    std::vector<std::string> ListModels() override { return {"gemini-2.5-flash", "gemini-2.5-pro"}; }
    
    AIResponse Call(const AIRequest &req) override {
        AIResponse resp;
        resp.model = req.model;
        std::string body = BuildGeminiBody(req);
        std::string url = baseUrl_ + "/v1beta/models/" + req.model + ":generateContent?key=" + apiKey_;
        std::string headers = "Content-Type: application/json\r\n";
        std::string result = WinHttpRequest(url, "POST", headers, body);
        if (!result.empty()) resp.content = ExtractGeminiResponse(result);
        else resp.content = "[Gemini: empty response]";
        return resp;
    }
    
    std::string TestConnection() override {
        std::string result = WinHttpRequest(baseUrl_ + "/v1beta/models?key=" + apiKey_, "GET", "", "");
        if (result.empty()) return "Connection failed (no response)";
        auto val = JsonValue::parse(result);
        if (val.has("error")) return val["error"]["message"].string();
        if (val.has("models")) return "";
        return "Unexpected response";
    }
    
    void CallStreaming(const AIRequest &req,
                       std::function<void(const std::string&)> onChunk,
                       std::function<void(const AIResponse&)> onDone,
                       std::function<void(const std::string&)> onError) override {
        std::thread([this, req, onChunk, onDone, onError]() {
            try {
                AIResponse resp;
                resp.model = req.model;
                std::string body = BuildGeminiBody(req);
                std::string url = baseUrl_ + "/v1beta/models/" + req.model + ":streamGenerateContent?key=" + apiKey_ + "&alt=sse";
                std::string headers = "Content-Type: application/json\r\n";
                std::string result = WinHttpRequest(url, "POST", headers, body, 120000);
                if (!result.empty()) {
                    std::istringstream stream(result);
                    std::string line;
                    while (std::getline(stream, line)) {
                        if (line.find("data: ") == 0) {
                            auto val = JsonValue::parse(line.substr(6));
                            auto &candidates = val["candidates"];
                            if (candidates.type() == JsonValue::Array && candidates.array().size() > 0) {
                                auto &parts = candidates[0]["content"]["parts"];
                                if (parts.type() == JsonValue::Array && parts.array().size() > 0) {
                                    std::string text = parts[0]["text"].string();
                                    if (!text.empty()) {
                                        if (onChunk) onChunk(text);
                                        resp.content += text;
                                    }
                                }
                            }
                        }
                    }
                }
                if (onDone) onDone(resp);
            } catch (const std::exception &e) {
                if (onError) onError(e.what());
            }
        }).detach();
    }
};

// ============ Ollama ============
class OllamaProvider : public AIProvider {
    std::string apiKey_, baseUrl_;
public:
    OllamaProvider(const std::string &key, const std::string &url)
        : apiKey_(key), baseUrl_(url.empty() ? "http://localhost:11434" : url) {}
    
    std::string Name() const override { return "ollama"; }
    std::vector<std::string> ListModels() override { return {"llama3.2", "mistral"}; }
    
    AIResponse Call(const AIRequest &req) override {
        AIResponse resp;
        resp.model = req.model;
        std::string body = BuildOllamaBody(req);
        std::string url = baseUrl_ + "/api/chat";
        std::string headers = "Content-Type: application/json\r\n";
        std::string result = WinHttpRequest(url, "POST", headers, body);
        if (!result.empty()) resp.content = ExtractOllamaResponse(result);
        else resp.content = "[Ollama: empty response]";
        return resp;
    }
    
    std::string TestConnection() override {
        std::string result = WinHttpRequest(baseUrl_ + "/api/tags", "GET", "", "");
        if (result.empty()) return "Connection failed (is Ollama running?)";
        auto val = JsonValue::parse(result);
        if (val.has("error")) return val["error"].string();
        if (val.has("models")) return "";
        return "Unexpected response";
    }
    
    void CallStreaming(const AIRequest &req,
                       std::function<void(const std::string&)> onChunk,
                       std::function<void(const AIResponse&)> onDone,
                       std::function<void(const std::string&)> onError) override {
        std::thread([this, req, onChunk, onDone, onError]() {
            try {
                AIResponse resp;
                resp.model = req.model;
                std::string body = BuildOllamaBody(req);
                std::string url = baseUrl_ + "/api/chat";
                std::string headers = "Content-Type: application/json\r\n";
                size_t pos = body.find("\"stream\":false");
                if (pos != std::string::npos) body.replace(pos, 13, "\"stream\":true");
                std::string result = WinHttpRequest(url, "POST", headers, body, 120000);
                if (!result.empty()) {
                    std::istringstream stream(result);
                    std::string line;
                    while (std::getline(stream, line)) {
                        if (line.empty()) continue;
                        auto val = JsonValue::parse(line);
                        if (val.has("message") && val["message"].has("content")) {
                            std::string text = val["message"]["content"].string();
                            if (!text.empty()) {
                                if (onChunk) onChunk(text);
                                resp.content += text;
                            }
                        }
                        if (val.has("done") && val["done"].boolean()) break;
                    }
                }
                if (onDone) onDone(resp);
            } catch (const std::exception &e) {
                if (onError) onError(e.what());
            }
        }).detach();
    }
};

// ============ Factory ============
AIProvider *AIProvider::Create(const std::string &providerType,
                                const std::string &apiKey,
                                const std::string &baseUrl) {
    if (providerType == "openai")    return new OpenAIProvider(apiKey, baseUrl);
    if (providerType == "anthropic") return new AnthropicProvider(apiKey, baseUrl);
    if (providerType == "gemini")    return new GeminiProvider(apiKey, baseUrl);
    if (providerType == "ollama")    return new OllamaProvider(apiKey, baseUrl);
    return nullptr;
}

// ============ WinHTTP helper ============
static std::string WinHttpRequest(const std::string &url, const std::string &method,
                                   const std::string &headers, const std::string &body,
                                   int timeoutMs) {
    HINTERNET hSession = WinHttpOpen(L"Prompts/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      nullptr, nullptr, 0);
    if (!hSession) return {};
    
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
    
    // Parse URL
    URL_COMPONENTSW urlComp = {sizeof(urlComp)};
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    std::wstring wurl(url.begin(), url.end());
    WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &urlComp);
    
    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0 && urlComp.lpszExtraInfo) {
        path += std::wstring(L"?") + std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }
    
    INTERNET_PORT port = urlComp.nPort;
    bool isSecure = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return {}; }
    
    DWORD flags = isSecure ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wmethod(method.begin(), method.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), path.c_str(),
                                             nullptr, nullptr, nullptr, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {}; }
    
    // Set headers
    if (!headers.empty()) {
        std::wstring wheaders(headers.begin(), headers.end());
        WinHttpAddRequestHeaders(hRequest, wheaders.c_str(), (DWORD)wheaders.size(),
                                 WINHTTP_ADDREQ_FLAG_ADD);
    }
    
    // Send request
    std::string result;
    if (WinHttpSendRequest(hRequest, nullptr, 0, (LPVOID)body.data(), (DWORD)body.size(),
                           (DWORD)body.size(), 0) &&
        WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD bytesRead = 0;
        char buf[4096];
        while (WinHttpReadData(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
            buf[bytesRead] = 0;
            result += buf;
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

// ============ Request builders ============
static std::string EscapeJson(const std::string &s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default: r += c;
        }
    }
    return r;
}

static std::string BuildOpenAIBody(const AIRequest &req) {
    std::string body = "{";
    body += "\"model\":\"" + EscapeJson(req.model) + "\",";
    body += "\"messages\":[";
    if (!req.systemPrompt.empty())
        body += "{\"role\":\"system\",\"content\":\"" + EscapeJson(req.systemPrompt) + "\"},";
    body += "{\"role\":\"user\",\"content\":\"" + EscapeJson(req.userPrompt) + "\"}";
    // Multi-modal attachments
    for (auto &att : req.attachments) {
        if (att.mimetype.find("image/") == 0 && !att.content.empty()) {
            body += ",{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"Attached image\"},";
            body += "{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:" + att.mimetype + ";base64," + att.content + "\"}}]}";
        }
    }
    body += "],";
    body += "\"temperature\":" + std::to_string(req.temperature) + ",";
    body += "\"max_tokens\":" + std::to_string(req.maxTokens) + ",";
    body += "\"stream\":false}";
    return body;
}

static std::string BuildAnthropicBody(const AIRequest &req) {
    std::string body = "{";
    body += "\"model\":\"" + EscapeJson(req.model) + "\",";
    body += "\"max_tokens\":" + std::to_string(req.maxTokens) + ",";
    body += "\"system\":\"" + EscapeJson(req.systemPrompt) + "\",";
    body += "\"messages\":[{\"role\":\"user\",\"content\":[";
    body += "{\"type\":\"text\",\"text\":\"" + EscapeJson(req.userPrompt) + "\"}";
    for (auto &att : req.attachments) {
        if (att.mimetype.find("image/") == 0 && !att.content.empty()) {
            body += ",{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":\"" + att.mimetype + "\",\"data\":\"" + att.content + "\"}}";
        }
    }
    body += "]}],\"stream\":false}";
    return body;
}

static std::string BuildGeminiBody(const AIRequest &req) {
    std::string body = "{\"contents\":[{\"parts\":[";
    body += "{\"text\":\"" + EscapeJson(req.userPrompt) + "\"}";
    for (auto &att : req.attachments) {
        if (att.mimetype.find("image/") == 0 && !att.content.empty()) {
            body += ",{\"inlineData\":{\"mimeType\":\"" + att.mimetype + "\",\"data\":\"" + att.content + "\"}}";
        }
    }
    body += "}]}],";
    body += "\"generationConfig\":{\"temperature\":" + std::to_string(req.temperature) + ",";
    body += "\"maxOutputTokens\":" + std::to_string(req.maxTokens) + "}}";
    return body;
}

static std::string BuildOllamaBody(const AIRequest &req) {
    std::string body = "{";
    body += "\"model\":\"" + EscapeJson(req.model) + "\",";
    body += "\"system\":\"" + EscapeJson(req.systemPrompt) + "\",";
    body += "\"prompt\":\"" + EscapeJson(req.userPrompt) + "\",";
    body += "\"options\":{\"temperature\":" + std::to_string(req.temperature) + "},";
    body += "\"stream\":false}";
    return body;
}

// ============ Response extractors ============
static std::string ExtractOpenAIResponse(const std::string &json) {
    auto val = JsonValue::parse(json);
    auto &choices = val["choices"];
    if (choices.type() == JsonValue::Array && choices.array().size() > 0)
        return choices[0]["message"]["content"].string();
    return "";
}

static std::string ExtractAnthropicResponse(const std::string &json) {
    auto val = JsonValue::parse(json);
    auto &content = val["content"];
    if (content.type() == JsonValue::Array && content.array().size() > 0)
        return content[0]["text"].string();
    return "";
}

static std::string ExtractGeminiResponse(const std::string &json) {
    auto val = JsonValue::parse(json);
    auto &candidates = val["candidates"];
    if (candidates.type() == JsonValue::Array && candidates.array().size() > 0) {
        auto &parts = candidates[0]["content"]["parts"];
        if (parts.type() == JsonValue::Array && parts.array().size() > 0)
            return parts[0]["text"].string();
    }
    return "";
}

static std::string ExtractOllamaResponse(const std::string &json) {
    auto val = JsonValue::parse(json);
    return val["message"]["content"].string();
}
