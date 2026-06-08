#include "Storage.h"
#include "JsonParser.h"
#include "Base64.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <ctime>

namespace fs = std::filesystem;

Storage::Storage() {}

bool Storage::Init(const std::wstring &appDataPath) {
    basePath_ = appDataPath;
    if (!EnsureDirectory(basePath_)) return false;
    EnsureDirectory(basePath_ + L"\\data");
    EnsureDirectory(basePath_ + L"\\blobs");
    EnsureDirectory(basePath_ + L"\\history");
    return true;
}

bool Storage::EnsureDirectory(const std::wstring &path) {
    return CreateDirectoryW(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

std::wstring Storage::GetUserDataPath() const {
    return basePath_;
}

std::wstring Storage::DataPath(const std::wstring &relativePath) const {
    return basePath_ + L"\\data\\" + relativePath;
}

std::wstring Storage::BlobPath(const std::wstring &relativePath) const {
    return basePath_ + L"\\blobs\\" + relativePath;
}

static std::string ReadFileUtf8(const std::wstring &path) {
    FILE *f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return {};
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string result(len, '\0');
    fread(&result[0], 1, len, f);
    fclose(f);
    return result;
}

static bool WriteFileUtf8(const std::wstring &path, const std::string &content) {
    FILE *f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) return false;
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return true;
}

static std::wstring GetTimestamp() {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_s(&tm, &t);
    wchar_t buf[32];
    wcsftime(buf, 32, L"%Y%m%d_%H%M%S", &tm);
    return buf;
}

// --- Session ---
SessionData Storage::LoadSession() {
    SessionData session;
    std::wstring path = basePath_ + L"\\session.json";
    auto json = ReadFileUtf8(path);
    if (json.empty()) {
        // Create default
        TabData def;
        def.name = "General";
        def.file = "general.json";
        session.tabs.push_back(def);
        SaveSession(session);
        return session;
    }
    auto val = JsonValue::parse(json);
    if (val.has("tabs")) {
        for (auto &t : val["tabs"].array()) {
            TabData tab;
            tab.name = t["name"].string();
            tab.file = t["file"].string();
            session.tabs.push_back(tab);
        }
    }
    return session;
}

static JsonValue NodeToJson(const Node &node) {
    std::map<std::string, JsonValue> obj;
    obj["title"] = JsonValue::fromString(node.title);
    obj["content"] = JsonValue::fromString(node.content);
    obj["mimetype"] = JsonValue::fromString(node.mimetype);
    std::vector<JsonValue> atts;
    for (auto &a : node.attachments) {
        std::map<std::string, JsonValue> att;
        att["id"] = JsonValue::fromString(a.id);
        att["mimetype"] = JsonValue::fromString(a.mimetype);
        att["inline"] = JsonValue::fromBool(a.inlineData);
        att["content"] = JsonValue::fromString(a.content);
        att["file"] = JsonValue::fromString(a.file);
        att["size"] = JsonValue::fromDouble((double)a.size);
        atts.push_back(JsonValue::fromObject(att));
    }
    obj["attachments"] = JsonValue::fromArray(atts);
    std::vector<JsonValue> children;
    for (auto &c : node.children)
        children.push_back(NodeToJson(c));
    obj["children"] = JsonValue::fromArray(children);
    if (!node.pipelineMeta.empty())
        obj["pipelineMeta"] = JsonValue::fromString(node.pipelineMeta);
    return JsonValue::fromObject(obj);
}

/*static*/ std::string Storage::SerializeNode(const Node &node) {
    return NodeToJson(node).serialize();
}

static Node JsonToNode(const JsonValue &val) {
    Node node;
    if (val.has("title")) node.title = val["title"].string();
    if (val.has("content")) node.content = val["content"].string();
    if (val.has("mimetype")) node.mimetype = val["mimetype"].string();
    if (val.has("attachments")) {
        for (auto &a : val["attachments"].array()) {
            Attachment att;
            if (a.has("id")) att.id = a["id"].string();
            if (a.has("mimetype")) att.mimetype = a["mimetype"].string();
            if (a.has("inline")) att.inlineData = a["inline"].boolean();
            if (a.has("content")) att.content = a["content"].string();
            if (a.has("file")) att.file = a["file"].string();
            if (a.has("size")) att.size = (size_t)a["size"].number();
            node.attachments.push_back(att);
        }
    }
    if (val.has("children")) {
        for (auto &c : val["children"].array())
            node.children.push_back(JsonToNode(c));
    }
    if (val.has("pipelineMeta")) node.pipelineMeta = val["pipelineMeta"].string();
    return node;
}

void Storage::SaveSession(const SessionData &session) {
    std::map<std::string, JsonValue> tabsArr;
    std::vector<JsonValue> arr;
    for (auto &t : session.tabs) {
        std::map<std::string, JsonValue> obj;
        obj["name"] = JsonValue::fromString(t.name);
        obj["file"] = JsonValue::fromString(t.file);
        arr.push_back(JsonValue::fromObject(obj));
    }
    tabsArr["tabs"] = JsonValue::fromArray(arr);
    auto json = JsonValue::fromObject(tabsArr).serialize(true);
    WriteFileUtf8(basePath_ + L"\\session.json", json);
}

// --- Tab data ---
Node Storage::LoadTabData(const std::wstring &filePath) {
    std::wstring path = DataPath(filePath);
    if (!fs::exists(path)) return {};
    auto json = ReadFileUtf8(path);
    if (json.empty()) return {};
    auto val = JsonValue::parse(json);
    return JsonToNode(val);
}

void Storage::SaveTabData(const std::wstring &filePath, const Node &root) {
    auto json = NodeToJson(root).serialize(true);
    WriteFileUtf8(DataPath(filePath), json);
}

// --- Blobs ---
std::string Storage::LoadBlob(const std::wstring &relativePath) {
    return ReadFileUtf8(BlobPath(relativePath));
}

std::wstring Storage::SaveBlob(const std::string &data, const std::wstring &ext) {
    return SaveBlob((const unsigned char*)data.data(), data.size(), ext);
}

std::wstring Storage::SaveBlob(const unsigned char *data, size_t len, const std::wstring &ext) {
    auto ts = GetTimestamp();
    static int counter = 0;
    std::wstring name = L"pipeline_" + ts + L"_" + std::to_wstring(counter++) + ext;
    std::wstring path = BlobPath(name);
    FILE *f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") == 0 && f) {
        fwrite(data, 1, len, f);
        fclose(f);
    }
    return name;
}

void Storage::RemoveBlob(const std::wstring &relativePath) {
    auto path = BlobPath(relativePath);
    DeleteFileW(path.c_str());
}

void Storage::GarbageCollectBlobs(const std::vector<std::wstring> &referencedPaths) {
    std::wstring blobDir = basePath_ + L"\\blobs\\";
    std::wstring pattern = blobDir + L"*";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring name = ffd.cFileName;
        if (name == L"." || name == L"..") continue;
        bool referenced = false;
        for (auto &ref : referencedPaths) {
            if (ref == name) { referenced = true; break; }
        }
        if (!referenced) {
            DeleteFileW((blobDir + name).c_str());
        }
    } while (FindNextFileW(hFind, &ffd) != 0);
    FindClose(hFind);
}

std::vector<std::wstring> Storage::GetTabFiles() {
    std::vector<std::wstring> files;
    std::wstring dataDir = basePath_ + L"\\data\\";
    std::wstring pattern = dataDir + L"*.json";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return files;
    do {
        std::wstring name = ffd.cFileName;
        if (name != L"." && name != L"..") files.push_back(name);
    } while (FindNextFileW(hFind, &ffd) != 0);
    FindClose(hFind);
    return files;
}

// --- Recent Files ---
std::vector<std::wstring> Storage::LoadRecentFiles() {
    std::vector<std::wstring> files;
    auto json = ReadFileUtf8(basePath_ + L"\\recent_files.json");
    if (json.empty()) return files;
    auto val = JsonValue::parse(json);
    if (val.has("files")) {
        for (auto &f : val["files"].array())
            files.push_back(L"data/" + std::wstring(f.string().begin(), f.string().end()));
    }
    return files;
}

void Storage::SaveRecentFiles(const std::vector<std::wstring> &files) {
    std::vector<JsonValue> arr;
    for (auto &f : files) {
        std::string fa;
        int n = WideCharToMultiByte(CP_UTF8, 0, f.c_str(), -1, nullptr, 0, nullptr, nullptr);
        fa.resize(n);
        WideCharToMultiByte(CP_UTF8, 0, f.c_str(), -1, &fa[0], n, nullptr, nullptr);
        if (!fa.empty() && fa.back() == '\0') fa.pop_back();
        arr.push_back(JsonValue::fromString(fa));
    }
    std::map<std::string, JsonValue> root;
    root["files"] = JsonValue::fromArray(arr);
    WriteFileUtf8(basePath_ + L"\\recent_files.json", JsonValue::fromObject(root).serialize(true));
}

// --- History ---
void Storage::SaveHistory(const std::string &recordJson) {
    std::wstring filename;
    auto val = JsonValue::parse(recordJson);
    if (val.has("id") && !val["id"].string().empty()) {
        std::string id = val["id"].string();
        // id contains only ASCII chars, safe narrow→wide
        std::wstring wid(id.begin(), id.end());
        filename = L"run_" + wid + L".json";
    } else {
        filename = L"run_" + GetTimestamp() + L".json";
    }
    WriteFileUtf8(basePath_ + L"\\history\\" + filename, recordJson);
}

std::vector<std::wstring> Storage::ListHistory() {
    std::vector<std::wstring> result;
    std::wstring histDir = basePath_ + L"\\history\\";
    std::wstring pattern = histDir + L"run_*.json";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return result;
    do {
        std::wstring name = ffd.cFileName;
        if (name != L"." && name != L"..") result.push_back(name);
    } while (FindNextFileW(hFind, &ffd) != 0);
    FindClose(hFind);
    std::sort(result.rbegin(), result.rend()); // newest first
    return result;
}

std::string Storage::LoadHistoryRecord(const std::wstring &filename) {
    return ReadFileUtf8(basePath_ + L"\\history\\" + filename);
}

// --- Providers ---
std::map<std::string, ProviderConfig> Storage::LoadProviders() {
    std::map<std::string, ProviderConfig> providers;
    auto json = ReadFileUtf8(basePath_ + L"\\providers.json");
    if (json.empty()) return providers;
    auto val = JsonValue::parse(json);
    for (auto &kv : val.object()) {
        ProviderConfig cfg;
        if (kv.second.has("apiKey")) cfg.apiKey = kv.second["apiKey"].string();
        if (kv.second.has("baseUrl")) cfg.baseUrl = kv.second["baseUrl"].string();
        if (kv.second.has("models")) {
            for (auto &m : kv.second["models"].array())
                cfg.models.push_back(m.string());
        }
        providers[kv.first] = cfg;
    }
    return providers;
}

void Storage::SaveProviders(const std::map<std::string, ProviderConfig> &providers) {
    std::map<std::string, JsonValue> obj;
    for (auto &kv : providers) {
        std::map<std::string, JsonValue> cfg;
        cfg["apiKey"] = JsonValue::fromString(kv.second.apiKey);
        cfg["baseUrl"] = JsonValue::fromString(kv.second.baseUrl);
        std::vector<JsonValue> models;
        for (auto &m : kv.second.models)
            models.push_back(JsonValue::fromString(m));
        cfg["models"] = JsonValue::fromArray(models);
        obj[kv.first] = JsonValue::fromObject(cfg);
    }
    WriteFileUtf8(basePath_ + L"\\providers.json", JsonValue::fromObject(obj).serialize(true));
}

// --- Pipelines ---
std::vector<Pipeline> Storage::LoadPipelines() {
    std::vector<Pipeline> pipelines;
    auto json = ReadFileUtf8(basePath_ + L"\\pipeline.json");
    if (json.empty()) return pipelines;
    auto val = JsonValue::parse(json);
    if (val.has("pipelines")) {
        for (auto &p : val["pipelines"].array()) {
            Pipeline pipe;
            if (p.has("name")) pipe.name = p["name"].string();
            if (p.has("mode")) pipe.mode = p["mode"].string();
            if (p.has("outputMode")) pipe.outputMode = p["outputMode"].string();
            if (p.has("outputNaming")) pipe.outputNaming = p["outputNaming"].string();
            if (p.has("multiMedia")) pipe.multiMedia = p["multiMedia"].string();
            if (p.has("steps")) {
                for (auto &s : p["steps"].array()) {
                    PipelineStep step;
                    if (s.has("name")) step.name = s["name"].string();
                    if (s.has("type")) step.type = s["type"].string();
                    // params would be loaded more thoroughly
                    for (auto &kv : s.object()) {
                        step.params[kv.first] = kv.second.serialize();
                    }
                    pipe.steps.push_back(step);
                }
            }
            pipelines.push_back(pipe);
        }
    }
    return pipelines;
}

/*static*/ std::string Storage::SerializePipelines(const std::vector<Pipeline> &pipelines) {
    std::vector<JsonValue> arr;
    for (auto &p : pipelines) {
        std::map<std::string, JsonValue> obj;
        obj["name"] = JsonValue::fromString(p.name);
        obj["mode"] = JsonValue::fromString(p.mode);
        obj["outputMode"] = JsonValue::fromString(p.outputMode);
        obj["outputNaming"] = JsonValue::fromString(p.outputNaming);
        obj["multiMedia"] = JsonValue::fromString(p.multiMedia);
        obj["retryCount"] = JsonValue::fromDouble((double)p.retryCount);
        obj["retryDelayMs"] = JsonValue::fromDouble((double)p.retryDelayMs);
        std::vector<JsonValue> steps;
        for (auto &s : p.steps) {
            std::map<std::string, JsonValue> step;
            step["name"] = JsonValue::fromString(s.name);
            step["type"] = JsonValue::fromString(s.type);
            for (auto &kv : s.params) {
                // Try to parse as JSON, fall back to string
                auto parsed = JsonValue::parse(kv.second);
                if (parsed.type() == JsonValue::Null) {
                    step[kv.first] = JsonValue::fromString(kv.second);
                } else {
                    step[kv.first] = parsed;
                }
            }
            steps.push_back(JsonValue::fromObject(step));
        }
        obj["steps"] = JsonValue::fromArray(steps);
        arr.push_back(JsonValue::fromObject(obj));
    }
    std::map<std::string, JsonValue> root;
    root["pipelines"] = JsonValue::fromArray(arr);
    return JsonValue::fromObject(root).serialize(true);
}

void Storage::SavePipelines(const std::vector<Pipeline> &pipelines) {
    WriteFileUtf8(basePath_ + L"\\pipeline.json", SerializePipelines(pipelines));
}
