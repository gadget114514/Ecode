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

static std::string WideToUtf8(const std::wstring &wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, nullptr, nullptr);
    if (!str.empty() && str.back() == '\0') {
        str.pop_back();
    }
    return str;
}

Storage::Storage() {}

bool Storage::Init(const std::wstring &appDataPath) {
    basePath_ = appDataPath;
    if (globalPath_.empty()) {
        globalPath_ = appDataPath;
    }
    if (!EnsureDirectory(basePath_)) return false;
    EnsureDirectory(basePath_ + L"\\data");
    EnsureDirectory(basePath_ + L"\\blobs");
    EnsureDirectory(basePath_ + L"\\history");
    return true;
}

bool Storage::EnsureDirectory(const std::wstring &path) {
    if (CreateDirectoryW(path.c_str(), NULL)) return true;
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) return true;
    if (err == ERROR_PATH_NOT_FOUND) {
        // Create parent directory recursively
        size_t sep = path.rfind(L'\\');
        if (sep != std::wstring::npos && sep > 0) {
            std::wstring parent = path.substr(0, sep);
            if (EnsureDirectory(parent)) {
                return CreateDirectoryW(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
            }
        }
    }
    return false;
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
    if (!node.evaluation.empty())
        obj["evaluation"] = JsonValue::fromString(node.evaluation);
    if (!node.evaluatedAt.empty())
        obj["evaluatedAt"] = JsonValue::fromString(node.evaluatedAt);
    if (!node.evaluationNote.empty())
        obj["evaluationNote"] = JsonValue::fromString(node.evaluationNote);
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
    if (val.has("evaluation")) node.evaluation = val["evaluation"].string();
    if (val.has("evaluatedAt")) node.evaluatedAt = val["evaluatedAt"].string();
    if (val.has("evaluationNote")) node.evaluationNote = val["evaluationNote"].string();
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

std::map<std::string, ProviderConfig> Storage::LoadProviders() {
    std::map<std::string, ProviderConfig> providers;
    std::wstring filePath = (globalPath_.empty() ? basePath_ : globalPath_) + L"\\providers.json";
    OutputDebugStringW((L"[TRACE] Storage::LoadProviders: loading from " + filePath + L"\n").c_str());
    printf("[TRACE] Storage::LoadProviders: loading from %S\n", filePath.c_str());
    fflush(stdout);

    auto json = ReadFileUtf8(filePath);
    if (json.empty()) {
        OutputDebugStringA("[TRACE] Storage::LoadProviders: file is empty or missing\n");
        printf("[TRACE] Storage::LoadProviders: file is empty or missing\n");
        fflush(stdout);
        return providers;
    }
    
    OutputDebugStringA(("[TRACE] Storage::LoadProviders: raw json: " + json + "\n").c_str());
    printf("[TRACE] Storage::LoadProviders: raw json: %s\n", json.c_str());
    fflush(stdout);

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
        OutputDebugStringA(("[TRACE] Storage::LoadProviders: loaded provider '" + kv.first + "' with key='" + cfg.apiKey + "'\n").c_str());
        printf("[TRACE] Storage::LoadProviders: loaded provider '%s' with key='%s'\n", kv.first.c_str(), cfg.apiKey.c_str());
        fflush(stdout);
    }
    return providers;
}

bool Storage::SaveProviders(const std::map<std::string, ProviderConfig> &providers) {
    std::wstring parentPath = globalPath_.empty() ? basePath_ : globalPath_;
    EnsureDirectory(parentPath);
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
        
        OutputDebugStringA(("[TRACE] Storage::SaveProviders: saving provider '" + kv.first + "' with key='" + kv.second.apiKey + "'\n").c_str());
        printf("[TRACE] Storage::SaveProviders: saving provider '%s' with key='%s'\n", kv.first.c_str(), kv.second.apiKey.c_str());
        fflush(stdout);
    }
    std::wstring filePath = parentPath + L"\\providers.json";
    std::string serialized = JsonValue::fromObject(obj).serialize(true);
    OutputDebugStringW((L"[TRACE] Storage::SaveProviders: writing to " + filePath + L"\n").c_str());
    printf("[TRACE] Storage::SaveProviders: writing to %S\n", filePath.c_str());
    fflush(stdout);
    return WriteFileUtf8(filePath, serialized);
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

// --- History: update evaluation field in-place ---
void Storage::UpdateHistoryEvaluation(const std::wstring &filename, const std::string &evaluation) {
    std::wstring path = basePath_ + L"\\history\\" + filename;
    std::string json = ReadFileUtf8(path);
    if (json.empty()) return;
    auto val = JsonValue::parse(json);
    // Rebuild with updated evaluation field
    // Simple approach: re-serialize the record with the new evaluation
    std::map<std::string, JsonValue> obj;
    for (auto &kv : val.object()) obj[kv.first] = kv.second;
    obj["evaluation"] = JsonValue::fromString(evaluation);
    WriteFileUtf8(path, JsonValue::fromObject(obj).serialize(true));
}

// --- Optimizer: generic key-value JSON storage ---
void Storage::SaveOptimizerData(const std::wstring &relativePath, const std::string &json) {
    // Ensure opt_versions subdirectory if needed
    std::wstring fullPath = basePath_ + L"\\" + relativePath;
    // Create parent directory if it contains a backslash
    auto sep = fullPath.rfind(L'\\');
    if (sep != std::wstring::npos) {
        std::wstring dir = fullPath.substr(0, sep);
        EnsureDirectory(dir);
    }
    WriteFileUtf8(fullPath, json);
}

std::string Storage::LoadOptimizerData(const std::wstring &relativePath) {
    return ReadFileUtf8(basePath_ + L"\\" + relativePath);
}

// --- Wizards ---
std::string Storage::LoadWizardData(const std::string &wizardName) {
    // Try AppData/wizards/<name>.json first (user custom wizards)
    std::wstring wizName(wizardName.begin(), wizardName.end());
    std::wstring wizPath = basePath_ + L"\\wizards\\" + wizName + L".json";
    std::string result = ReadFileUtf8(wizPath);
    if (!result.empty()) return result;

    // Try frontend/wizards/<name>.json (built-in wizards)
    // The frontend directory is alongside the app binary:
    // <exe_dir>/frontend/wizards/<name>.json
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    auto pos = exeDir.rfind(L'\\');
    if (pos != std::wstring::npos) exeDir = exeDir.substr(0, pos);
    wizPath = exeDir + L"\\frontend\\wizards\\" + wizName + L".json";
    return ReadFileUtf8(wizPath);
}

// --- Project Path Resolution (security: reject traversal) ---
std::wstring Storage::ResolveProjectPath(const std::wstring &relativePath) const {
    std::wstring dataDir = basePath_ + L"\\data";
    std::wstring fullPath = dataDir + L"\\" + relativePath;
    wchar_t resolved[32768] = {};
    GetFullPathNameW(fullPath.c_str(), 32768, resolved, nullptr);
    std::wstring result(resolved);
    // Reject if it escapes the data directory
    std::wstring expectedPrefix = dataDir + L"\\";
    if (result.substr(0, expectedPrefix.size()) != expectedPrefix) {
        return L"";  // traversal attack
    }
    return result;
}

// --- Checkpoints ---
void Storage::SaveCheckpoint(const std::string &runId, int stepIndex,
                              const std::string &input, const std::string &output,
                              const std::string &metaJson) {
    std::wstring wRunId(runId.begin(), runId.end());
    std::wstring dir = basePath_ + L"\\history\\" + wRunId + L"\\checkpoint_" + std::to_wstring(stepIndex);
    EnsureDirectory(dir);
    WriteFileUtf8(dir + L"\\input.json", input);
    WriteFileUtf8(dir + L"\\output.json", output);
    WriteFileUtf8(dir + L"\\meta.json", metaJson);
}

static std::string ReadCheckpointFile(const std::wstring &dir, const std::wstring &filename) {
    return ReadFileUtf8(dir + L"\\" + filename);
}

std::string Storage::LoadCheckpointInput(const std::string &runId, int stepIndex) {
    std::wstring wRunId(runId.begin(), runId.end());
    return ReadFileUtf8(basePath_ + L"\\history\\" + wRunId + L"\\checkpoint_" + std::to_wstring(stepIndex) + L"\\input.json");
}

std::string Storage::LoadCheckpointOutput(const std::string &runId, int stepIndex) {
    std::wstring wRunId(runId.begin(), runId.end());
    return ReadFileUtf8(basePath_ + L"\\history\\" + wRunId + L"\\checkpoint_" + std::to_wstring(stepIndex) + L"\\output.json");
}

std::string Storage::LoadCheckpointMeta(const std::string &runId, int stepIndex) {
    std::wstring wRunId(runId.begin(), runId.end());
    return ReadFileUtf8(basePath_ + L"\\history\\" + wRunId + L"\\checkpoint_" + std::to_wstring(stepIndex) + L"\\meta.json");
}

// --- Run State ---
void Storage::SaveRunState(const std::string &runId, const std::string &stateJson) {
    std::wstring wRunId(runId.begin(), runId.end());
    std::wstring dir = basePath_ + L"\\history\\" + wRunId;
    EnsureDirectory(dir);
    WriteFileUtf8(dir + L"\\state.json", stateJson);
}

std::string Storage::LoadRunState(const std::string &runId) {
    std::wstring wRunId(runId.begin(), runId.end());
    return ReadFileUtf8(basePath_ + L"\\history\\" + wRunId + L"\\state.json");
}

// --- Incomplete Run Detection ---
std::vector<Storage::IncompleteRun> Storage::ScanIncompleteRuns() {
    std::vector<IncompleteRun> result;
    std::wstring historyDir = basePath_ + L"\\history";
    std::wstring pattern = historyDir + L"\\*";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return result;
    do {
        std::wstring name = ffd.cFileName;
        if (name == L"." || name == L"..") continue;
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        std::wstring stateFile = historyDir + L"\\" + name + L"\\state.json";
        std::string state = ReadFileUtf8(stateFile);
        if (state.empty()) continue;
        auto val = JsonValue::parse(state);
        if (!val.has("status")) continue;
        std::string status = val["status"].string();
        if (status != "running") continue;

        IncompleteRun run;
        run.runId = WideToUtf8(name);
        if (val.has("pipelineName")) run.pipelineName = val["pipelineName"].string();
        if (val.has("currentStep")) run.lastCompletedStep = (int)val["currentStep"].number();
        if (val.has("totalSteps")) run.totalSteps = (int)val["totalSteps"].number();
        if (val.has("startedAt")) run.startedAt = val["startedAt"].string();
        run.status = "interrupted";
        result.push_back(run);
    } while (FindNextFileW(hFind, &ffd) != 0);
    FindClose(hFind);
    return result;
}

// Helper to clean up a directory recursively
static void StorageCleanupDir(const std::wstring &dir) {
    std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = ffd.cFileName;
            if (name == L"." || name == L"..") continue;
            std::wstring full = dir + L"\\" + name;
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                StorageCleanupDir(full);
                RemoveDirectoryW(full.c_str());
            } else {
                DeleteFileW(full.c_str());
            }
        } while (FindNextFileW(hFind, &ffd) != 0);
        FindClose(hFind);
    }
}

void Storage::CloseRun(const std::string &runId) {
    std::wstring wRunId(runId.begin(), runId.end());
    std::wstring stateFile = basePath_ + L"\\history\\" + wRunId + L"\\state.json";
    std::string state = ReadFileUtf8(stateFile);
    if (state.empty()) return;
    auto val = JsonValue::parse(state);
    // Rebuild object with updated status
    std::map<std::string, JsonValue> obj;
    for (auto &kv : val.object()) obj[kv.first] = kv.second;
    obj["status"] = JsonValue::fromString("completed");
    WriteFileUtf8(stateFile, JsonValue::fromObject(obj).serialize(true));
}

void Storage::DiscardRun(const std::string &runId) {
    std::wstring wRunId(runId.begin(), runId.end());
    std::wstring runDir = basePath_ + L"\\history\\" + wRunId;
    // Recursive delete
    StorageCleanupDir(runDir);
}

// --- Named Chests ---
static std::wstring ChestRootForBase(const std::wstring &basePath) {
    // Chests are at the same level as projects/ — parent of basePath
    std::wstring parent = basePath;
    auto pos = parent.rfind(L'\\');
    if (pos != std::wstring::npos) parent = parent.substr(0, pos);
    pos = parent.rfind(L'\\');
    if (pos != std::wstring::npos) parent = parent.substr(0, pos);
    return parent + L"\\chests";
}

void Storage::SaveToNamedChest(const std::string &chestName, const std::string &content) {
    std::wstring chestDir = ChestRootForBase(basePath_);
    EnsureDirectory(chestDir);
    std::wstring wName(chestName.begin(), chestName.end());
    WriteFileUtf8(chestDir + L"\\chest_" + wName + L".json", content);
}

std::string Storage::LoadFromNamedChest(const std::string &chestName) {
    std::wstring chestDir = ChestRootForBase(basePath_);
    std::wstring wName(chestName.begin(), chestName.end());
    return ReadFileUtf8(chestDir + L"\\chest_" + wName + L".json");
}

bool Storage::ChestExists(const std::string &chestName) const {
    std::wstring chestDir = ChestRootForBase(basePath_);
    std::wstring wName(chestName.begin(), chestName.end());
    std::wstring path = chestDir + L"\\chest_" + wName + L".json";
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::vector<std::string> Storage::ListNamedChests() const {
    std::vector<std::string> result;
    std::wstring chestDir = ChestRootForBase(basePath_);
    std::wstring pattern = chestDir + L"\\chest_*.json";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = ffd.cFileName;
            if (name == L"." || name == L"..") continue;
            // Extract chest name from "chest_<name>.json"
            std::wstring chestName = name.substr(6); // strip "chest_"
            auto dotPos = chestName.rfind(L'.');
            if (dotPos != std::wstring::npos) chestName = chestName.substr(0, dotPos);
            result.push_back(WideToUtf8(chestName));
        } while (FindNextFileW(hFind, &ffd) != 0);
        FindClose(hFind);
    }
    return result;
}

// --- Storage Chest ---
static std::wstring StorageChestDirForBase(const std::wstring &basePath) {
    std::wstring parent = basePath;
    auto pos = parent.rfind(L'\\');
    if (pos != std::wstring::npos) parent = parent.substr(0, pos);
    pos = parent.rfind(L'\\');
    if (pos != std::wstring::npos) parent = parent.substr(0, pos);
    return parent + L"\\storage_chest";
}

void Storage::MoveToStorageChest(const std::wstring &sourcePath, const std::string &storedName) {
    std::wstring chestDir = StorageChestDirForBase(basePath_);
    EnsureDirectory(chestDir);
    std::wstring wName(storedName.begin(), storedName.end());
    std::wstring destPath = chestDir + L"\\" + wName;
    // Move the file
    MoveFileExW(sourcePath.c_str(), destPath.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
}

std::vector<std::wstring> Storage::ListStorageChest() const {
    std::vector<std::wstring> result;
    std::wstring chestDir = StorageChestDirForBase(basePath_);
    std::wstring pattern = chestDir + L"\\*";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = ffd.cFileName;
            if (name == L"." || name == L"..") continue;
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            result.push_back(chestDir + L"\\" + name);
        } while (FindNextFileW(hFind, &ffd) != 0);
        FindClose(hFind);
    }
    return result;
}

std::string Storage::LoadFromStorageChest(const std::wstring &filename) {
    std::wstring chestDir = StorageChestDirForBase(basePath_);
    return ReadFileUtf8(chestDir + L"\\" + filename);
}

// --- History Retention ---
void Storage::SetMaxHistoryRuns(int maxRuns) {
    maxHistoryRuns_ = (maxRuns < 10) ? 10 : (maxRuns > 500) ? 500 : maxRuns;
}

// --- General Config ---
Storage::GeneralConfig Storage::LoadGeneralConfig() {
    GeneralConfig cfg;
    std::wstring filePath = (globalPath_.empty() ? basePath_ : globalPath_) + L"\\config.json";
    auto json = ReadFileUtf8(filePath);
    if (json.empty()) return cfg;
    auto val = JsonValue::parse(json);
    if (val.has("historyRetention"))
        cfg.historyRetention = (int)val["historyRetention"].number();
    if (val.has("defaultProvider"))
        cfg.defaultProvider = val["defaultProvider"].string();
    if (val.has("defaultModel"))
        cfg.defaultModel = val["defaultModel"].string();
    return cfg;
}

bool Storage::SaveGeneralConfig(const GeneralConfig &cfg) {
    std::wstring parentPath = globalPath_.empty() ? basePath_ : globalPath_;
    EnsureDirectory(parentPath);
    std::map<std::string, JsonValue> obj;
    obj["historyRetention"] = JsonValue::fromDouble((double)cfg.historyRetention);
    if (!cfg.defaultProvider.empty())
        obj["defaultProvider"] = JsonValue::fromString(cfg.defaultProvider);
    if (!cfg.defaultModel.empty())
        obj["defaultModel"] = JsonValue::fromString(cfg.defaultModel);
    std::wstring filePath = parentPath + L"\\config.json";
    return WriteFileUtf8(filePath, JsonValue::fromObject(obj).serialize(true));
}

// --- Recipes ---
std::vector<Storage::Recipe> Storage::LoadRecipes() {
    std::vector<Recipe> recipes;
    std::wstring filePath = (globalPath_.empty() ? basePath_ : globalPath_) + L"\\recipes.json";
    OutputDebugStringW((L"[TRACE] Storage::LoadRecipes: loading from " + filePath + L"\n").c_str());
    printf("[TRACE] Storage::LoadRecipes: loading from %S\n", filePath.c_str());
    fflush(stdout);

    auto json = ReadFileUtf8(filePath);
    if (json.empty()) {
        OutputDebugStringA("[TRACE] Storage::LoadRecipes: file is empty or missing\n");
        printf("[TRACE] Storage::LoadRecipes: file is empty or missing\n");
        fflush(stdout);
        return recipes;
    }

    OutputDebugStringA(("[TRACE] Storage::LoadRecipes: raw json: " + json + "\n").c_str());
    printf("[TRACE] Storage::LoadRecipes: raw json: %s\n", json.c_str());
    fflush(stdout);

    auto val = JsonValue::parse(json);
    if (val.type() != JsonValue::Array) {
        OutputDebugStringA("[TRACE] Storage::LoadRecipes: parsed JSON is not an Array!\n");
        printf("[TRACE] Storage::LoadRecipes: parsed JSON is not an Array!\n");
        fflush(stdout);
        return recipes;
    }

    for (auto &item : val.array()) {
        Recipe r;
        if (item.has("type")) r.type = item["type"].string();
        if (item.has("name")) r.name = item["name"].string();
        if (item.has("provider")) r.provider = item["provider"].string();
        if (item.has("model")) r.model = item["model"].string();
        if (item.has("temperature")) r.temperature = item["temperature"].number();
        if (item.has("systemPrompt")) r.systemPrompt = item["systemPrompt"].string();
        if (item.has("command")) r.command = item["command"].string();
        if (!r.name.empty()) {
            recipes.push_back(r);
            OutputDebugStringA(("[TRACE] Storage::LoadRecipes: loaded recipe '" + r.name + "'\n").c_str());
            printf("[TRACE] Storage::LoadRecipes: loaded recipe '%s'\n", r.name.c_str());
            fflush(stdout);
        }
    }
    return recipes;
}

bool Storage::SaveRecipes(const std::vector<Recipe> &recipes) {
    std::wstring parentPath = globalPath_.empty() ? basePath_ : globalPath_;
    EnsureDirectory(parentPath);
    std::vector<JsonValue> arr;
    for (auto &r : recipes) {
        std::map<std::string, JsonValue> obj;
        obj["type"] = JsonValue::fromString(r.type);
        obj["name"] = JsonValue::fromString(r.name);
        obj["provider"] = JsonValue::fromString(r.provider);
        obj["model"] = JsonValue::fromString(r.model);
        obj["temperature"] = JsonValue::fromDouble(r.temperature);
        obj["systemPrompt"] = JsonValue::fromString(r.systemPrompt);
        obj["command"] = JsonValue::fromString(r.command);
        arr.push_back(JsonValue::fromObject(obj));

        OutputDebugStringA(("[TRACE] Storage::SaveRecipes: saving recipe '" + r.name + "'\n").c_str());
        printf("[TRACE] Storage::SaveRecipes: saving recipe '%s'\n", r.name.c_str());
        fflush(stdout);
    }
    std::wstring filePath = parentPath + L"\\recipes.json";
    std::string serialized = JsonValue::fromArray(arr).serialize(true);

    OutputDebugStringW((L"[TRACE] Storage::SaveRecipes: writing to " + filePath + L"\n").c_str());
    printf("[TRACE] Storage::SaveRecipes: writing to %S\n", filePath.c_str());
    fflush(stdout);

    return WriteFileUtf8(filePath, serialized);
}

std::string Storage::GetFileTreeJson() const {
    fs::path dataDir(basePath_ + L"\\data");
    if (!fs::exists(dataDir) || !fs::is_directory(dataDir)) {
        return "[]";
    }
    
    std::vector<JsonValue> rootList;
    std::function<JsonValue(const fs::path&)> scan = [&](const fs::path &dir) -> JsonValue {
        std::map<std::string, JsonValue> obj;
        obj["name"] = JsonValue::fromString(WideToUtf8(dir.filename().wstring()));
        obj["type"] = JsonValue::fromString("directory");
        
        std::vector<JsonValue> children;
        for (const auto &entry : fs::directory_iterator(dir)) {
            if (entry.is_directory()) {
                children.push_back(scan(entry.path()));
            } else if (entry.is_regular_file()) {
                std::map<std::string, JsonValue> fileObj;
                fileObj["name"] = JsonValue::fromString(WideToUtf8(entry.path().filename().wstring()));
                fileObj["type"] = JsonValue::fromString("file");
                
                auto rel = fs::relative(entry.path(), dataDir);
                fileObj["path"] = JsonValue::fromString(WideToUtf8(rel.wstring()));
                children.push_back(JsonValue::fromObject(fileObj));
            }
        }
        obj["children"] = JsonValue::fromArray(children);
        return JsonValue::fromObject(obj);
    };
    
    for (const auto &entry : fs::directory_iterator(dataDir)) {
        if (entry.is_directory()) {
            rootList.push_back(scan(entry.path()));
        } else if (entry.is_regular_file()) {
            std::map<std::string, JsonValue> fileObj;
            fileObj["name"] = JsonValue::fromString(WideToUtf8(entry.path().filename().wstring()));
            fileObj["type"] = JsonValue::fromString("file");
            
            auto rel = fs::relative(entry.path(), dataDir);
            fileObj["path"] = JsonValue::fromString(WideToUtf8(rel.wstring()));
            rootList.push_back(JsonValue::fromObject(fileObj));
        }
    }
    
    return JsonValue::fromArray(rootList).serialize(false);
}
