#include "BackupEngine.h"
#include "Strategies/ComparingBackupStrategy.h"
#include "Strategies/ParallelBackupStrategy.h"
#include "Strategies/StandardBackupStrategy.h"

BackupEngine::BackupEngine(IBackupLogger *logger) : m_logger(logger) {
  // Default to Standard Strategy
  m_strategy = std::make_unique<StandardBackupStrategy>();
}

BackupEngine::~BackupEngine() {}

void BackupEngine::SetStrategy(std::unique_ptr<IBackupStrategy> strategy) {
  m_strategy = std::move(strategy);
}

void BackupEngine::Run(const BackupTask &task, bool dryRun) {
  if (m_strategy) {
    m_aborted = false;
    BackupTask activeTask = task;
    activeTask.IsAborted = [this]() { return m_aborted.load(); };
    m_strategy->Execute(activeTask, m_logger, dryRun);
  } else {
    if (m_logger) {
      m_logger->Log(L"Error: No backup strategy selected.");
    }
  }
}
