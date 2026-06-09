#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>

struct Attachment;

struct AIRequest {
    std::string model;
    std::string systemPrompt;
    std::string userPrompt;
    double temperature = 0.7;
    int maxTokens = 4096;
    std::vector<Attachment> attachments;
    std::map<std::string, std::string> extraParams;
};

struct AIResponse {
    std::string content;
    std::string model;
    std::vector<Attachment> mediaOutputs;
    int promptTokens = 0;
    int completionTokens = 0;
};

class AIProvider {
public:
    virtual ~AIProvider() = default;
    
    virtual std::string Name() const = 0;
    virtual std::vector<std::string> ListModels() = 0;
    virtual AIResponse Call(const AIRequest &req) = 0;
    virtual void CallStreaming(const AIRequest &req,
                                std::function<void(const std::string &chunk)> onChunk,
                                std::function<void(const AIResponse &)> onDone,
                                std::function<void(const std::string &error)> onError) = 0;
    
    // Connection test: returns empty string on success, error message on failure
    virtual std::string TestConnection() = 0;
    
    // Factory
    static AIProvider *Create(const std::string &providerType,
                               const std::string &apiKey,
                               const std::string &baseUrl = "");
};

// Set global HTTP log callback (for debugging HTTP request/response in JS log)
void SetHttpLogCallback(std::function<void(const std::string&)> cb);
