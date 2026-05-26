#pragma once

#include "IBackupStrategy.h"
#include <memory>
#include <mutex>
#include <vector>

class ComparingBackupStrategy : public IBackupStrategy {
public:
  void Execute(const BackupTask &task, IBackupLogger *logger,
               bool dryRun) override;
  void CancelWorker(int index) override;

private:
  void ProcessDirectory(const fs::path &source, const fs::path &target,
                        const BackupTask &task, IBackupLogger *logger,
                        bool dryRun, TaskProgress &progress);

  std::vector<std::shared_ptr<int>> m_cancelFlags;
  std::mutex m_cancelMutex;

  void SafeLog(IBackupLogger *logger, const std::wstring &msg);
};
