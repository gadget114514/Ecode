#include "ComparingBackupStrategy.h"
#include "BackupUtils.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
struct CompareWorkItem {
  fs::path source;
  fs::path target;
};
} // namespace

void ComparingBackupStrategy::CancelWorker(int index) {
  std::lock_guard<std::mutex> lock(m_cancelMutex);
  if (index >= 0 && index < (int)m_cancelFlags.size()) {
    if (m_cancelFlags[index]) {
      *m_cancelFlags[index] = 1;
    }
  }
}

void ComparingBackupStrategy::SafeLog(IBackupLogger *logger,
                                      const std::wstring &msg) {
  static std::mutex s_logMutex;
  std::lock_guard<std::mutex> lock(s_logMutex);
  if (logger) {
    logger->Log(msg);
  }
}

void ComparingBackupStrategy::Execute(const BackupTask &task,
                                      IBackupLogger *logger, bool dryRun) {
  if (logger) {
    std::wstringstream ss;
    ss << L"--------------------------------------------------\n"
       << L"COMPARING / CHECKING ENGINE START\n"
       << L"Name: " << task.name << L" (Verify Mode)\n"
       << L"Source: " << task.sourcePath << L"\n"
       << L"Target: " << task.targetPath << L"\n"
       << L"--------------------------------------------------";
    SafeLog(logger, ss.str());
  }

  fs::path sourcePath(task.sourcePath);
  fs::path targetPath(task.targetPath);

  if (!fs::exists(sourcePath)) {
    SafeLog(logger, L"ERROR: Source path does not exist.");
    return;
  }

  TaskProgress progress;
  if (logger) {
    SafeLog(logger, L"Scanning source... please wait.");
    BackupUtils::ScanSource(sourcePath, progress.totalFiles,
                            progress.totalBytes);
  }

  try {
    ProcessDirectory(sourcePath, targetPath, task, logger, dryRun, progress);
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
      SafeLog(logger, L"Comparison engine finished.");
    }
    SafeLog(logger, L"--------------------------------------------------");
  }
}

void ComparingBackupStrategy::ProcessDirectory(
    const fs::path &source, const fs::path &target, const BackupTask &task,
    IBackupLogger *logger, bool /*dryRun*/, TaskProgress &progress) {
  std::deque<CompareWorkItem> queue;
  std::mutex queueMutex;
  std::condition_variable cv;
  std::atomic<int> activeWorkers(0);
  std::atomic<bool> globalAbort(false);
  std::atomic<bool> finished(false);

  {
    std::lock_guard<std::mutex> lock(queueMutex);
    queue.push_back({source, target});
  }

  const int numThreads = 4;
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
      CompareWorkItem item;
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
              logger->OnWorkerProgress(threadIndex, L"Idle", 0);
            cv.notify_all();
          } else {
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

      int *myCancelFlag = m_cancelFlags[threadIndex].get();
      *myCancelFlag = 0;

      auto progressCallback = [&](long long total, long long transferred) {
        if (total > 0 && logger) {
          int pct = (int)((transferred * 100) / total);
          logger->OnWorkerProgress(threadIndex,
                                   item.source.filename().wstring(), pct);
        }
      };

      if (logger)
        logger->OnWorkerProgress(threadIndex, item.source.filename().wstring(),
                                 0);

      try {
        if (fs::is_directory(item.source)) {
          if (!fs::exists(item.target)) {
            SafeLog(logger, L"MISSING DIR: " + item.target.wstring());
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
          bool mismatch = false;
          if (!fs::exists(item.target)) {
            SafeLog(logger, L"MISSING FILE: " + item.target.wstring());
            mismatch = true;
          } else {
            if (BackupUtils::NeedsUpdate(item.source, item.target, task,
                                         myCancelFlag, progressCallback))
              mismatch = true;
          }

          if (mismatch && fs::exists(item.target)) {
            SafeLog(logger, L"MISMATCH: " + item.source.wstring());
          }

          if (logger) {
            static std::mutex s_progressMutex;
            std::lock_guard<std::mutex> lock(s_progressMutex);
            progress.processedFiles++;
            if (!fs::is_symlink(item.source)) {
              try {
                progress.processedBytes += fs::file_size(item.source);
              } catch (...) {
              }
            }
            progress.currentFile = item.source.filename().wstring();
            logger->OnProgressDetailed(progress);
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
      cv.notify_all();
    }
  };

  std::vector<std::thread> workers;
  for (int i = 0; i < numThreads; ++i)
    workers.push_back(std::thread(worker, i));
  for (auto &t : workers)
    t.join();

  if (globalAbort)
    throw std::runtime_error("Comparison suspended due to error policy.");
}
