#pragma once
#include <string>
#include <vector>
#include <functional>
#include "NodeData.h"
#include "PipelineRunner.h"  // HistoryRecord, HistoryStep

struct OptEditProposal {
    std::string op;          // "replace" | "add" | "delete"
    std::string stepName;
    std::string field;       // "systemPrompt" | "userPrompt"
    std::string oldValue;
    std::string newValue;
    std::string rationale;
};

struct OptSession {
    std::string pipelineName;
    std::string sessionId;
    std::vector<OptEditProposal> proposals;
    std::vector<OptEditProposal> rejectedBuffer;  // loaded from disk + accumulated in this session
};

class Storage;
class AIProvider;

class PipelineOptimizer {
public:
    explicit PipelineOptimizer(Storage &storage);

    // Load evaluated history, call optimizer AI, post "optimize_proposals" or "optimize_error"
    // via bridgeCb. Runs on a background thread.
    void StartSession(const std::string &pipelineName,
                      const Pipeline &pipeline,
                      int historyLimit,
                      int maxEditsPerStep,
                      const std::string &providerType,
                      const std::string &apiKey,
                      const std::string &baseUrl,
                      const std::string &model,
                      std::function<void(const std::string &type, const std::string &json)> bridgeCb);

    // Apply approved proposals to pipeline; return updated pipeline.
    // rejected proposals are accumulated into session.rejectedBuffer for persistence.
    static Pipeline ApplyApprovals(const Pipeline &pipeline,
                                   const std::vector<int> &approvedIndices,
                                   const std::vector<int> &rejectedIndices,
                                   OptSession &session);

    // Rejected-edit buffer persistence
    void SaveRejectedBuffer(const std::string &pipelineName,
                            const std::vector<OptEditProposal> &buffer);
    std::vector<OptEditProposal> LoadRejectedBuffer(const std::string &pipelineName);

    // Build the prompt sent to the optimizer model
    static std::string BuildOptimizerPrompt(
        const Pipeline &pipeline,
        const std::vector<HistoryRecord> &okSamples,
        const std::vector<HistoryRecord> &rejectedSamples,
        const std::vector<std::string> &pinnedContents,
        const std::vector<OptEditProposal> &rejectedBuffer,
        int maxEditsPerStep);

    // Parse the AI's JSON array response into proposals (empty on parse failure)
    static std::vector<OptEditProposal> ParseProposals(const std::string &aiResponse);

    // Serialize/deserialize helpers for bridge messages
    static std::string SerializeProposals(const std::vector<OptEditProposal> &proposals);
    static std::string SerializeSession(const OptSession &session);

private:
    Storage &storage_;

    static std::string SanitizeName(const std::string &name);

    // Collect evaluated history records from disk
    std::vector<HistoryRecord> LoadEvaluatedHistory(const std::string &pipelineName,
                                                    const std::string &evaluation,
                                                    int limit);

    // Collect pinned node contents relevant to this pipeline
    std::vector<std::string> CollectPinnedContent(const std::string &pipelineName);

    static std::string TruncateUtf8(const std::string &s, size_t maxBytes);
    static std::string IsoTimestamp();
};
