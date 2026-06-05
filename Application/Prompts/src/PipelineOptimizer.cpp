#include "PipelineOptimizer.h"
#include "Storage.h"
#include "AIProvider.h"
#include "JsonParser.h"
#include <thread>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <windows.h>

PipelineOptimizer::PipelineOptimizer(Storage &storage)
    : storage_(storage) {}

/*static*/ std::string PipelineOptimizer::SanitizeName(const std::string &name) {
    std::string safe;
    for (char c : name) {
        if (std::isalnum((unsigned char)c) || c == '-') safe += c;
        else safe += '_';
    }
    return safe;
}

static std::wstring OptimizerToWide(const std::string &s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

/*static*/ std::string PipelineOptimizer::IsoTimestamp() {
    time_t t = time(nullptr);
    struct tm tm_info;
    gmtime_s(&tm_info, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_info);
    return buf;
}

/*static*/ std::string PipelineOptimizer::TruncateUtf8(const std::string &s, size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    size_t i = maxBytes;
    while (i > 0 && (s[i] & 0xC0) == 0x80) --i;
    return s.substr(0, i) + "...";
}

// --- Rejected buffer persistence ---

void PipelineOptimizer::SaveRejectedBuffer(const std::string &pipelineName,
                                           const std::vector<OptEditProposal> &buffer) {
    std::vector<JsonValue> arr;
    for (auto &p : buffer) {
        std::map<std::string, JsonValue> obj;
        obj["op"] = JsonValue::fromString(p.op);
        obj["stepName"] = JsonValue::fromString(p.stepName);
        obj["field"] = JsonValue::fromString(p.field);
        obj["oldValue"] = JsonValue::fromString(p.oldValue);
        obj["newValue"] = JsonValue::fromString(p.newValue);
        obj["rationale"] = JsonValue::fromString(p.rationale);
        arr.push_back(JsonValue::fromObject(obj));
    }
    // Cap at 50 entries (keep most recent)
    if (arr.size() > 50) arr.erase(arr.begin(), arr.begin() + (arr.size() - 50));
    std::map<std::string, JsonValue> root;
    root["entries"] = JsonValue::fromArray(arr);
    std::string json = JsonValue::fromObject(root).serialize(true);
    std::string safeName = SanitizeName(pipelineName);
    storage_.SaveOptimizerData(OptimizerToWide("opt_rejected_" + safeName + ".json"), json);
}

std::vector<OptEditProposal> PipelineOptimizer::LoadRejectedBuffer(const std::string &pipelineName) {
    std::string safeName = SanitizeName(pipelineName);
    std::string json = storage_.LoadOptimizerData(
        OptimizerToWide("opt_rejected_" + safeName + ".json"));
    std::vector<OptEditProposal> result;
    if (json.empty()) return result;
    auto val = JsonValue::parse(json);
    if (!val.has("entries")) return result;
    for (auto &e : val["entries"].array()) {
        OptEditProposal p;
        if (e.has("op")) p.op = e["op"].string();
        if (e.has("stepName")) p.stepName = e["stepName"].string();
        if (e.has("field")) p.field = e["field"].string();
        if (e.has("oldValue")) p.oldValue = e["oldValue"].string();
        if (e.has("newValue")) p.newValue = e["newValue"].string();
        if (e.has("rationale")) p.rationale = e["rationale"].string();
        result.push_back(p);
    }
    return result;
}

// --- History loading ---

std::vector<HistoryRecord> PipelineOptimizer::LoadEvaluatedHistory(
    const std::string &pipelineName,
    const std::string &evaluation,
    int limit)
{
    std::vector<HistoryRecord> result;
    auto files = storage_.ListHistory();
    int count = 0;
    for (auto &f : files) {
        if (count >= limit) break;
        std::string json = storage_.LoadHistoryRecord(f);
        if (json.empty()) continue;
        auto val = JsonValue::parse(json);
        // Filter by pipeline name
        if (val.has("pipelineName") && val["pipelineName"].string() != pipelineName) continue;
        // Filter by evaluation (run-level)
        std::string eval = val.has("evaluation") ? val["evaluation"].string() : "";
        if (!evaluation.empty() && eval != evaluation) continue;

        HistoryRecord rec;
        if (val.has("id")) rec.id = val["id"].string();
        if (val.has("pipelineName")) rec.pipelineName = val["pipelineName"].string();
        if (val.has("startedAt")) rec.startedAt = val["startedAt"].string();
        if (val.has("status")) rec.status = val["status"].string();
        if (val.has("evaluation")) rec.evaluation = val["evaluation"].string();
        if (val.has("steps")) {
            for (auto &s : val["steps"].array()) {
                HistoryStep step;
                if (s.has("index")) step.index = (int)s["index"].number();
                if (s.has("name")) step.name = s["name"].string();
                if (s.has("type")) step.type = s["type"].string();
                if (s.has("input")) step.input = s["input"].string();
                if (s.has("output")) step.output = s["output"].string();
                if (s.has("status")) step.status = s["status"].string();
                if (s.has("evaluation")) step.evaluation = s["evaluation"].string();
                if (s.has("evaluationNote")) step.evaluationNote = s["evaluationNote"].string();
                rec.steps.push_back(step);
            }
        }
        result.push_back(rec);
        count++;
    }
    return result;
}

std::vector<std::string> PipelineOptimizer::CollectPinnedContent(const std::string &pipelineName) {
    // Collect pinned content from history steps for this pipeline
    std::vector<std::string> result;
    auto files = storage_.ListHistory();
    for (auto &f : files) {
        std::string json = storage_.LoadHistoryRecord(f);
        if (json.empty()) continue;
        auto val = JsonValue::parse(json);
        if (val.has("pipelineName") && val["pipelineName"].string() != pipelineName) continue;
        if (!val.has("steps")) continue;
        for (auto &s : val["steps"].array()) {
            std::string eval = s.has("evaluation") ? s["evaluation"].string() : "";
            if (eval == "pinned") {
                std::string output = s.has("output") ? s["output"].string() : "";
                if (!output.empty()) result.push_back(output);
            }
        }
    }
    return result;
}

// --- Optimizer prompt ---

/*static*/ std::string PipelineOptimizer::BuildOptimizerPrompt(
    const Pipeline &pipeline,
    const std::vector<HistoryRecord> &okSamples,
    const std::vector<HistoryRecord> &rejectedSamples,
    const std::vector<std::string> &pinnedContents,
    const std::vector<OptEditProposal> &rejectedBuffer,
    int maxEditsPerStep)
{
    std::ostringstream ss;

    ss << "## Current Pipeline Definition\n";
    ss << "Name: " << pipeline.name << "\n";
    ss << "Steps:\n";
    for (auto &step : pipeline.steps) {
        ss << "  ### Step: " << step.name << " (type: " << step.type << ")\n";
        auto spit = step.params.find("systemPrompt");
        if (spit != step.params.end())
            ss << "  systemPrompt: " << TruncateUtf8(spit->second, 1000) << "\n";
        auto upit = step.params.find("userPrompt");
        if (upit != step.params.end())
            ss << "  userPrompt: " << TruncateUtf8(upit->second, 1000) << "\n";
    }

    ss << "\n## Pinned Content (DO NOT modify)\n";
    if (pinnedContents.empty()) {
        ss << "None\n";
    } else {
        for (size_t i = 0; i < pinnedContents.size(); i++)
            ss << "[pin " << (i+1) << "] " << TruncateUtf8(pinnedContents[i], 300) << "\n";
    }

    auto dumpSamples = [&](const std::vector<HistoryRecord> &samples, const std::string &label) {
        ss << "\n## " << label << " (" << samples.size() << " runs)\n";
        for (auto &rec : samples) {
            ss << "Run: " << rec.id << " | " << rec.startedAt << "\n";
            for (auto &step : rec.steps) {
                ss << "  Step \"" << step.name << "\" (" << step.type << ")";
                if (!step.evaluation.empty()) ss << " [" << step.evaluation << "]";
                ss << "\n";
                if (!step.input.empty())
                    ss << "    Input: " << TruncateUtf8(step.input, 400) << "\n";
                if (!step.output.empty())
                    ss << "    Output: " << TruncateUtf8(step.output, 400) << "\n";
            }
        }
    };

    dumpSamples(okSamples, "OK (success) samples");
    dumpSamples(rejectedSamples, "Rejected (failure) samples");

    ss << "\n## Previously Rejected Proposals (do NOT re-propose)\n";
    if (rejectedBuffer.empty()) {
        ss << "None\n";
    } else {
        for (auto &p : rejectedBuffer)
            ss << "- " << p.op << " | " << p.stepName << "." << p.field
               << " | " << p.rationale << "\n";
    }

    ss << "\n## Task\n";
    ss << "Analyze the traces. Propose improvements for the REJECTED/failed samples.\n";
    ss << "Preserve patterns shown in the OK samples.\n";
    ss << "Each step may have at most " << maxEditsPerStep << " operations.\n";

    return ss.str();
}

static const char *kSystemPrompt = R"(You are a pipeline prompt optimizer.
Analyze execution traces of an AI pipeline and propose minimal, targeted improvements
to the systemPrompt and userPrompt fields of pipeline steps.

Rules:
- Propose at most {maxEdits} operations per step.
- Operations: "replace" (change text), "add" (append instruction), "delete" (remove).
- Pinned content must NOT be modified.
- Do NOT re-propose previously rejected edits.
- Output a single JSON array only — no other text.

Output format (strict JSON array):
[
  {
    "op": "replace"|"add"|"delete",
    "stepName": "<step name>",
    "field": "systemPrompt"|"userPrompt",
    "oldValue": "<current text or empty for add>",
    "newValue": "<proposed text or empty for delete>",
    "rationale": "<one sentence>"
  }
]
)";

// --- Parse proposals ---

/*static*/ std::vector<OptEditProposal> PipelineOptimizer::ParseProposals(
    const std::string &aiResponse)
{
    std::vector<OptEditProposal> result;
    if (aiResponse.empty()) return result;
    // Find JSON array in response
    auto startPos = aiResponse.find('[');
    auto endPos = aiResponse.rfind(']');
    if (startPos == std::string::npos || endPos == std::string::npos) return result;
    std::string json = aiResponse.substr(startPos, endPos - startPos + 1);
    auto val = JsonValue::parse(json);
    if (val.type() != JsonValue::Array) return result;
    for (auto &e : val.array()) {
        OptEditProposal p;
        if (e.has("op")) p.op = e["op"].string();
        if (e.has("stepName")) p.stepName = e["stepName"].string();
        if (e.has("field")) p.field = e["field"].string();
        if (e.has("oldValue")) p.oldValue = e["oldValue"].string();
        if (e.has("newValue")) p.newValue = e["newValue"].string();
        if (e.has("rationale")) p.rationale = e["rationale"].string();
        if (!p.op.empty() && !p.stepName.empty() && !p.field.empty())
            result.push_back(p);
    }
    return result;
}

// --- Serialize for bridge ---

/*static*/ std::string PipelineOptimizer::SerializeProposals(
    const std::vector<OptEditProposal> &proposals)
{
    std::vector<JsonValue> arr;
    for (auto &p : proposals) {
        std::map<std::string, JsonValue> obj;
        obj["op"] = JsonValue::fromString(p.op);
        obj["stepName"] = JsonValue::fromString(p.stepName);
        obj["field"] = JsonValue::fromString(p.field);
        obj["oldValue"] = JsonValue::fromString(p.oldValue);
        obj["newValue"] = JsonValue::fromString(p.newValue);
        obj["rationale"] = JsonValue::fromString(p.rationale);
        arr.push_back(JsonValue::fromObject(obj));
    }
    return JsonValue::fromArray(arr).serialize();
}

/*static*/ std::string PipelineOptimizer::SerializeSession(const OptSession &session) {
    std::map<std::string, JsonValue> obj;
    obj["pipelineName"] = JsonValue::fromString(session.pipelineName);
    obj["sessionId"] = JsonValue::fromString(session.sessionId);
    return JsonValue::fromObject(obj).serialize();
}

// --- ApplyApprovals ---

/*static*/ Pipeline PipelineOptimizer::ApplyApprovals(
    const Pipeline &pipeline,
    const std::vector<int> &approvedIndices,
    const std::vector<int> &rejectedIndices,
    OptSession &session)
{
    Pipeline result = pipeline;
    for (int idx : approvedIndices) {
        if (idx < 0 || idx >= (int)session.proposals.size()) continue;
        auto &p = session.proposals[idx];
        for (auto &step : result.steps) {
            if (step.name != p.stepName) continue;
            if (p.op == "replace") {
                auto it = step.params.find(p.field);
                if (it != step.params.end() && it->second == p.oldValue)
                    it->second = p.newValue;
            } else if (p.op == "add") {
                step.params[p.field] += (step.params[p.field].empty() ? "" : "\n") + p.newValue;
            } else if (p.op == "delete") {
                auto it = step.params.find(p.field);
                if (it != step.params.end() && it->second == p.oldValue)
                    it->second = "";
            }
            break;
        }
    }
    // Accumulate rejected proposals into buffer
    for (int idx : rejectedIndices) {
        if (idx >= 0 && idx < (int)session.proposals.size())
            session.rejectedBuffer.push_back(session.proposals[idx]);
    }
    return result;
}

// --- StartSession (async) ---

void PipelineOptimizer::StartSession(
    const std::string &pipelineName,
    const Pipeline &pipeline,
    int historyLimit,
    int maxEditsPerStep,
    const std::string &providerType,
    const std::string &apiKey,
    const std::string &baseUrl,
    const std::string &model,
    std::function<void(const std::string &type, const std::string &json)> bridgeCb)
{
    // Capture everything by value for the thread
    std::thread([=]() mutable {
        auto postProgress = [&](const std::string &msg) {
            std::map<std::string, JsonValue> obj;
            obj["message"] = JsonValue::fromString(msg);
            bridgeCb("optimize_progress", JsonValue::fromObject(obj).serialize());
        };

        postProgress("Loading execution history...");
        auto okSamples = LoadEvaluatedHistory(pipelineName, "ok", historyLimit);
        auto rejSamples = LoadEvaluatedHistory(pipelineName, "rejected", historyLimit);

        // If no evaluated history, also load completed (unevaluated) runs as fallback
        if (okSamples.empty() && rejSamples.empty()) {
            auto allSamples = LoadEvaluatedHistory(pipelineName, "", historyLimit);
            // Split by step-level status as heuristic
            for (auto &rec : allSamples) {
                bool anyFail = false;
                for (auto &s : rec.steps)
                    if (s.status != "completed" && s.status != "ok") { anyFail = true; break; }
                if (anyFail) rejSamples.push_back(rec);
                else okSamples.push_back(rec);
            }
        }

        if (okSamples.empty() && rejSamples.empty()) {
            std::map<std::string, JsonValue> err;
            err["message"] = JsonValue::fromString("No execution history found for pipeline: " + pipelineName);
            bridgeCb("optimize_error", JsonValue::fromObject(err).serialize());
            return;
        }

        postProgress("Collecting pinned content...");
        auto pinnedContents = CollectPinnedContent(pipelineName);
        auto rejectedBuffer = LoadRejectedBuffer(pipelineName);

        postProgress("Building optimizer prompt...");
        std::string userPrompt = BuildOptimizerPrompt(
            pipeline, okSamples, rejSamples, pinnedContents, rejectedBuffer, maxEditsPerStep);

        // Build system prompt
        std::string sysPrompt = kSystemPrompt;
        // Replace placeholder
        auto replaceAll = [](std::string s, const std::string &from, const std::string &to) {
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.size(), to);
                pos += to.size();
            }
            return s;
        };
        sysPrompt = replaceAll(sysPrompt, "{maxEdits}", std::to_string(maxEditsPerStep));

        postProgress("Calling optimizer AI...");
        AIProvider *provider = nullptr;
        try {
            provider = AIProvider::Create(providerType, apiKey, baseUrl);
            if (!provider) throw std::runtime_error("Failed to create provider: " + providerType);

            AIRequest req;
            req.model = model;
            req.systemPrompt = sysPrompt;
            req.userPrompt = userPrompt;
            req.temperature = 0.3;
            req.maxTokens = 2048;

            AIResponse resp = provider->Call(req);
            delete provider;
            provider = nullptr;

            auto proposals = ParseProposals(resp.content);
            if (proposals.empty()) {
                std::map<std::string, JsonValue> err;
                err["message"] = JsonValue::fromString("Optimizer returned no valid proposals.");
                bridgeCb("optimize_error", JsonValue::fromObject(err).serialize());
                return;
            }

            // Build session ID
            time_t t = time(nullptr);
            struct tm tm_info;
            gmtime_s(&tm_info, &t);
            char buf[32];
            strftime(buf, sizeof(buf), "opt_%Y%m%d_%H%M%S", &tm_info);
            std::string sessionId = buf;

            // Count evaluation summary
            int okCount = (int)okSamples.size();
            int rejCount = (int)rejSamples.size();
            int pinCount = (int)pinnedContents.size();

            std::map<std::string, JsonValue> payload;
            payload["sessionId"] = JsonValue::fromString(sessionId);
            payload["pipelineName"] = JsonValue::fromString(pipelineName);

            auto parsedProposals = JsonValue::parse(SerializeProposals(proposals));
            payload["proposals"] = parsedProposals;

            std::map<std::string, JsonValue> summary;
            summary["okCount"] = JsonValue::fromDouble(okCount);
            summary["rejectedCount"] = JsonValue::fromDouble(rejCount);
            summary["pinnedCount"] = JsonValue::fromDouble(pinCount);
            payload["evaluationSummary"] = JsonValue::fromObject(summary);

            bridgeCb("optimize_proposals", JsonValue::fromObject(payload).serialize());

        } catch (const std::exception &ex) {
            if (provider) { delete provider; provider = nullptr; }
            std::map<std::string, JsonValue> err;
            err["message"] = JsonValue::fromString(std::string("Optimizer error: ") + ex.what());
            bridgeCb("optimize_error", JsonValue::fromObject(err).serialize());
        }
    }).detach();
}
