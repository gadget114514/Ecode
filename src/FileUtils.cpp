#include "Globals.inl"

void DebugLog(const std::string &msg, LogLevel level) {
  if (level < g_currentLogLevel)
    return;
  std::ofstream ofs("debug_init.log", std::ios::app);
  const char *levelStr[] = {"DEBUG", "INFO", "WARN", "ERROR"};
  ofs << "[" << levelStr[level] << "] " << msg << std::endl;
  if (g_logCallback)
    g_logCallback(msg, level);
}

bool SafeSave(const std::wstring &targetPath, const std::string &content) {
  std::wstring tempPath = targetPath + L".tmp";

  HANDLE hFile = CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    DebugLog("SafeSave - CreateFileW failed: " +
                 GetWin32ErrorString(GetLastError()),
             LOG_ERROR);
    return false;
  }

  DWORD bytesWritten;
  if (!WriteFile(hFile, content.c_str(), static_cast<DWORD>(content.length()),
                 &bytesWritten, NULL)) {
    DebugLog("SafeSave - WriteFile failed: " +
                 GetWin32ErrorString(GetLastError()),
             LOG_ERROR);
    CloseHandle(hFile);
    DeleteFileW(tempPath.c_str());
    return false;
  }
  CloseHandle(hFile);

  // Atomic-ish rename
  if (!ReplaceFileW(targetPath.c_str(), tempPath.c_str(), NULL,
                    REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
    DWORD err = GetLastError();
    // Fallback for new files
    if (err == ERROR_FILE_NOT_FOUND) {
      if (!MoveFileExW(tempPath.c_str(), targetPath.c_str(),
                       MOVEFILE_REPLACE_EXISTING)) {
        DebugLog("SafeSave - MoveFileExW failed: " +
                     GetWin32ErrorString(GetLastError()),
                 LOG_ERROR);
        DeleteFileW(tempPath.c_str());
        return false;
      }
    } else {
      DebugLog("SafeSave - ReplaceFileW failed: " + GetWin32ErrorString(err),
               LOG_ERROR);
      DeleteFileW(tempPath.c_str());
      return false;
    }
  }

  return true;
}

bool SafeSaveStreaming(
    const std::wstring &targetPath,
    const std::function<void(std::function<void(const char *, size_t)>)>
        &source) {
  std::wstring tempPath = targetPath + L".tmp";

  HANDLE hFile = CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    DebugLog("SafeSaveStreaming - CreateFileW failed: " +
                 GetWin32ErrorString(GetLastError()),
             LOG_ERROR);
    return false;
  }

  bool success = true;
  source([&](const char *data, size_t length) {
    if (!success)
      return;
    DWORD bytesWritten;
    if (!WriteFile(hFile, data, static_cast<DWORD>(length), &bytesWritten,
                   NULL)) {
      DebugLog("SafeSaveStreaming - WriteFile failed: " +
                   GetWin32ErrorString(GetLastError()),
               LOG_ERROR);
      success = false;
    }
  });

  CloseHandle(hFile);
  if (!success) {
    DeleteFileW(tempPath.c_str());
    return false;
  }

  // Atomic-ish rename
  if (!ReplaceFileW(targetPath.c_str(), tempPath.c_str(), NULL,
                    REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
    DWORD err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND) {
      if (!MoveFileExW(tempPath.c_str(), targetPath.c_str(),
                       MOVEFILE_REPLACE_EXISTING)) {
        DebugLog("SafeSaveStreaming - MoveFileExW failed: " +
                     GetWin32ErrorString(GetLastError()),
                 LOG_ERROR);
        DeleteFileW(tempPath.c_str());
        return false;
      }
    } else {
      DebugLog("SafeSaveStreaming - ReplaceFileW failed: " +
                   GetWin32ErrorString(err),
               LOG_ERROR);
      DeleteFileW(tempPath.c_str());
      return false;
    }
  }

  return true;
}
