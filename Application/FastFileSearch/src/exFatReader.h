#pragma once
#include "MFTReader.h"
#include "exFatStructs.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>


class exFatReader {
public:
  exFatReader();
  ~exFatReader();

  bool Initialize(TCHAR driveLetter);
  void Close();
  bool Scan(void (*progressCallback)(int, int, void *), void *userData,
            std::function<void(const std::wstring &)> onFileFound = nullptr);

  void SetTraceCallback(std::function<void(const std::wstring &)> callback) {
    traceCallback = callback;
  }

  std::vector<FileResult> Search(const std::wstring &query,
                                 const std::wstring &targetFolder,
                                 const SearchOptions &options = SearchOptions(),
                                 int maxResults = -1);

  std::wstring GetLastErrorMessage() const;

private:
  std::wstring lastError;
  void SetError(const std::wstring &msg);

  struct Entry {
    uint32_t FirstCluster;
    uint32_t ParentFirstCluster;
    std::wstring Name;
    uint64_t Size;
    uint64_t LastWriteTime;
    bool IsDirectory;
    bool IsValid;
  };

  HANDLE hVolume;
  TCHAR currentDrive;
  std::unordered_map<uint32_t, Entry> fileMap;

  std::function<void(const std::wstring &)> traceCallback;

  uint32_t bytesPerSector;
  uint32_t sectorsPerCluster;
  uint32_t clusterHeapOffset;
  uint32_t fatOffset;
  uint32_t fatLength;
  uint32_t rootDirectoryCluster;

  void ProcessDirectory(uint32_t cluster, bool noFatChain, uint64_t dataLength,
                        uint32_t parentCluster, const std::wstring &parentPath);
  uint32_t GetNextCluster(uint32_t cluster);
  uint64_t ClusterToSector(uint32_t cluster);

  uint64_t FatTimestampToWin32(uint32_t timestamp, uint8_t tenMs);

  bool MatchPattern(const std::wstring &str, const std::wstring &pattern,
                    const SearchOptions &options);
};
