#include "ParallelBackupStrategy.h"
#include "BackupUtils.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <sstream>
#include <thread>

struct WorkItem {
  fs::path source;
  fs::path target;
};

void ParallelBackupStrategy::CancelWorker(int index) {
  std::lock_guard<std::mutex> lock(m_cancelMutex);
  if (index >= 0 && index < (int)m_cancelFlags.size()) {
    if (m_cancelFlags[index]) {
      *m_cancelFlags[index] = 1; // Set cancel flag to TRUE (1)
    }
  }
}

void ParallelBackupStrategy::SafeLog(IBackupLogger *logger,
                                     const std::wstring &msg) {
  std::lock_guard<std::mutex> lock(m_cancelMutex); // Reuse mutex for simplicity
  if (logger) {
    logger->Log(msg);
  }
}

void ParallelBackupStrategy::Execute(const BackupTask &task,
                                     IBackupLogger *logger, bool dryRun) {
  if (logger) {
    std::wstringstream ss;
    ss << L"--------------------------------------------------\n"
       << (dryRun ? L"PARALLEL PREVIEW / SIMULATION MODE\n"
                  : L"PARALLEL BACKUP EXECUTION MODE\n")
       << L"Name: " << task.name << L" ("
       << (task.mode == BackupMode::Sync ? L"Sync" : L"Copy") << L")\n"
       << L"Source: " << task.sourcePath << L"\n"
       << L"Target: " << task.targetPath << L"\n"
       << L"--------------------------------------------------";
    SafeLog(logger, ss.str());
  }

  fs::path sourcePath(task.sourcePath);
  fs::path targetPath(task.targetPath);

  if (!fs::exists(sourcePath)) {
    SafeLog(logger, L"ERROR: Source path found.");
    return;
  }

  // Pre-scan for progress
  TaskProgress progress;
  if (logger) {
    SafeLog(logger, L"Scanning source... please wait.");
    BackupUtils::ScanSource(sourcePath, progress.totalFiles,
                            progress.totalBytes);
  }

  try {
    ProcessDirectory(sourcePath, targetPath, task, logger, dryRun, progress);
    if (task.mode == BackupMode::Sync &&
        (!task.IsAborted || !task.IsAborted())) {
      SyncDelete(sourcePath, targetPath, task, logger, dryRun);
    }
  } catch (const std::exception &e) {
    std::string what = e.what();
    SafeLog(logger,
            L"CRITICAL ERROR: " + std::wstring(what.begin(), what.end()));
  }

  if (logger) {
    SafeLog(logger, L"--------------------------------------------------");
    if (task.IsAborted && task.IsAborted()) {
      SafeLog(logger, L"PROCESS INTERRUPTED BY USER.");
    } else {
      SafeLog(logger, dryRun ? L"Parallel Preview finished."
                             : L"Parallel Backup finished.");
    }
    SafeLog(logger, L"--------------------------------------------------");
  }
}

void ParallelBackupStrategy::ProcessDirectory(
    const fs::path &source, const fs::path &target, const BackupTask &task,
    IBackupLogger *logger, bool dryRun, TaskProgress &aggregateProgress) {
  std::deque<WorkItem> queue;
  std::mutex queueMutex;
  std::condition_variable cv;
  std::atomic<int> activeWorkers(0);
  std::atomic<bool> globalAbort(false);
  std::atomic<bool> finished(false);

  // Progress tracking
  std::mutex progressMutex;

  {
    std::lock_guard<std::mutex> lock(queueMutex);
    queue.push_back({source, target});
  }

  // Limit to 4 threads as requested for UI matching
  const int numThreads = 4; // Fixed to 4 for UI

  // Initialize cancel flags
  {
    std::lock_guard<std::mutex> lock(m_cancelMutex);
    m_cancelFlags.clear();
    for (int i = 0; i < numThreads; ++i) {
      m_cancelFlags.push_back(std::make_shared<int>(0));
    }
  }

  auto worker = [&](int threadIndex) {
    activeWorkers++;
    while (true) {
      WorkItem item;
      {
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, [&] {
          return !queue.empty() || finished || globalAbort ||
                 (task.IsAborted && task.IsAborted());
        });

        if (globalAbort || (task.IsAborted && task.IsAborted()))
          break;
        if (queue.empty() && finished)
          break;

        if (queue.empty()) {
          activeWorkers--;
          if (activeWorkers == 0) {
            finished = true;
            if (logger)
              logger->OnWorkerProgress(threadIndex, L"Idle", 0); // Clear status
            cv.notify_all();
          } else {
            if (logger)
              logger->OnWorkerProgress(threadIndex, L"Waiting...", 0);
            cv.wait(lock, [&] {
              return !queue.empty() || finished || globalAbort ||
                     (task.IsAborted && task.IsAborted());
            });
            activeWorkers++;
            if (globalAbort || (task.IsAborted && task.IsAborted()) ||
                (queue.empty() && finished))
              break;
            if (!queue.empty()) {
              item = queue.front();
              queue.pop_front();
            } else
              continue;
          }
          if (finished)
            break;
        } else {
          item = queue.front();
          queue.pop_front();
        }
      }

      if (item.source.empty())
        continue;

      // Check per-worker cancel flag
      int *myCancelFlag = nullptr;
      {
        std::lock_guard<std::mutex> lk(m_cancelMutex);
        if (threadIndex < (int)m_cancelFlags.size()) {
          myCancelFlag = m_cancelFlags[threadIndex].get();
          *myCancelFlag = 0;
        }
      }

      long long lastTransferred = 0;
      auto progressCallback = [&](long long total, long long transferred) {
        if (total > 0 && logger) {
          int pct = (int)((transferred * 100) / total);
          logger->OnWorkerProgress(threadIndex,
                                   item.source.filename().wstring(), pct);
        }

        // Update aggregate progress bytes
        if (logger) {
          std::lock_guard<std::mutex> pLock(progressMutex);
          aggregateProgress.processedBytes += (transferred - lastTransferred);
          lastTransferred = transferred;
          aggregateProgress.currentFile = item.source.filename().wstring();
          logger->OnProgressDetailed(aggregateProgress);
        }
      };

      if (logger)
        logger->OnWorkerProgress(threadIndex, item.source.filename().wstring(),
                                 0);

      try {
        if (fs::is_symlink(item.source)) {
          if (BackupUtils::NeedsUpdate(item.source, item.target, task)) {
            SafeLog(logger, L"Link: " + item.source.wstring() + L" -> " +
                                item.target.wstring());
            if (!dryRun) {
              std::wstring err;
              if (!BackupUtils::RobustCopy(item.source, item.target, true, err,
                                           myCancelFlag, progressCallback)) {

                // If cancelled
                if (myCancelFlag && *myCancelFlag) {
                  SafeLog(logger, L"Worker " +
                                      std::to_wstring(threadIndex + 1) +
                                      L" Cancelled.");
                } else {
                  SafeLog(logger, L"  Link Error: " + err);
                  if (task.errorPolicy == ErrorPolicy::Suspend)
                    globalAbort = true;
                }
              } else {
                // Symbolic links don't usually invoke RobustCopy callbacks for
                // bytes, so we manually increment file count
                std::lock_guard<std::mutex> pLock(progressMutex);
                aggregateProgress.processedFiles++;
                logger->OnProgressDetailed(aggregateProgress);
              }
            }
          }
          continue;
        }

        if (fs::is_directory(item.source)) {
          if (!fs::exists(item.target) && !dryRun) {
            fs::create_directories(item.target);
          }
          {
            std::lock_guard<std::mutex> lock(queueMutex);
            for (const auto &entry : fs::directory_iterator(item.source)) {
              queue.push_back(
                  {entry.path(), item.target / entry.path().filename()});
            }
            cv.notify_all();
          }
        } else {
          bool updated =
              BackupUtils::NeedsUpdate(item.source, item.target, task);
          if (updated) {
            SafeLog(logger, L"Copy: " + item.source.wstring() + L" -> " +
                                item.target.wstring());
            if (!dryRun) {
              std::wstring err;
              bool success =
                  BackupUtils::RobustCopy(item.source, item.target, false, err,
                                          myCancelFlag, progressCallback);
              if (success) {
                {
                  std::lock_guard<std::mutex> pLock(progressMutex);
                  aggregateProgress.processedFiles++;
                  logger->OnProgressDetailed(aggregateProgress);
                }
                if (task.verify) {
                  if (BackupUtils::CompareFilesBinary(item.source,
                                                      item.target)) {
                    // verified
                  } else {
                    SafeLog(logger,
                            L"  VERIFICATION FAILED: " + item.target.wstring());
                    if (task.errorPolicy == ErrorPolicy::Suspend)
                      globalAbort = true;
                  }
                }
              } else {
                if (myCancelFlag && *myCancelFlag) {
                  SafeLog(logger, L"Worker " +
                                      std::to_wstring(threadIndex + 1) +
                                      L" Cancelled.");
                } else {
                  SafeLog(logger, L"  Copy Error: " + err);
                  if (task.errorPolicy == ErrorPolicy::Suspend)
                    globalAbort = true;
                }
              }
            } else {
              // Dry Run aggregate progress
              std::lock_guard<std::mutex> pLock(progressMutex);
              aggregateProgress.processedFiles++;
              try {
                aggregateProgress.processedBytes += fs::file_size(item.source);
              } catch (...) {
              }
              logger->OnProgressDetailed(aggregateProgress);
            }
          } else {
            // Skiped item (Identical) - still count towards aggregate
            std::lock_guard<std::mutex> pLock(progressMutex);
            aggregateProgress.processedFiles++;
            try {
              aggregateProgress.processedBytes += fs::file_size(item.source);
            } catch (...) {
            }
            logger->OnProgressDetailed(aggregateProgress);
          }
        }
      } catch (...) {
        if (task.errorPolicy == ErrorPolicy::Suspend)
          globalAbort = true;
      }
    }
    activeWorkers--;
    if (activeWorkers == 0) {
      finished = true;
      if (logger)
        logger->OnWorkerProgress(threadIndex, L"Done", 100);
      cv.notify_all();
    }
  };

  std::vector<std::thread> workers;
  for (int i = 0; i < numThreads; ++i)
    workers.push_back(std::thread(worker, i));
  for (auto &t : workers)
    t.join();

  if (globalAbort)
    throw std::runtime_error(
        "Parallel operation suspended due to error policy.");
}

void ParallelBackupStrategy::SyncDelete(const fs::path &source,
                                        const fs::path &target,
                                        const BackupTask &task,
                                        IBackupLogger *logger, bool dryRun) {
  if (task.IsAborted && task.IsAborted())
    return;
  if (!fs::exists(target))
    return;

  // SyncDelete is recursive and single-threaded in this implementation logic
  // (recursively called). To make it parallel or cancel-aware per worker, would
  // require refactoring into the queue. For now, we leave it as is but check
  // global abort. Ideally, SyncDelete should also use the queue.

  for (const auto &entry : fs::directory_iterator(target)) {
    if (task.IsAborted && task.IsAborted())
      break;
    auto item = entry.path();
    auto src = source / item.filename();
    if (!fs::exists(src)) {
      SafeLog(logger, L"Delete: " + item.wstring());
      if (!dryRun)
        fs::remove_all(item);
    } else if (fs::is_directory(item) && !fs::is_symlink(item)) {
      SyncDelete(src, item, task, logger, dryRun);
    }
  }
}
