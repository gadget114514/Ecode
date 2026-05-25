#pragma once

#include "Types.h"

class IBackupStrategy {
public:
  virtual ~IBackupStrategy() = default;

  // Core execution method
  virtual void Execute(const BackupTask &task, IBackupLogger *logger,
                       bool dryRun) = 0;
  virtual void CancelWorker(int /*index*/) {}
};
