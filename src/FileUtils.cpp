#include <functional>
#include <string>
#include <windows.h>

bool SafeSave(const std::wstring &targetPath, const std::string &content) {
  std::wstring tempPath = targetPath + L".tmp";

  HANDLE hFile = CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
    return false;

  DWORD bytesWritten;
  if (!WriteFile(hFile, content.c_str(), static_cast<DWORD>(content.length()),
                 &bytesWritten, NULL)) {
    CloseHandle(hFile);
    DeleteFileW(tempPath.c_str());
    return false;
  }
  CloseHandle(hFile);

  // Atomic-ish rename
  if (!ReplaceFileW(targetPath.c_str(), tempPath.c_str(), NULL,
                    REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
    // Fallback for new files
    if (GetLastError() == ERROR_FILE_NOT_FOUND) {
      if (!MoveFileExW(tempPath.c_str(), targetPath.c_str(),
                       MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tempPath.c_str());
        return false;
      }
    } else {
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
  if (hFile == INVALID_HANDLE_VALUE)
    return false;

  bool success = true;
  source([&](const char *data, size_t length) {
    if (!success)
      return;
    DWORD bytesWritten;
    if (!WriteFile(hFile, data, static_cast<DWORD>(length), &bytesWritten,
                   NULL)) {
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
    if (GetLastError() == ERROR_FILE_NOT_FOUND) {
      if (!MoveFileExW(tempPath.c_str(), targetPath.c_str(),
                       MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tempPath.c_str());
        return false;
      }
    } else {
      DeleteFileW(tempPath.c_str());
      return false;
    }
  }

  return true;
}
