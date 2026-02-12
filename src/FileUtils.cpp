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
