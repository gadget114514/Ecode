#include "PipelineRunner.h"
#include "AIProvider.h"
#include "JsonParser.h"
#include <sstream>
#include <algorithm>
#include <ctime>
#include <thread>

PipelineRunner::PipelineRunner() {}
PipelineRunner::~PipelineRunner() { Cancel(); }

/*static*/ std::string PipelineRunner::JsonEscape(const std::string &s) {
    std::string out;
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

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
    
    // Record step params for metadata
    executedStepParams_.clear();
    for (auto &s : steps) executedStepParams_.push_back(s);

    PostBridge("step_started", "{\"index\":0,\"name\":\"" + steps[0].name + "\"}");

    RunNextStep();
}

void PipelineRunner::RunNextStep() {
    if (cancelled_ || pendingSteps_.empty()) {
        running_ = false;
        if (!cancelled_) {
            PostBridge("pipeline_completed", BuildMetaJson());
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
            HandleError("Provider not configured: " + (step.params.count("provider") ? step.params.at("provider") : "unknown"));
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
    } else if (type == "manual") {
        std::string mode   = step.params.count("mode")   ? step.params.at("mode")   : "view";
        std::string prompt = step.params.count("prompt") ? step.params.at("prompt") : "";
        std::string content = currentStepIndex_ > 0 && currentStepIndex_ - 1 < (int)historySteps_.size()
                              ? historySteps_[currentStepIndex_ - 1].output
                              : inputContent_;

        waitingForManual_ = true;

        if (mode == "compare") {
            // Build branches JSON from previous parallel step's results
            std::string branchesJson = "[";
            bool first = true;
            if (currentStepIndex_ > 0 && currentStepIndex_ - 1 < (int)historySteps_.size()) {
                for (auto &kv : historySteps_[currentStepIndex_ - 1].parallelBranches) {
                    if (!first) branchesJson += ",";
                    first = false;
                    branchesJson += "{\"name\":\"" + JsonEscape(kv.first) + "\""
                                  + ",\"content\":\"" + JsonEscape(kv.second) + "\"}";
                }
            }
            branchesJson += "]";
            PostBridge("manual_step_pause",
                "{\"index\":" + std::to_string(currentStepIndex_) +
                ",\"mode\":\"compare\"" +
                ",\"prompt\":\"" + JsonEscape(prompt) + "\"" +
                ",\"branches\":" + branchesJson + "}");
        } else {
            std::string choicesJson = step.params.count("choices") ? step.params.at("choices") : "[]";
            PostBridge("manual_step_pause",
                "{\"index\":" + std::to_string(currentStepIndex_) +
                ",\"mode\":\"" + mode + "\"" +
                ",\"prompt\":\"" + JsonEscape(prompt) + "\"" +
                ",\"content\":\"" + JsonEscape(content) + "\"" +
                ",\"choices\":" + choicesJson + "}");
        }
        return; // wait for ResumeManual()

    } else if (type == "command") {
        std::string cmd       = step.params.count("command")    ? step.params.at("command")    : "";
        std::string argsStr   = step.params.count("args")       ? step.params.at("args")       : "[]";
        std::string workDir   = step.params.count("workingDir") ? step.params.at("workingDir") : "";
        std::string resultAs  = step.params.count("resultAs")   ? step.params.at("resultAs")   : "text";
        int timeoutSec        = step.params.count("timeout")    ? std::stoi(step.params.at("timeout")) : 60;

        // Resolve current content
        std::string content = currentStepIndex_ > 0 && currentStepIndex_ - 1 < (int)historySteps_.size()
                              ? historySteps_[currentStepIndex_ - 1].output : inputContent_;

        // Build argument list from JSON array string
        std::vector<std::string> args;
        auto argsVal = JsonValue::parse(argsStr);
        for (auto &a : argsVal.array()) args.push_back(a.string());

        int stepIdx = currentStepIndex_;

        std::thread([this, cmd, args, workDir, content, resultAs, timeoutSec, stepIdx]() {
            // Write content to temp file for {content_file} placeholder
            wchar_t tempDir[MAX_PATH], tempFile[MAX_PATH] = {};
            GetTempPathW(MAX_PATH, tempDir);
            GetTempFileNameW(tempDir, L"pro", 0, tempFile);
            {
                HANDLE hf = CreateFileW(tempFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
                if (hf != INVALID_HANDLE_VALUE) {
                    DWORD written;
                    WriteFile(hf, content.c_str(), (DWORD)content.size(), &written, nullptr);
                    CloseHandle(hf);
                }
            }
            std::string tempFileA;
            { int n = WideCharToMultiByte(CP_UTF8,0,tempFile,-1,nullptr,0,nullptr,nullptr);
              tempFileA.resize(n); WideCharToMultiByte(CP_UTF8,0,tempFile,-1,&tempFileA[0],n,nullptr,nullptr); }
            if (!tempFileA.empty() && tempFileA.back() == '\0') tempFileA.pop_back();

            // Build command line
            auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
                size_t pos = 0;
                while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
            };
            std::string cmdLine = "\"" + cmd + "\"";
            for (auto arg : args) {
                replaceAll(arg, "{content_file}", tempFileA);
                replaceAll(arg, "{content}", content);
                replaceAll(arg, "{result}", content);
                cmdLine += " \"" + arg + "\"";
            }

            // Create stdout pipe
            HANDLE hRead, hWrite;
            SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
            CreatePipe(&hRead, &hWrite, &sa, 0);
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si = {};
            si.cb = sizeof(si);
            si.hStdOutput = hWrite;
            si.hStdError  = hWrite;
            si.dwFlags    = STARTF_USESTDHANDLES;

            // Working directory
            std::string wd = workDir;
            replaceAll(wd, "%APPDATA%", getenv("APPDATA") ? getenv("APPDATA") : "");

            PROCESS_INFORMATION pi = {};
            bool launched = CreateProcessA(nullptr, &cmdLine[0], nullptr, nullptr, TRUE,
                                            CREATE_NO_WINDOW, nullptr,
                                            wd.empty() ? nullptr : wd.c_str(), &si, &pi) != 0;
            CloseHandle(hWrite);

            std::string output;
            if (launched) {
                char buf[4096]; DWORD read;
                while (ReadFile(hRead, buf, sizeof(buf), &read, nullptr) && read > 0) {
                    std::string chunk(buf, read);
                    output += chunk;
                    PostBridge("stream_chunk",
                        "{\"stepIndex\":" + std::to_string(stepIdx) +
                        ",\"text\":\"" + JsonEscape(chunk) + "\"}");
                }
                WaitForSingleObject(pi.hProcess, (DWORD)(timeoutSec * 1000));
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            } else {
                output = "[command launch failed: " + cmd + "]";
                PostBridge("stream_chunk",
                    "{\"stepIndex\":" + std::to_string(stepIdx) +
                    ",\"text\":\"" + JsonEscape(output) + "\"}");
            }
            CloseHandle(hRead);
            DeleteFileW(tempFile);

            // Store result
            if (stepIdx < (int)historySteps_.size()) {
                historySteps_[stepIdx].output = (resultAs == "text") ? output : "";
                historySteps_[stepIdx].status = "completed";
            }
            PostBridge("step_done", "{\"index\":" + std::to_string(stepIdx) + "}");
            RunNextStep();
        }).detach();
        return;

    } else if (type == "parallel") {
        auto branchesJsonStr = step.params.count("branches") ? step.params.at("branches") : "[]";
        auto branchesVal = JsonValue::parse(branchesJsonStr);

        parallelState_ = std::make_unique<ParallelState>();
        parallelState_->inputContent = currentStepIndex_ > 0 && currentStepIndex_ - 1 < (int)historySteps_.size()
                                       ? historySteps_[currentStepIndex_ - 1].output
                                       : inputContent_;

        for (auto &b : branchesVal.array()) {
            ParallelState::Branch branch;
            branch.name = b.has("name") ? b["name"].string() : "branch";
            if (b.has("steps")) {
                for (auto &s : b["steps"].array()) {
                    PipelineStep ps;
                    ps.type = s.has("type") ? s["type"].string() : "ai";
                    ps.name = s.has("name") ? s["name"].string() : branch.name;
                    if (s.has("provider"))    ps.params["provider"]    = s["provider"].string();
                    if (s.has("model"))       ps.params["model"]       = s["model"].string();
                    if (s.has("userPrompt"))  ps.params["userPrompt"]  = s["userPrompt"].string();
                    if (s.has("systemPrompt"))ps.params["systemPrompt"]= s["systemPrompt"].string();
                    if (s.has("temperature")) ps.params["temperature"] = s["temperature"].string();
                    branch.steps.push_back(ps);
                }
            }
            parallelState_->branches.push_back(branch);
        }
        ExecuteNextParallelBranch();
        return;

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

void PipelineRunner::ExecuteNextParallelBranch() {
    if (!parallelState_ || cancelled_) {
        parallelState_.reset();
        RunNextStep();
        return;
    }

    int idx = parallelState_->currentBranch;
    if (idx >= (int)parallelState_->branches.size()) {
        // All branches done — store results and move on
        auto &hs = historySteps_[currentStepIndex_];
        hs.parallelBranches = parallelState_->results;
        hs.status = "completed";

        // Build output as JSON summary
        std::string out = "{";
        bool first = true;
        for (auto &kv : parallelState_->results) {
            if (!first) out += ",";
            first = false;
            out += "\"" + JsonEscape(kv.first) + "\":\"" + JsonEscape(kv.second) + "\"";
        }
        out += "}";
        hs.output = out;

        PostBridge("step_done", "{\"index\":" + std::to_string(currentStepIndex_) + "}");
        parallelState_.reset();
        RunNextStep();
        return;
    }

    auto &branch = parallelState_->branches[idx];
    std::string branchName = branch.name;

    // Notify JS which branch is running
    PostBridge("stream_chunk",
        "{\"stepIndex\":" + std::to_string(currentStepIndex_) +
        ",\"branch\":\"" + JsonEscape(branchName) + "\"" +
        ",\"text\":\"\"}");

    if (branch.steps.empty()) {
        parallelState_->results[branchName] = parallelState_->inputContent;
        parallelState_->currentBranch++;
        ExecuteNextParallelBranch();
        return;
    }

    auto &bStep = branch.steps[0]; // MVP: first step only per branch
    auto providerIt = providers_.find(bStep.params.count("provider") ? bStep.params.at("provider") : "openai");
    if (providerIt == providers_.end()) {
        parallelState_->results[branchName] = "[Provider not configured]";
        parallelState_->currentBranch++;
        ExecuteNextParallelBranch();
        return;
    }

    AIRequest req;
    req.model        = bStep.params.count("model")        ? bStep.params.at("model")        : "gpt-4.1";
    req.systemPrompt = bStep.params.count("systemPrompt") ? bStep.params.at("systemPrompt") : "";
    req.userPrompt   = bStep.params.count("userPrompt")   ? bStep.params.at("userPrompt")   : "{content}";

    auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };
    replaceAll(req.userPrompt, "{content}", parallelState_->inputContent);
    replaceAll(req.userPrompt, "{result}",  parallelState_->inputContent);

    providerIt->second->CallStreaming(req,
        [this, branchName](const std::string &chunk) {
            PostBridge("stream_chunk",
                "{\"stepIndex\":" + std::to_string(currentStepIndex_) +
                ",\"branch\":\"" + JsonEscape(branchName) + "\"" +
                ",\"text\":\"" + JsonEscape(chunk) + "\"}");
        },
        [this, branchName](const AIResponse &resp) {
            if (parallelState_) {
                parallelState_->results[branchName] = resp.content;
                parallelState_->currentBranch++;
            }
            ExecuteNextParallelBranch();
        },
        [this, branchName](const std::string &error) {
            if (parallelState_) {
                parallelState_->results[branchName] = "[Error: " + error + "]";
                parallelState_->currentBranch++;
            }
            ExecuteNextParallelBranch();
        });
}

std::string PipelineRunner::BuildMetaJson() {
    // Timestamp
    time_t now = time(nullptr);
    char timebuf[32];
    struct tm *tm_info = localtime(&now);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", tm_info);

    std::string lastOutput = historySteps_.empty() ? "" : historySteps_.back().output;

    std::string json = "{";
    json += "\"outputContent\":\"" + JsonEscape(lastOutput) + "\"";
    json += ",\"pipelineName\":\"" + JsonEscape(pipelineName_) + "\"";
    json += ",\"executedAt\":\"" + std::string(timebuf) + "\"";
    json += ",\"steps\":[";
    for (int i = 0; i < (int)executedStepParams_.size(); i++) {
        auto &step = executedStepParams_[i];
        if (i > 0) json += ",";
        json += "{";
        json += "\"name\":\"" + JsonEscape(step.name) + "\"";
        json += ",\"type\":\"" + JsonEscape(step.type) + "\"";
        for (auto &kv : step.params) {
            json += ",\"" + JsonEscape(kv.first) + "\":\"" + JsonEscape(kv.second) + "\"";
        }
        if (i < (int)historySteps_.size()) {
            json += ",\"output\":\"" + JsonEscape(historySteps_[i].output) + "\"";
            json += ",\"tokens\":" + std::to_string(historySteps_[i].completionTokens);
        }
        json += "}";
    }
    json += "]}";
    return json;
}

void PipelineRunner::ResumeManual(const std::string &content) {
    if (!waitingForManual_) return;
    waitingForManual_ = false;
    if (currentStepIndex_ < (int)historySteps_.size()) {
        historySteps_[currentStepIndex_].output = content;
        historySteps_[currentStepIndex_].status = "completed";
    }
    PostBridge("step_done", "{\"index\":" + std::to_string(currentStepIndex_) + "}");
    RunNextStep();
}

void PipelineRunner::CancelManual() {
    if (!waitingForManual_) return;
    waitingForManual_ = false;
    Cancel();
}

void PipelineRunner::AppendPipelineSteps(const std::string &pipelineName) {
    // Would need access to Storage to load pipeline by name
    (void)pipelineName;
}
