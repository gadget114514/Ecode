#pragma once
#include <string>
#include <vector>
#include <functional>
#include "NodeData.h"

class Storage {
public:
    Storage();
    
    bool Init(const std::wstring &appDataPath);
    
    // Session
    SessionData LoadSession();
    void SaveSession(const SessionData &session);
    
    // Tab data
    Node LoadTabData(const std::wstring &filePath);
    void SaveTabData(const std::wstring &filePath, const Node &root);
    
    // Blobs
    std::string LoadBlob(const std::wstring &relativePath);
    std::wstring SaveBlob(const std::string &data, const std::wstring &ext);
    std::wstring SaveBlob(const unsigned char *data, size_t len, const std::wstring &ext);
    void RemoveBlob(const std::wstring &relativePath);
    void GarbageCollectBlobs(const std::vector<std::wstring> &referencedPaths);
    
    // Get all tab file paths from tabs directory
    std::vector<std::wstring> GetTabFiles();
    std::string GetFileTreeJson() const;
    
    // Path accessors
    std::wstring DataPath(const std::wstring &relativePath) const;
    std::wstring BlobPath(const std::wstring &relativePath) const;
    
    // History
    void SaveHistory(const std::string &recordJson);
    void UpdateHistoryEvaluation(const std::wstring &filename, const std::string &evaluation);
    std::vector<std::wstring> ListHistory();
    std::string LoadHistoryRecord(const std::wstring &filename);

    // Optimizer: rejected-edit buffer and version snapshots
    void SaveOptimizerData(const std::wstring &relativePath, const std::string &json);
    std::string LoadOptimizerData(const std::wstring &relativePath);
    
    // Providers
    std::map<std::string, ProviderConfig> LoadProviders();
    bool SaveProviders(const std::map<std::string, ProviderConfig> &providers);
    
    // Pipelines
    std::vector<Pipeline> LoadPipelines();
    void SavePipelines(const std::vector<Pipeline> &pipelines);
    
    std::wstring GetBasePath() const { return basePath_; }

    // Recent Files
    std::vector<std::wstring> LoadRecentFiles();
    void SaveRecentFiles(const std::vector<std::wstring> &files);

    // Checkpoints — step-level I/O persistence per run
    void SaveCheckpoint(const std::string &runId, int stepIndex,
                        const std::string &input, const std::string &output,
                        const std::string &metaJson);
    std::string LoadCheckpointInput(const std::string &runId, int stepIndex);
    std::string LoadCheckpointOutput(const std::string &runId, int stepIndex);
    std::string LoadCheckpointMeta(const std::string &runId, int stepIndex);

    // Run state persistence
    void SaveRunState(const std::string &runId, const std::string &stateJson);
    std::string LoadRunState(const std::string &runId);

    // Incomplete run detection
    struct IncompleteRun {
        std::string runId;
        std::string pipelineName;
        int lastCompletedStep = 0;
        int totalSteps = 0;
        std::string startedAt;
        std::string status;
    };
    std::vector<IncompleteRun> ScanIncompleteRuns();
    void CloseRun(const std::string &runId);
    void DiscardRun(const std::string &runId);

    // Named chests (shared cross-project buffer)
    void SaveToNamedChest(const std::string &chestName, const std::string &content);
    std::string LoadFromNamedChest(const std::string &chestName);
    bool ChestExists(const std::string &chestName) const;
    std::vector<std::string> ListNamedChests() const;

    // Storage chest (trash/overflow for GC'd data)
    void MoveToStorageChest(const std::wstring &sourcePath, const std::string &storedName);
    std::vector<std::wstring> ListStorageChest() const;
    std::string LoadFromStorageChest(const std::wstring &filename);

    // History retention config (default 50, min 10, max 500)
    void SetMaxHistoryRuns(int maxRuns);
    int GetMaxHistoryRuns() const { return maxHistoryRuns_; }
    
    // General config persistence
    struct GeneralConfig {
        int historyRetention = 50;
        std::string defaultProvider = "openai";
        std::string defaultModel;
    };
    GeneralConfig LoadGeneralConfig();
    bool SaveGeneralConfig(const GeneralConfig &cfg);
    
    // Recipes
    struct Recipe {
        std::string type = "ai";     // "ai" or "command"
        std::string name;
        // AI fields
        std::string provider;
        std::string model;
        double temperature = 0.7;
        std::string systemPrompt;
        // Command field
        std::string command;
    };
    std::vector<Recipe> LoadRecipes();
    bool SaveRecipes(const std::vector<Recipe> &recipes);

    // Project isolation — resolve a file path within the current project, rejecting traversal
    std::wstring ResolveProjectPath(const std::wstring &relativePath) const;

    // Wizards — load wizard JSON definition by name
    std::string LoadWizardData(const std::string &wizardName);

    // Serialize a Node tree to JSON string (for Bridge transmission)
    static std::string SerializeNode(const Node &node);
    static std::string SerializePipelines(const std::vector<Pipeline> &pipelines);

    bool EnsureDirectory(const std::wstring &path);

private:
    std::wstring basePath_;
    std::wstring globalPath_;
    int maxHistoryRuns_ = 50;
    std::wstring GetUserDataPath() const;
};
