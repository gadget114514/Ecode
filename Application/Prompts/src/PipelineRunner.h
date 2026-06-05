#pragma once
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <atomic>
#include <map>
#include <memory>
#include <windows.h>
#include <winhttp.h>
#include "NodeData.h"

struct HistoryStep {
    int index = 0;
    std::string name;
    std::string type;
    std::string input;
    std::string output;
    std::map<std::string, std::string> parallelBranches; // branch name → output (parallel step)
    int retries = 0;
    int iterations = 0;
    bool test = false;
    std::string childRunId;
    std::string errorPipelineRunId;
    int promptTokens = 0;
    int completionTokens = 0;
    double durationMs = 0;
    std::string status = "pending";
};

struct HistoryRecord {
    std::string id;
    std::string pipelineName;
    std::string inputNodeId;
    std::string outputNodeId;
    std::string startedAt;
    std::string status;
    std::vector<HistoryStep> steps;
};

// State for a running parallel step
struct ParallelState {
    struct Branch {
        std::string name;
        std::vector<PipelineStep> steps;
    };
    std::vector<Branch> branches;
    int currentBranch = 0;
    std::map<std::string, std::string> results; // branch name → final output
    std::string inputContent;
};

class PipelineRunner {
public:
    PipelineRunner();
    ~PipelineRunner();

    void SetBridgeCallback(std::function<void(const std::string &type, const std::string &json)> cb);

    void Run(const std::string &pipelineName,
             const std::vector<PipelineStep> &steps,
             const std::string &inputContent,
             const std::vector<Attachment> &inputAttachments,
             const std::string &outputMode);

    // Dynamic queue operations
    void AppendStep(const PipelineStep &step);
    void InsertStep(size_t index, const PipelineStep &step);
    void RemoveStep(size_t index);
    void UpdateStep(size_t index, const PipelineStep &step);
    void AppendPipelineSteps(const std::string &pipelineName);

    void Cancel();
    bool IsRunning() const { return running_; }

    void ResumeManual(const std::string &content);
    void CancelManual();

    void RegisterProvider(const std::string &type, const std::string &apiKey, const std::string &baseUrl);
    static std::string JsonEscape(const std::string &s);

private:
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    std::deque<PipelineStep> pendingSteps_;
    std::vector<PipelineStep> originalSteps_;
    std::string inputContent_;
    std::vector<Attachment> inputAttachments_;
    std::string outputMode_;
    std::string pipelineName_;
    std::function<void(const std::string&, const std::string&)> bridgeCb_;

    HINTERNET hSession_{nullptr};
    HINTERNET hRequest_{nullptr};

    std::vector<HistoryStep> historySteps_;
    int currentStepIndex_{-1};
    bool waitingForManual_{false};

    std::string runId_;
    std::string startedAt_;

    // Parallel step state
    std::unique_ptr<ParallelState> parallelState_;
    void ExecuteNextParallelBranch();

    void RunNextStep();
    void ExecuteStep(const PipelineStep &step);
    void HandleError(const std::string &message);
    void PostBridge(const std::string &type, const std::string &json);
    std::string BuildMetaJson();

    std::vector<PipelineStep> executedStepParams_;

    std::map<std::string, class AIProvider*> providers_;

    static void CALLBACK WinHttpCallback(HINTERNET hInternet, DWORD_PTR dwContext,
                                          DWORD dwInternetStatus, LPVOID lpvStatusInformation,
                                          DWORD dwStatusInformationLength);
    std::string sseBuffer_;
    void ProcessSSE(const std::string &data);
};
