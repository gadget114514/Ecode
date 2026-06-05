#pragma once
#include <string>
#include <vector>
#include <map>

struct Attachment {
    std::string id;
    std::string mimetype;
    bool inlineData = false;
    std::string content;  // base64 if inline, else empty
    std::string file;     // relative path if external
    size_t size = 0;
};

struct Node {
    std::string title;      // base64
    std::string content;    // base64
    std::string mimetype;   // "text/plain" | "text/html" | "application/rtf" | "image/png" | "image/jpeg" | "image/webp"
    std::vector<Attachment> attachments;
    std::vector<Node> children;
    std::string pipelineMeta; // JSON: how this node was created (empty if not pipeline-generated)

    // Evaluation (SkillOpt-style feedback signals)
    std::string evaluation;      // "" | "ok" | "rejected" | "pinned"
    std::string evaluatedAt;     // ISO 8601 timestamp
    std::string evaluationNote;  // optional user comment
};

struct TabData {
    std::string name;
    std::string file;       // relative path in data/
    Node root;
};

struct SessionData {
    std::vector<TabData> tabs;
};

struct ProviderConfig {
    std::string apiKey;
    std::string baseUrl;
    std::vector<std::string> models;
};

struct PipelineStep {
    std::string name;
    std::string type;       // "ai" | "manual" | "command" | "tool" | "fetch" | "condition" | "history"
    std::map<std::string, std::string> params;
};

struct Pipeline {
    std::string name;
    std::string mode = "basic";
    int retryCount = 3;
    int retryDelayMs = 2000;
    std::string outputMode = "child";
    std::string outputNaming = "{pipeline_name}_{timestamp}";
    std::string multiMedia = "attachments";
    std::vector<PipelineStep> steps;
};
