#include "PipelineVersionManager.h"
#include "PipelineOptimizer.h"  // OptEditProposal
#include "Storage.h"
#include "JsonParser.h"
#include <sstream>
#include <algorithm>
#include <ctime>
#include <windows.h>
// Win32 macros clash with our method names — undefine them after including windows.h
#ifdef LoadCursor
#undef LoadCursor
#endif

PipelineVersionManager::PipelineVersionManager(Storage &storage)
    : storage_(storage) {}

// --- Name sanitization ---
/*static*/ std::string PipelineVersionManager::SanitizeName(const std::string &name) {
    std::string safe;
    for (char c : name) {
        if (std::isalnum((unsigned char)c) || c == '-') safe += c;
        else safe += '_';
    }
    return safe;
}

static std::wstring ToWide(const std::string &s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

static std::string IsoTimestamp() {
    time_t t = time(nullptr);
    struct tm tm_info;
    gmtime_s(&tm_info, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_info);
    return buf;
}

// --- Serialization helpers ---

/*static*/ std::string PipelineVersionManager::SerializeCursor(const VersionCursor &cursor) {
    std::map<std::string, JsonValue> obj;
    obj["pipelineName"] = JsonValue::fromString(cursor.pipelineName);
    obj["currentVersion"] = JsonValue::fromDouble(cursor.currentVersion);
    obj["headVersion"] = JsonValue::fromDouble(cursor.headVersion);
    std::vector<JsonValue> entries;
    for (auto &e : cursor.entries) {
        std::map<std::string, JsonValue> ej;
        ej["version"] = JsonValue::fromDouble(e.version);
        ej["timestamp"] = JsonValue::fromString(e.timestamp);
        ej["sessionId"] = JsonValue::fromString(e.sessionId);
        ej["label"] = JsonValue::fromString(e.label);
        // approvedProposals serialized as array
        std::vector<JsonValue> props;
        for (auto &p : e.approvedProposals) {
            std::map<std::string, JsonValue> pj;
            pj["op"] = JsonValue::fromString(p.op);
            pj["stepName"] = JsonValue::fromString(p.stepName);
            pj["field"] = JsonValue::fromString(p.field);
            pj["oldValue"] = JsonValue::fromString(p.oldValue);
            pj["newValue"] = JsonValue::fromString(p.newValue);
            pj["rationale"] = JsonValue::fromString(p.rationale);
            props.push_back(JsonValue::fromObject(pj));
        }
        ej["approvedProposals"] = JsonValue::fromArray(props);
        entries.push_back(JsonValue::fromObject(ej));
    }
    obj["entries"] = JsonValue::fromArray(entries);
    return JsonValue::fromObject(obj).serialize(true);
}

static OptEditProposal ProposalFromJson(const JsonValue &j) {
    OptEditProposal p;
    if (j.has("op")) p.op = j["op"].string();
    if (j.has("stepName")) p.stepName = j["stepName"].string();
    if (j.has("field")) p.field = j["field"].string();
    if (j.has("oldValue")) p.oldValue = j["oldValue"].string();
    if (j.has("newValue")) p.newValue = j["newValue"].string();
    if (j.has("rationale")) p.rationale = j["rationale"].string();
    return p;
}

/*static*/ VersionCursor PipelineVersionManager::DeserializeCursor(const std::string &json,
                                                                    const std::string &pipelineName) {
    VersionCursor cursor;
    cursor.pipelineName = pipelineName;
    if (json.empty()) return cursor;
    auto val = JsonValue::parse(json);
    if (val.has("currentVersion")) cursor.currentVersion = (int)val["currentVersion"].number();
    if (val.has("headVersion")) cursor.headVersion = (int)val["headVersion"].number();
    if (val.has("entries")) {
        for (auto &e : val["entries"].array()) {
            PipelineVersion v;
            if (e.has("version")) v.version = (int)e["version"].number();
            if (e.has("timestamp")) v.timestamp = e["timestamp"].string();
            if (e.has("sessionId")) v.sessionId = e["sessionId"].string();
            if (e.has("label")) v.label = e["label"].string();
            if (e.has("approvedProposals")) {
                for (auto &p : e["approvedProposals"].array())
                    v.approvedProposals.push_back(ProposalFromJson(p));
            }
            cursor.entries.push_back(v);
        }
    }
    return cursor;
}

/*static*/ std::string PipelineVersionManager::SerializeSnapshot(const Pipeline &p) {
    // Reuse Storage::SerializePipelines logic for a single pipeline
    std::map<std::string, JsonValue> obj;
    obj["name"] = JsonValue::fromString(p.name);
    obj["mode"] = JsonValue::fromString(p.mode);
    obj["outputMode"] = JsonValue::fromString(p.outputMode);
    obj["outputNaming"] = JsonValue::fromString(p.outputNaming);
    obj["multiMedia"] = JsonValue::fromString(p.multiMedia);
    obj["retryCount"] = JsonValue::fromDouble(p.retryCount);
    obj["retryDelayMs"] = JsonValue::fromDouble(p.retryDelayMs);
    std::vector<JsonValue> steps;
    for (auto &s : p.steps) {
        std::map<std::string, JsonValue> step;
        step["name"] = JsonValue::fromString(s.name);
        step["type"] = JsonValue::fromString(s.type);
        for (auto &kv : s.params) {
            auto parsed = JsonValue::parse(kv.second);
            if (parsed.type() == JsonValue::Null)
                step[kv.first] = JsonValue::fromString(kv.second);
            else
                step[kv.first] = parsed;
        }
        steps.push_back(JsonValue::fromObject(step));
    }
    obj["steps"] = JsonValue::fromArray(steps);
    std::map<std::string, JsonValue> root;
    root["pipeline"] = JsonValue::fromObject(obj);
    return JsonValue::fromObject(root).serialize(true);
}

/*static*/ Pipeline PipelineVersionManager::DeserializeSnapshot(const std::string &json) {
    Pipeline p;
    if (json.empty()) return p;
    auto root = JsonValue::parse(json);
    if (!root.has("pipeline")) return p;
    auto &v = root["pipeline"];
    if (v.has("name")) p.name = v["name"].string();
    if (v.has("mode")) p.mode = v["mode"].string();
    if (v.has("outputMode")) p.outputMode = v["outputMode"].string();
    if (v.has("outputNaming")) p.outputNaming = v["outputNaming"].string();
    if (v.has("multiMedia")) p.multiMedia = v["multiMedia"].string();
    if (v.has("retryCount")) p.retryCount = (int)v["retryCount"].number();
    if (v.has("retryDelayMs")) p.retryDelayMs = (int)v["retryDelayMs"].number();
    if (v.has("steps")) {
        for (auto &s : v["steps"].array()) {
            PipelineStep step;
            if (s.has("name")) step.name = s["name"].string();
            if (s.has("type")) step.type = s["type"].string();
            for (auto &kv : s.object()) step.params[kv.first] = kv.second.serialize();
            p.steps.push_back(step);
        }
    }
    return p;
}

// --- Persistence ---

void PipelineVersionManager::SaveCursor(const VersionCursor &cursor) {
    std::string safeName = SanitizeName(cursor.pipelineName);
    std::wstring relPath = L"opt_versions\\" + ToWide(safeName) + L"\\versions.json";
    storage_.SaveOptimizerData(relPath, SerializeCursor(cursor));
}

VersionCursor PipelineVersionManager::ReadCursor(const std::string &pipelineName) {
    std::string safeName = SanitizeName(pipelineName);
    std::wstring relPath = L"opt_versions\\" + ToWide(safeName) + L"\\versions.json";
    std::string json = storage_.LoadOptimizerData(relPath);
    return DeserializeCursor(json, pipelineName);
}

void PipelineVersionManager::SaveSnapshot(const std::string &pipelineName,
                                          int version, const Pipeline &pipeline) {
    std::string safeName = SanitizeName(pipelineName);
    std::ostringstream ss;
    ss << "opt_versions\\" << safeName << "\\v";
    ss.width(3); ss.fill('0'); ss << version;
    ss << ".json";
    storage_.SaveOptimizerData(ToWide(ss.str()), SerializeSnapshot(pipeline));
}

Pipeline PipelineVersionManager::LoadSnapshot(const std::string &pipelineName, int version) {
    std::string safeName = SanitizeName(pipelineName);
    std::ostringstream ss;
    ss << "opt_versions\\" << safeName << "\\v";
    ss.width(3); ss.fill('0'); ss << version;
    ss << ".json";
    std::string json = storage_.LoadOptimizerData(ToWide(ss.str()));
    return DeserializeSnapshot(json);
}

// --- Public API ---

void PipelineVersionManager::EnsureBaseVersion(const std::string &pipelineName,
                                               const Pipeline &pipeline) {
    auto cursor = ReadCursor(pipelineName);
    if (!cursor.entries.empty()) return;  // already initialized
    cursor.pipelineName = pipelineName;
    cursor.currentVersion = 1;
    cursor.headVersion = 1;
    PipelineVersion v;
    v.version = 1;
    v.timestamp = IsoTimestamp();
    v.sessionId = "";
    v.label = "Initial";
    cursor.entries.push_back(v);
    SaveSnapshot(pipelineName, 1, pipeline);
    SaveCursor(cursor);
}

int PipelineVersionManager::CommitVersion(const std::string &pipelineName,
                                          const Pipeline &pipeline,
                                          const std::string &sessionId,
                                          const std::string &label,
                                          const std::vector<OptEditProposal> &approvedProposals) {
    auto cursor = ReadCursor(pipelineName);
    if (cursor.entries.empty()) {
        // Auto-initialize if EnsureBaseVersion was never called
        EnsureBaseVersion(pipelineName, pipeline);
        cursor = ReadCursor(pipelineName);
    }

    // If we are not at head, drop future versions (detached-HEAD behaviour)
    if (cursor.currentVersion < cursor.headVersion) {
        cursor.entries.erase(
            std::remove_if(cursor.entries.begin(), cursor.entries.end(),
                           [&](const PipelineVersion &e){ return e.version > cursor.currentVersion; }),
            cursor.entries.end());
        cursor.headVersion = cursor.currentVersion;
    }

    int newVersion = cursor.headVersion + 1;
    PipelineVersion v;
    v.version = newVersion;
    v.timestamp = IsoTimestamp();
    v.sessionId = sessionId;
    v.label = label;
    v.approvedProposals = approvedProposals;
    cursor.entries.push_back(v);
    cursor.currentVersion = newVersion;
    cursor.headVersion = newVersion;

    SaveSnapshot(pipelineName, newVersion, pipeline);
    SaveCursor(cursor);
    return newVersion;
}

std::optional<Pipeline> PipelineVersionManager::Undo(const std::string &pipelineName) {
    auto cursor = ReadCursor(pipelineName);
    if (cursor.currentVersion <= 1) return std::nullopt;
    cursor.currentVersion--;
    SaveCursor(cursor);
    return LoadSnapshot(pipelineName, cursor.currentVersion);
}

std::optional<Pipeline> PipelineVersionManager::Redo(const std::string &pipelineName) {
    auto cursor = ReadCursor(pipelineName);
    if (cursor.currentVersion >= cursor.headVersion) return std::nullopt;
    cursor.currentVersion++;
    SaveCursor(cursor);
    return LoadSnapshot(pipelineName, cursor.currentVersion);
}

std::optional<Pipeline> PipelineVersionManager::CheckoutVersion(const std::string &pipelineName,
                                                                 int version) {
    auto cursor = ReadCursor(pipelineName);
    bool found = false;
    for (auto &e : cursor.entries) {
        if (e.version == version) { found = true; break; }
    }
    if (!found) return std::nullopt;
    cursor.currentVersion = version;
    SaveCursor(cursor);
    return LoadSnapshot(pipelineName, version);
}

Pipeline PipelineVersionManager::ReapplyVersion(const std::string &pipelineName,
                                                 int version,
                                                 const Pipeline &current) {
    auto cursor = ReadCursor(pipelineName);
    std::vector<OptEditProposal> proposals;
    for (auto &e : cursor.entries) {
        if (e.version == version) { proposals = e.approvedProposals; break; }
    }
    if (proposals.empty()) return current;

    Pipeline result = current;
    for (auto &p : proposals) {
        if (p.op == "replace" || p.op == "add") {
            for (auto &step : result.steps) {
                if (step.name == p.stepName) {
                    if (p.op == "replace" && step.params.count(p.field) &&
                        step.params[p.field] == p.oldValue) {
                        step.params[p.field] = p.newValue;
                    } else if (p.op == "add") {
                        step.params[p.field] += "\n" + p.newValue;
                    }
                    break;
                }
            }
        } else if (p.op == "delete") {
            for (auto &step : result.steps) {
                if (step.name == p.stepName && step.params.count(p.field) &&
                    step.params[p.field] == p.oldValue) {
                    step.params[p.field] = "";
                    break;
                }
            }
        }
    }

    std::ostringstream label;
    label << "Re-apply v" << version;
    CommitVersion(pipelineName, result, "", label.str(), proposals);
    return result;
}

VersionCursor PipelineVersionManager::GetCursor(const std::string &pipelineName) {
    return ReadCursor(pipelineName);
}
