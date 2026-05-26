#pragma once

#ifndef _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#endif

#include <functional>
#include <string>

// Robust filesystem detection using version check
#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || (__cplusplus >= 201703L)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

enum class BackupMode { Copy, Sync, Verify };
enum class ErrorPolicy { Continue, Suspend };

struct BackupTask {
  std::wstring name;
  std::wstring sourcePath;
  std::wstring targetPath;
  BackupMode mode;
  bool verify;
  ErrorPolicy errorPolicy;
  std::function<bool()>
      IsAborted; // Callback to check if user stopped the process
  bool parallelMode = false;

  // New: Criteria for file comparison
  bool criteriaSize = true;  // Use file size?
  bool criteriaTime = true;  // Use timestamp?
  bool criteriaData = false; // Use full binary difference? (Expensive)

  // NOTE: If mode is Verify, we typically want full data check, but we'll
  // respect flags or default to Data for Verify mode if user wants. Actually,
  // for "Verify Only" strategy, we typically want to check integrity.
};

struct TaskProgress {
  long long totalFiles = 0;
  long long processedFiles = 0;
  long long totalBytes = 0;
  long long processedBytes = 0;
  std::wstring currentFile;
};

class IBackupLogger {
public:
  virtual ~IBackupLogger() = default;
  virtual void Log(const std::wstring &message) = 0;
  virtual void OnFileAction(const std::wstring &action,
                            const std::wstring &path) = 0;
  virtual void OnProgress(const std::wstring &path) = 0;
  virtual void OnProgressDetailed(const TaskProgress &progress) = 0;
  virtual void OnWorkerProgress(int workerId, const std::wstring &file,
                                int percent) = 0;
};
