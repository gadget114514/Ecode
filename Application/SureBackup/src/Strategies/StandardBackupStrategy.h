#pragma once

#include "IBackupStrategy.h"

class StandardBackupStrategy : public IBackupStrategy {
public:
  void Execute(const BackupTask &task, IBackupLogger *logger,
               bool dryRun) override;

private:
  void ProcessDirectory(const fs::path &source, const fs::path &target,
                        const BackupTask &task, IBackupLogger *logger,
                        bool dryRun, TaskProgress &progress);
  void SyncDelete(const fs::path &source, const fs::path &target,
                  const BackupTask &task, IBackupLogger *logger, bool dryRun);
  bool NeedsUpdate(const fs::path &sourceFile, const fs::path &targetFile);
};
