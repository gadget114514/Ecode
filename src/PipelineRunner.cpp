#include "PipelineRunner.h"
#include "AIProvider.h"
#include "JsonParser.h"
#include <sstream>
#include <algorithm>

PipelineRunner::PipelineRunner() {}
PipelineRunner::~PipelineRunner() { Cancel(); }

void PipelineRunner::SetBridgeCallback(std::function<void(const std::string&, const std::string&)> cb) {
    bridgeCb_ = cb;
}

void PipelineRunner::PostBridge(const std::string &type, const std::string &json) {
    if (bridgeCb_) bridgeCb_(type, json);
}

void PipelineRunner::RegisterProvider(const std::string &type, const std::string &apiKey, const std::string &baseUrl) {
    auto *provider = AIProvider::Create(type, apiKey, baseUrl);
    if (provider) providers_[type] = provider;
}

void PipelineRunner::Run(const std::string &pipelineName,
                          const std::vector<PipelineStep> &steps,
                          const std::string &inputContent,
                          const std::vector<Attachment> &inputAttachments,
                          const std::string &outputMode) {
    if (running_) return;
    
    pipelineName_ = pipelineName;
    originalSteps_ = steps;
    inputContent_ = inputContent;
    inputAttachments_ = inputAttachments;
    outputMode_ = outputMode;
    cancelled_ = false;
    running_ = true;
    historySteps_.clear();
    currentStepIndex_ = -1;
    
    // Initialize pending queue
    pendingSteps_.clear();
    for (auto &s : steps) pendingSteps_.push_back(s);
    
    // Build initial history steps
    for (int i = 0; i < (int)steps.size(); i++) {
        HistoryStep hs;
        hs.index = i;
        hs.name = steps[i].name;
        hs.type = steps[i].type;
        hs.status = "pending";
        historySteps_.push_back(hs);
    }
    
    // Set step 0 input
    if (!historySteps_.empty()) {
        historySteps_[0].input = inputContent_;
        historySteps_[0].status = "running";
    }
    
    PostBridge("step_started", "{\"index\":0,\"name\":\"" + steps[0].name + "\"}");
    
    RunNextStep();
}

void PipelineRunner::RunNextStep() {
    if (cancelled_ || pendingSteps_.empty()) {
        running_ = false;
        if (!cancelled_) {
            PostBridge("step_done", "{\"status\":\"completed\"}");
        } else {
            PostBridge("pipeline_error", "{\"message\":\"Canceled\"}");
        }
        return;
    }
    
    currentStepIndex_++;
    auto step = pendingSteps_.front();
    pendingSteps_.pop_front();
    
    PostBridge("step_started", "{\"index\":" + std::to_string(currentStepIndex_) +
               ",\"name\":\"" + step.name + "\"}");
    
    ExecuteStep(step);
}

void PipelineRunner::ExecuteStep(const PipelineStep &step) {
    std::string type = step.type;
    
    if (type == "ai") {
        // Execute AI step
        auto providerIt = providers_.find(step.params.count("provider") ? step.params.at("provider") : "openai");
        if (providerIt == providers_.end()) {
            HandleError("Provider not configured: " + step.params.count("provider") ? step.params.at("provider") : "unknown");
            return;
        }
        
        AIRequest req;
        req.model = step.params.count("model") ? step.params.at("model") : "gpt-4.1";
        req.systemPrompt = step.params.count("systemPrompt") ? step.params.at("systemPrompt") : "";
        req.userPrompt = step.params.count("userPrompt") ? step.params.at("userPrompt") : "{content}";
        
        // Replace placeholders
        auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.length(), to);
                pos += to.length();
            }
        };
        replaceAll(req.userPrompt, "{content}", inputContent_);
        if (currentStepIndex_ > 0 && currentStepIndex_ - 1 < (int)historySteps_.size()) {
            replaceAll(req.userPrompt, "{result}", historySteps_[currentStepIndex_ - 1].output);
        } else {
            replaceAll(req.userPrompt, "{result}", inputContent_);
        }
        
        req.attachments = inputAttachments_;
        
        providerIt->second->CallStreaming(req,
            [this, &step](const std::string &chunk) {
                PostBridge("stream_chunk", "{\"stepIndex\":" + std::to_string(currentStepIndex_) + ",\"text\":\"" + chunk + "\"}");
            },
            [this, &step](const AIResponse &resp) {
                if (currentStepIndex_ < (int)historySteps_.size()) {
                    historySteps_[currentStepIndex_].output = resp.content;
                    historySteps_[currentStepIndex_].status = "completed";
                    historySteps_[currentStepIndex_].promptTokens = resp.promptTokens;
                    historySteps_[currentStepIndex_].completionTokens = resp.completionTokens;
                }
                PostBridge("step_done", "{\"index\":" + std::to_string(currentStepIndex_) +
                           ",\"tokens\":" + std::to_string(resp.completionTokens) + "}");
                RunNextStep();
            },
            [this](const std::string &error) {
                HandleError(error);
            });
    } else if (type == "condition") {
        // Evaluate condition
        std::string expr = step.params.count("expression") ? step.params.at("expression") : "{result}";
        std::string op = step.params.count("operator") ? step.params.at("operator") : "contains";
        std::string value = step.params.count("value") ? step.params.at("value") : "";
        
        // Replace placeholders in expression
        auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.length(), to);
                pos += to.length();
            }
        };
        std::string resolved = expr;
        replaceAll(resolved, "{result}", currentStepIndex_ > 0 ? historySteps_[currentStepIndex_ - 1].output : inputContent_);
        
        bool matched = false;
        if (op == "contains") matched = resolved.find(value) != std::string::npos;
        else if (op == "equals") matched = (resolved == value);
        
        if (matched) {
            std::string action = step.params.count("onTrue") ? step.params.at("onTrue") : "next_step";
            (void)action;
        } else {
            std::string action = step.params.count("onFalse") ? step.params.at("onFalse") : "next_step";
            (void)action;
        }
        
        if (currentStepIndex_ < (int)historySteps_.size()) {
            historySteps_[currentStepIndex_].status = "completed";
        }
        PostBridge("step_done", "{\"index\":" + std::to_string(currentStepIndex_) + "}");
        RunNextStep();
    } else {
        // Unknown step type — skip
        if (currentStepIndex_ < (int)historySteps_.size()) {
            historySteps_[currentStepIndex_].status = "skipped";
        }
        RunNextStep();
    }
}

void PipelineRunner::HandleError(const std::string &message) {
    running_ = false;
    PostBridge("pipeline_error", "{\"message\":\"" + message + "\"}");
}

void PipelineRunner::Cancel() {
    cancelled_ = true;
    running_ = false;
    pendingSteps_.clear();
    if (hRequest_) {
        WinHttpCloseHandle(hRequest_);
        hRequest_ = nullptr;
    }
    if (hSession_) {
        WinHttpCloseHandle(hSession_);
        hSession_ = nullptr;
    }
}

void PipelineRunner::AppendStep(const PipelineStep &step) {
    if (running_) pendingSteps_.push_back(step);
}

void PipelineRunner::InsertStep(size_t index, const PipelineStep &step) {
    if (running_ && index <= pendingSteps_.size()) {
        auto it = pendingSteps_.begin();
        std::advance(it, index);
        pendingSteps_.insert(it, step);
    }
}

void PipelineRunner::RemoveStep(size_t index) {
    if (running_ && index < pendingSteps_.size()) {
        auto it = pendingSteps_.begin();
        std::advance(it, index);
        pendingSteps_.erase(it);
    }
}

void PipelineRunner::UpdateStep(size_t index, const PipelineStep &step) {
    if (running_ && index < pendingSteps_.size()) {
        auto it = pendingSteps_.begin();
        std::advance(it, index);
        *it = step;
    }
}

void PipelineRunner::AppendPipelineSteps(const std::string &pipelineName) {
    // Would need access to Storage to load pipeline by name
    (void)pipelineName;
}
