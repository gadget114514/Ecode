#include "BackupUtils.h"
#include <fstream>
#include <vector>
#include <windows.h>

namespace BackupUtils {

bool CompareFilesBinary(const fs::path &p1, const fs::path &p2, int *cancelFlag,
                        CopyProgressCallback progressCallback) {
  try {
    long long size = fs::file_size(p1);
    if (size != fs::file_size(p2))
      return false;
    std::ifstream f1(p1, std::ios::binary);
    std::ifstream f2(p2, std::ios::binary);
    if (!f1 || !f2)
      return false;

    const size_t bufSize = 1024 * 16;
    std::vector<char> b1(bufSize);
    std::vector<char> b2(bufSize);

    long long processed = 0;
    while (f1.read(b1.data(), bufSize) || f1.gcount() > 0) {
      if (cancelFlag && *cancelFlag)
        return false;

      f2.read(b2.data(), bufSize);
      if (f1.gcount() != f2.gcount())
        return false;
      if (std::memcmp(b1.data(), b2.data(), (size_t)f1.gcount()) != 0)
        return false;

      processed += f1.gcount();
      if (progressCallback)
        progressCallback(size, processed);

      if (f1.eof() || f2.eof())
        break;
    }
    return true;
  } catch (...) {
    return false;
  }
}

DWORD CALLBACK CopyProgressRoutine(LARGE_INTEGER TotalFileSize,
                                   LARGE_INTEGER TotalBytesTransferred,
                                   LARGE_INTEGER StreamSize,
                                   LARGE_INTEGER StreamBytesTransferred,
                                   DWORD dwStreamNumber, DWORD dwCallbackReason,
                                   HANDLE hSourceFile, HANDLE hDestinationFile,
                                   LPVOID lpData) {
  auto cb = static_cast<CopyProgressCallback *>(lpData);
  if (cb && *cb) {
    (*cb)(TotalFileSize.QuadPart, TotalBytesTransferred.QuadPart);
  }
  return PROGRESS_CONTINUE;
}

void SetFileTimestamps(const fs::path &src, const fs::path &dst) {
  HANDLE hSrc = CreateFileW(src.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hSrc == INVALID_HANDLE_VALUE)
    return;
  FILETIME creation, access, write;
  if (GetFileTime(hSrc, &creation, &access, &write)) {
    HANDLE hDst = CreateFileW(dst.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDst != INVALID_HANDLE_VALUE) {
      SetFileTime(hDst, &creation, &access, &write);
      CloseHandle(hDst);
    }
  }
  CloseHandle(hSrc);
}

bool RobustCopy(const fs::path &src, const fs::path &dst, bool isSymlink,
                std::wstring &errorMsg, int *cancelFlag,
                CopyProgressCallback progressCallback) {
  if (isSymlink) {
    if (cancelFlag && *cancelFlag)
      return false;

    if (CopyFileExW(src.c_str(), dst.c_str(), NULL, NULL, (BOOL *)cancelFlag,
                    COPY_FILE_COPY_SYMLINK | COPY_FILE_FAIL_IF_EXISTS)) {
      if (progressCallback)
        progressCallback(0, 0);
      SetFileTimestamps(src, dst);
      return true;
    }
    if (GetLastError() == ERROR_FILE_EXISTS ||
        GetLastError() == ERROR_ALREADY_EXISTS) {
      fs::remove(dst);
      return CopyFileExW(src.c_str(), dst.c_str(), NULL, NULL,
                         (BOOL *)cancelFlag, COPY_FILE_COPY_SYMLINK);
    }
  } else {
    if (CopyFileExW(src.c_str(), dst.c_str(), CopyProgressRoutine,
                    (LPVOID)&progressCallback, (BOOL *)cancelFlag, 0)) {
      SetFileTimestamps(src, dst);
      return true;
    }
  }

  DWORD err = GetLastError();
  if (cancelFlag && *cancelFlag) {
    errorMsg = L"Operation cancelled";
    return false;
  }

  wchar_t buf[256];
  FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                 (sizeof(buf) / sizeof(wchar_t)), NULL);
  errorMsg = buf;
  return false;
}

bool NeedsUpdate(const fs::path &source, const fs::path &target,
                 const BackupTask &task, int *cancelFlag,
                 CopyProgressCallback progressCallback) {
  if (!fs::exists(target))
    return true;
  if (fs::is_symlink(source))
    return true;
  if (fs::is_directory(source))
    return false;
  try {
    if (task.criteriaSize) {
      if (fs::file_size(source) != fs::file_size(target))
        return true;
    }
    if (task.criteriaTime) {
      auto sTime = fs::last_write_time(source);
      auto tTime = fs::last_write_time(target);
      if (sTime > tTime)
        return true;
    }
    if (task.criteriaData) {
      if (!CompareFilesBinary(source, target, cancelFlag, progressCallback))
        return true;
    }
    return false;
  } catch (...) {
    return true;
  }
}

void ScanSource(const fs::path &source, long long &totalFiles,
                long long &totalBytes) {
  totalFiles = 0;
  totalBytes = 0;
  if (!fs::exists(source))
    return;
  if (!fs::is_directory(source)) {
    totalFiles = 1;
    totalBytes = fs::file_size(source);
    return;
  }

  try {
#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || (__cplusplus >= 201703L)
    auto options = fs::directory_options::skip_permission_denied;
#else
    auto options = fs::directory_options::none;
#endif
    for (const auto &entry :
         fs::recursive_directory_iterator(source, options)) {
      if (!fs::is_directory(entry)) {
        totalFiles++;
        try {
          if (!fs::is_symlink(entry))
            totalBytes += fs::file_size(entry);
        } catch (...) {
        }
      }
    }
  } catch (...) {
  }
}

} // namespace BackupUtils
