#pragma once
#include <string>
#include <vector>
#include <optional>
#include "NodeData.h"

// Forward declaration
struct OptEditProposal;

struct PipelineVersion {
    int version = 0;
    std::string timestamp;
    std::string sessionId;
    std::string label;
    std::vector<OptEditProposal> approvedProposals;
};

struct VersionCursor {
    std::string pipelineName;
    int currentVersion = 0;  // active version number
    int headVersion = 0;     // Redo upper bound
    std::vector<PipelineVersion> entries;
};

class Storage;

class PipelineVersionManager {
public:
    explicit PipelineVersionManager(Storage &storage);

    // Create v1 snapshot if none exists yet
    void EnsureBaseVersion(const std::string &pipelineName, const Pipeline &pipeline);

    // Save a new version after optimization; returns new version number.
    // If currentVersion < headVersion, entries after currentVersion are dropped (git-like).
    int CommitVersion(const std::string &pipelineName,
                      const Pipeline &pipeline,
                      const std::string &sessionId,
                      const std::string &label,
                      const std::vector<OptEditProposal> &approvedProposals);

    // Move back one version; returns the restored Pipeline, or nullopt if already at v1.
    std::optional<Pipeline> Undo(const std::string &pipelineName);

    // Move forward one version (up to headVersion); returns restored Pipeline or nullopt.
    std::optional<Pipeline> Redo(const std::string &pipelineName);

    // Jump to an arbitrary version; returns its Pipeline or nullopt if not found.
    std::optional<Pipeline> CheckoutVersion(const std::string &pipelineName, int version);

    // Apply a previous version's approvedProposals to `current`, then CommitVersion.
    Pipeline ReapplyVersion(const std::string &pipelineName,
                            int version,
                            const Pipeline &current);

    // Get the current cursor state (for UI display).
    VersionCursor GetCursor(const std::string &pipelineName);

private:
    Storage &storage_;

    void SaveCursor(const VersionCursor &cursor);
    VersionCursor ReadCursor(const std::string &pipelineName);
    void SaveSnapshot(const std::string &pipelineName, int version, const Pipeline &pipeline);
    Pipeline LoadSnapshot(const std::string &pipelineName, int version);

    static std::string SanitizeName(const std::string &name);
    static std::string SerializeCursor(const VersionCursor &cursor);
    static VersionCursor DeserializeCursor(const std::string &json, const std::string &pipelineName);
    static std::string SerializeSnapshot(const Pipeline &pipeline);
    static Pipeline DeserializeSnapshot(const std::string &json);
};
