#pragma once
#include "FatStructs.h"
#include "MFTReader.h" // For FileResult and other shared structures
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

class FatReader {
public:
  FatReader();
  ~FatReader();

  bool Initialize(TCHAR driveLetter);
  void Close();
  bool Scan(int codePage, void (*progressCallback)(int, int, void *),
            void *userData,
            std::function<void(const std::wstring &)> onFileFound = nullptr);

  void SetTraceCallback(std::function<void(const std::wstring &)> callback) {
    traceCallback = callback;
  }

  std::vector<FileResult> Search(const std::wstring &query,
                                 const std::wstring &targetFolder,
                                 int codePage = CP_OEMCP,
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
  std::unordered_map<uint32_t, Entry>
      fileMap; // Key: FirstCluster (unique for non-empty files/dirs)
  // For FAT, since multiple files can have FirstCluster=0 (empty),
  // we might need a better ID or store by full path.
  // Actually, FAT directory structure is tree-based.

  std::function<void(const std::wstring &)> traceCallback;

  uint32_t bytesPerSector;
  uint32_t sectorsPerCluster;
  uint32_t reservedSectors;
  uint32_t fatCount;
  uint32_t sectorsPerFat;
  uint32_t rootDirEntries; // Only for FAT12/16
  uint32_t rootDirSectors; // Only for FAT12/16
  uint32_t firstDataSector;
  uint32_t totalSectors;

  bool isFat32;
  uint32_t rootCluster; // For FAT32

  void ProcessDirectory(uint32_t cluster, uint32_t parentCluster,
                        const std::wstring &parentPath, int codePage);
  uint32_t GetNextCluster(uint32_t cluster);
  uint64_t ClusterToSector(uint32_t cluster);

  std::wstring BuildPath(const Entry &entry);
  bool MatchPattern(const std::wstring &str, const std::wstring &pattern,
                    const SearchOptions &options);

  std::vector<uint8_t> fatCache;
  void LoadFat();

  // Progress State
  uint64_t totalUsedClusters;
  uint64_t processedClusters;
  void (*progressCb)(int, int, void *);
  void *userPtr;
};
