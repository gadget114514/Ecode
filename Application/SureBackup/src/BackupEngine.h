#pragma once

#include "Strategies/IBackupStrategy.h"
#include "Strategies/Types.h"
#include <atomic>
#include <memory>

// The Engine now acts as a Context for the Strategy Pattern
class BackupEngine {
public:
  BackupEngine(IBackupLogger *logger);
  ~BackupEngine();

  // Allows runtime switching of logic
  void SetStrategy(std::unique_ptr<IBackupStrategy> strategy);

  void Run(const BackupTask &task, bool dryRun);

  void Abort() { m_aborted = true; }
  void CancelWorker(int index) {
    if (m_strategy)
      m_strategy->CancelWorker(index);
  }
  bool IsAborted() const { return m_aborted; }

private:
  IBackupLogger *m_logger;
  std::unique_ptr<IBackupStrategy> m_strategy;
  std::atomic<bool> m_aborted{false};
};
