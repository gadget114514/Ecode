#pragma once
#include "Types.h" // Needed for BackupTask and fs namespace
#include <functional>
#include <string>

namespace BackupUtils {
typedef std::function<void(long long total, long long transferred)>
    CopyProgressCallback;

bool CompareFilesBinary(const fs::path &p1, const fs::path &p2,
                        int *cancelFlag = nullptr,
                        CopyProgressCallback progressCallback = nullptr);

void SetFileTimestamps(const fs::path &src, const fs::path &dst);

bool RobustCopy(const fs::path &src, const fs::path &dst, bool isSymlink,
                std::wstring &errorMsg, int *cancelFlag = nullptr,
                CopyProgressCallback progressCallback = nullptr);

bool NeedsUpdate(const fs::path &source, const fs::path &target,
                 const BackupTask &task, int *cancelFlag = nullptr,
                 CopyProgressCallback progressCallback = nullptr);
void ScanSource(const fs::path &source, long long &totalFiles,
                long long &totalBytes);
} // namespace BackupUtils
