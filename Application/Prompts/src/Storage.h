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

    // Serialize a Node tree to JSON string (for Bridge transmission)
    static std::string SerializeNode(const Node &node);
    static std::string SerializePipelines(const std::vector<Pipeline> &pipelines);

    bool EnsureDirectory(const std::wstring &path);

private:
    std::wstring basePath_;
    std::wstring GetUserDataPath() const;
};
