#pragma once
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <atomic>
#include <windows.h>
#include <winhttp.h>
#include "NodeData.h"

struct HistoryStep {
    int index = 0;
    std::string name;
    std::string type;
    std::string input;
    std::string output;
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

class PipelineRunner {
public:
    PipelineRunner();
    ~PipelineRunner();
    
    void SetBridgeCallback(std::function<void(const std::string &type, const std::string &json)> cb);
    
    // Execute pipeline with given input
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
    
    // Cancel current execution
    void Cancel();
    bool IsRunning() const { return running_; }
    
    // Register provider
    void RegisterProvider(const std::string &type, const std::string &apiKey, const std::string &baseUrl);

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
    
    void RunNextStep();
    void ExecuteStep(const PipelineStep &step);
    void HandleError(const std::string &message);
    void PostBridge(const std::string &type, const std::string &json);
    
    // Provider management
    std::map<std::string, class AIProvider*> providers_;
    
    // WinHTTP async callback
    static void CALLBACK WinHttpCallback(HINTERNET hInternet, DWORD_PTR dwContext,
                                          DWORD dwInternetStatus, LPVOID lpvStatusInformation,
                                          DWORD dwStatusInformationLength);
    std::string sseBuffer_;
    void ProcessSSE(const std::string &data);
};
