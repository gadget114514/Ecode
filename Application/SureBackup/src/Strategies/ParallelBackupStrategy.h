#include "IBackupStrategy.h"
#include "Types.h"
#include <future>
#include <mutex>
#include <vector>

class ParallelBackupStrategy : public IBackupStrategy {
public:
  void Execute(const BackupTask &task, IBackupLogger *logger,
               bool dryRun) override;
  void CancelWorker(int index) override;

private:
  void ProcessDirectory(const fs::path &source, const fs::path &target,
                        const BackupTask &task, IBackupLogger *logger,
                        bool dryRun, TaskProgress &aggregateProgress);

  void SyncDelete(const fs::path &source, const fs::path &target,
                  const BackupTask &task, IBackupLogger *logger, bool dryRun);

  std::mutex m_loggerMutex;
  void SafeLog(IBackupLogger *logger, const std::wstring &msg);

  std::vector<std::shared_ptr<int>> m_cancelFlags;
  std::mutex m_cancelMutex;
};
