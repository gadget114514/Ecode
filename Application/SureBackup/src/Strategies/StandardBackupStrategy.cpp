#include "StandardBackupStrategy.h"
#include "BackupUtils.h"
#include <sstream>
#include <windows.h>

void StandardBackupStrategy::Execute(const BackupTask &task,
                                     IBackupLogger *logger, bool dryRun) {
  if (logger) {
    std::wstring modeStr = L"Copy";
    if (task.mode == BackupMode::Sync)
      modeStr = L"Sync";
    else if (task.mode == BackupMode::Verify)
      modeStr = L"Verify";

    std::wstringstream ss;
    ss << L"--------------------------------------------------\n"
       << (dryRun ? L"PREVIEW / SIMULATION MODE\n" : L"BACKUP EXECUTION MODE\n")
       << L"Name: " << task.name << L" (" << modeStr << L")\n"
       << L"Source: " << task.sourcePath << L"\n"
       << L"Target: " << task.targetPath << L"\n"
       << L"--------------------------------------------------";
    logger->Log(ss.str());
  }

  fs::path sourcePath(task.sourcePath);
  fs::path targetPath(task.targetPath);

  if (!fs::exists(sourcePath)) {
    if (logger)
      logger->Log(L"ERROR: Source path found.");
    return;
  }

  try {
    TaskProgress progress;
    if (logger) {
      logger->Log(L"Scanning source... please wait.");
      BackupUtils::ScanSource(sourcePath, progress.totalFiles,
                              progress.totalBytes);
    }

    ProcessDirectory(sourcePath, targetPath, task, logger, dryRun, progress);
    if (task.mode == BackupMode::Sync &&
        (!task.IsAborted || !task.IsAborted())) {
      SyncDelete(sourcePath, targetPath, task, logger, dryRun);
    }
  } catch (const std::exception &e) {
    if (logger) {
      std::string what = e.what();
      logger->Log(L"CRITICAL ERROR: " + std::wstring(what.begin(), what.end()));
    }
  }

  if (logger) {
    logger->Log(L"--------------------------------------------------");
    if (task.IsAborted && task.IsAborted()) {
      logger->Log(L"PROCESS INTERRUPTED BY USER.");
    } else {
      logger->Log(dryRun ? L"Preview simulation finished."
                         : L"Backup execution finished.");
    }
    logger->Log(L"--------------------------------------------------");
  }
}

void StandardBackupStrategy::ProcessDirectory(
    const fs::path &source, const fs::path &target, const BackupTask &task,
    IBackupLogger *logger, bool dryRun, TaskProgress &progress) {
  if (task.IsAborted && task.IsAborted())
    return;

  if (logger)
    logger->OnProgress(source.wstring());

  bool isSymlink = fs::is_symlink(source);

  if (isSymlink) {
    if (BackupUtils::NeedsUpdate(source, target, task)) {
      if (logger) {
        logger->OnFileAction(L"Link", (dryRun ? L"[PREVIEW] " : L"") +
                                          source.wstring() + L" -> " +
                                          target.wstring());
      }
      if (!dryRun) {
        std::wstring err;
        if (!BackupUtils::RobustCopy(source, target, true, err)) {
          if (logger)
            logger->Log(L"  Link Error: " + err);
          if (task.errorPolicy == ErrorPolicy::Suspend)
            throw std::runtime_error("Critical file error (Suspend policy)");
        }
      }
    }
    return;
  }

  if (fs::is_directory(source)) {
    if (task.mode != BackupMode::Verify) {
      if (!fs::exists(target) && !dryRun) {
        try {
          fs::create_directories(target);
        } catch (...) {
          if (logger)
            logger->Log(L"  Dir Create Error: " + target.wstring());
          if (task.errorPolicy == ErrorPolicy::Suspend)
            throw;
          return;
        }
      }
    } else {
      // In Verify mode, if directory doesn't exist on target, report it
      if (!fs::exists(target)) {
        if (logger)
          logger->Log(L"Verify Fail (Missing Dir): " + target.wstring());
      }
    }

    for (const auto &entry : fs::directory_iterator(source)) {
      if (task.IsAborted && task.IsAborted())
        break;
      ProcessDirectory(entry.path(), target / entry.path().filename(), task,
                       logger, dryRun, progress);
    }
    if (task.mode != BackupMode::Verify && !dryRun && fs::exists(target)) {
      BackupUtils::SetFileTimestamps(source, target);
    }
  } else {
    // Regular File
    // Standard Copy Logic
    if (task.mode == BackupMode::Verify) {
      progress.currentFile = source.filename().wstring();
      if (logger)
        logger->OnProgressDetailed(progress);

      bool match = true;
      // Verify assumes we want to check it.
      // If file doesn't exist on target -> Error/Missing
      if (!fs::exists(target)) {
        match = false;
        if (logger)
          logger->Log(L"Verify Fail (Missing): " + source.wstring());
      } else {
        // For Verify Mode, we usually want STRICT comparison, or use the
        // configured flags. If user selected Verify, and kept defaults
        // (Size+Time), we might use that. But traditionally Verify matches
        // content. Let's use the task criteria.
        if (task.criteriaData) {
          if (!BackupUtils::CompareFilesBinary(source, target))
            match = false;
        } else {
          if (BackupUtils::NeedsUpdate(source, target, task))
            match = false;
        }
      }

      if (match) {
        // Good
      } else {
        if (logger && fs::exists(target))
          logger->Log(L"Verify Fail (Mismatch): " + source.wstring());
        if (task.errorPolicy == ErrorPolicy::Suspend) {
          // handle error
        }
      }

      // Always increment progress in Verify Mode
      progress.processedFiles++;
      try {
        progress.processedBytes += fs::file_size(source);
      } catch (...) {
      }
      if (logger)
        logger->OnProgressDetailed(progress);

    } else if (BackupUtils::NeedsUpdate(source, target, task)) {

      progress.currentFile = source.filename().wstring();
      if (logger)
        logger->OnProgressDetailed(progress);

      if (logger) {
        logger->OnFileAction(L"Copy", (dryRun ? L"[PREVIEW] " : L"") +
                                          source.wstring() + L" -> " +
                                          target.wstring());
      }
      if (!dryRun) {
        std::wstring err;
        long long startBytes = progress.processedBytes;

        auto progressCb = [&](long long /*total*/, long long transferred) {
          progress.processedBytes = startBytes + transferred;
          if (logger)
            logger->OnProgressDetailed(progress);
        };

        if (BackupUtils::RobustCopy(source, target, false, err, nullptr,
                                    progressCb)) {

          // Finalize progress for this file
          try {
            // Use actual file size to match ScanSource
            progress.processedBytes = startBytes + fs::file_size(source);
          } catch (...) {
          }
          progress.processedFiles++;
          if (logger)
            logger->OnProgressDetailed(progress);

          if (task.verify) {
            if (BackupUtils::CompareFilesBinary(source, target)) {
              // OK
            } else {
              if (logger)
                logger->Log(L"  VERIFICATION FAILED: " + target.wstring());
              if (task.errorPolicy == ErrorPolicy::Suspend)
                throw std::runtime_error("Verification mismatch");
            }
          }
        } else {
          if (logger)
            logger->Log(L"  Copy Error: " + err);
          if (task.errorPolicy == ErrorPolicy::Suspend)
            throw std::runtime_error("Copy failed");
        }
      } else {
        // Dry run progress
        progress.processedFiles++;
        try {
          progress.processedBytes += fs::file_size(source);
        } catch (...) {
        }
        if (logger)
          logger->OnProgressDetailed(progress);
      }
    } else {
      // If files are identical, just update progress metrics (skipped file)
      progress.processedFiles++;
      try {
        progress.processedBytes += fs::file_size(source);
      } catch (...) {
      }
      if (logger)
        logger->OnProgressDetailed(progress);
    }
  }
}

void StandardBackupStrategy::SyncDelete(const fs::path &source,
                                        const fs::path &target,
                                        const BackupTask &task,
                                        IBackupLogger *logger, bool dryRun) {
  if (task.IsAborted && task.IsAborted())
    return;

  if (!fs::exists(target))
    return;

  for (const auto &entry : fs::directory_iterator(target)) {
    if (task.IsAborted && task.IsAborted())
      break;
    const auto &targetItem = entry.path();
    auto sourceFullPath = source / targetItem.filename();

    if (!fs::exists(sourceFullPath)) {
      if (logger) {
        logger->OnFileAction(L"Delete", (dryRun ? L"[PREVIEW] " : L"") +
                                            targetItem.wstring());
      }
      if (!dryRun) {
        try {
          fs::remove_all(targetItem);
        } catch (...) {
          if (logger)
            logger->Log(L"  Delete Failed: " + targetItem.wstring());
          if (task.errorPolicy == ErrorPolicy::Suspend)
            throw std::runtime_error("Delete failed");
        }
      }
    } else if (fs::is_directory(targetItem) && !fs::is_symlink(targetItem)) {
      SyncDelete(sourceFullPath, targetItem, task, logger, dryRun);
    }
  }
}
