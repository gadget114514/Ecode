#include "FatReader.h"
#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>

FatReader::FatReader() : hVolume(INVALID_HANDLE_VALUE), currentDrive(0) {}
FatReader::~FatReader() { Close(); }

void FatReader::SetError(const std::wstring &msg) {
  DWORD err = GetLastError();
  LPVOID lpMsgBuf;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPWSTR)&lpMsgBuf, 0, NULL);
  lastError = msg + L": " + (lpMsgBuf ? (wchar_t *)lpMsgBuf : L"Unknown Error");
  if (lpMsgBuf)
    LocalFree(lpMsgBuf);
}

void FatReader::Close() {
  if (hVolume != INVALID_HANDLE_VALUE) {
    CloseHandle(hVolume);
    hVolume = INVALID_HANDLE_VALUE;
  }
  fileMap.clear();
  fatCache.clear();
}

std::wstring FatReader::GetLastErrorMessage() const { return lastError; }

bool FatReader::Initialize(TCHAR driveLetter) {
  Close();
  currentDrive = driveLetter;

  std::wstring path = L"\\\\.\\";
  path += driveLetter;
  path += L":";

  hVolume = CreateFileW(path.c_str(), GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                        0, NULL);
  if (hVolume == INVALID_HANDLE_VALUE) {
    SetError(L"Failed to open volume");
    return false;
  }

  uint8_t bootSector[512];
  DWORD bytesRead;
  if (!ReadFile(hVolume, bootSector, 512, &bytesRead, NULL) ||
      bytesRead != 512) {
    SetError(L"Failed to read boot sector");
    return false;
  }

  FAT16_BPB *bpb16 = (FAT16_BPB *)bootSector;
  bytesPerSector = bpb16->BytesPerSector;
  sectorsPerCluster = bpb16->SectorsPerCluster;
  reservedSectors = bpb16->ReservedSectors;
  fatCount = bpb16->Fats;

  if (bpb16->SectorsPerFat != 0) {
    // FAT12/16
    isFat32 = false;
    sectorsPerFat = bpb16->SectorsPerFat;
    rootDirEntries = bpb16->RootEntries;
    rootDirSectors =
        ((rootDirEntries * 32) + (bytesPerSector - 1)) / bytesPerSector;
    firstDataSector =
        reservedSectors + (fatCount * sectorsPerFat) + rootDirSectors;
  } else {
    // FAT32
    isFat32 = true;
    FAT32_BPB *bpb32 = (FAT32_BPB *)bootSector;
    sectorsPerFat = bpb32->SectorsPerFat32;
    rootDirEntries = 0;
    rootDirSectors = 0;
    rootCluster = bpb32->RootCluster;
    firstDataSector = reservedSectors + (fatCount * sectorsPerFat);
  }

  // Load FAT for fast traversal
  LoadFat();

  return true;
}

void FatReader::LoadFat() {
  if (hVolume == INVALID_HANDLE_VALUE)
    return;

  uint64_t fatOffset = (uint64_t)reservedSectors * bytesPerSector;
  uint32_t fatSize = sectorsPerFat * bytesPerSector;

  fatCache.resize(fatSize);

  LARGE_INTEGER li;
  li.QuadPart = fatOffset;
  SetFilePointerEx(hVolume, li, NULL, FILE_BEGIN);

  DWORD bytesRead;
  ReadFile(hVolume, fatCache.data(), fatSize, &bytesRead, NULL);

  // Count Used Clusters
  totalUsedClusters = 0;
  if (isFat32) {
    uint32_t *fat32 = (uint32_t *)fatCache.data();
    uint32_t count = (uint32_t)fatCache.size() / 4;
    for (uint32_t i = 2; i < count; i++) {
      uint32_t val = fat32[i] & 0x0FFFFFFF;
      if (val != 0 && val != 0x0FFFFFFF && val < 0x0FFFFFF7)
        totalUsedClusters++;
    }
  } else {
    uint16_t *fat16 = (uint16_t *)fatCache.data();
    uint32_t count = (uint32_t)fatCache.size() / 2;
    for (uint32_t i = 2; i < count; i++) {
      uint16_t val = fat16[i];
      if (val != 0 && val < 0xFFF7)
        totalUsedClusters++;
    }
  }
}

uint32_t FatReader::GetNextCluster(uint32_t cluster) {
  if (isFat32) {
    if (cluster * 4 + 4 > fatCache.size())
      return 0x0FFFFFFF;
    uint32_t next = *(uint32_t *)(&fatCache[cluster * 4]);
    return next & 0x0FFFFFFF;
  } else {
    if (cluster * 2 + 2 > fatCache.size())
      return 0xFFFF;
    uint32_t next = *(uint16_t *)(&fatCache[cluster * 2]);
    return next;
  }
}

uint64_t FatReader::ClusterToSector(uint32_t cluster) {
  return firstDataSector + (uint64_t)(cluster - 2) * sectorsPerCluster;
}

bool FatReader::Scan(int codePage, void (*progressCallback)(int, int, void *),
                     void *userData,
                     std::function<void(const std::wstring &)> onFileFound) {
  if (hVolume == INVALID_HANDLE_VALUE)
    return false;

  progressCb = progressCallback;
  userPtr = userData;
  processedClusters = 0;

  // Root directory
  if (isFat32) {
    if (totalUsedClusters == 0)
      totalUsedClusters = 1; // Prevent div/0
    ProcessDirectory(rootCluster, 0, L"", codePage);
  } else {
    // FAT16 Root is fixed
    uint64_t rootOffset =
        (uint64_t)(reservedSectors + (fatCount * sectorsPerFat)) *
        bytesPerSector;
    uint32_t rootSize = rootDirEntries * 32;
    std::vector<uint8_t> buffer(rootSize);

    LARGE_INTEGER li;
    li.QuadPart = rootOffset;
    SetFilePointerEx(hVolume, li, NULL, FILE_BEGIN);

    DWORD bytesRead;
    ReadFile(hVolume, buffer.data(), rootSize, &bytesRead, NULL);

    // Manual processing of root buffer since it's not a cluster chain
    std::wstring lfn = L"";
    for (uint32_t i = 0; i < rootSize; i += 32) {
      FAT_DIRECTORY_ENTRY *de = (FAT_DIRECTORY_ENTRY *)&buffer[i];
      if (de->Name[0] == 0x00)
        break;
      if (de->Name[0] == 0xE5) {
        lfn = L"";
        continue;
      }

      if (de->Attributes == FAT_ATTR_LFN) {
        FAT_LFN_ENTRY *le = (FAT_LFN_ENTRY *)de;
        wchar_t part[14] = {0};
        memcpy(part, le->Name1, 10);
        memcpy(part + 5, le->Name2, 12);
        memcpy(part + 11, le->Name3, 4);
        lfn = std::wstring(part) + lfn;
        continue;
      }

      if (de->Attributes & FAT_ATTR_VOLUME_ID) {
        lfn = L"";
        continue;
      }

      Entry entry;
      entry.FirstCluster =
          de->FirstClusterLow | ((uint32_t)de->FirstClusterHigh << 16);
      entry.ParentFirstCluster = 0;

      if (!lfn.empty()) {
        // Trim trailing nulls or 0xFFFF from LFN
        size_t last = lfn.find_first_of(L"\0\xFFFF");
        if (last != std::wstring::npos)
          lfn.resize(last);
        entry.Name = lfn;
      } else {
        // SFN
        char sfn[13];
        int p = 0;
        for (int j = 0; j < 8; j++)
          if (de->Name[j] != ' ')
            sfn[p++] = de->Name[j];
        if (de->Name[8] != ' ') {
          sfn[p++] = '.';
          for (int j = 8; j < 11; j++)
            if (de->Name[j] != ' ')
              sfn[p++] = de->Name[j];
        }
        sfn[p] = 0;

        int len = MultiByteToWideChar(codePage, 0, sfn, -1, NULL, 0);
        wchar_t *wname = new wchar_t[len];
        MultiByteToWideChar(codePage, 0, sfn, -1, wname, len);
        entry.Name = wname;
        delete[] wname;
      }

      entry.Size = de->FileSize;
      entry.IsDirectory = (de->Attributes & FAT_ATTR_DIRECTORY) != 0;
      entry.IsValid = true;

      uint32_t id = entry.FirstCluster;
      if (id == 0)
        id = 0x80000000 + i;

      fileMap[id] = entry;
      lfn = L"";

      if (entry.IsDirectory && entry.FirstCluster != 0) {
        ProcessDirectory(entry.FirstCluster, 0, entry.Name, codePage);
      }
    }
  }

  if (progressCb)
    progressCb(100, 100, userPtr);

  return true;
}

void FatReader::ProcessDirectory(uint32_t cluster, uint32_t parentCluster,
                                 const std::wstring &parentPath, int codePage) {
  uint32_t current = cluster;
  std::wstring lfn = L"";

  // Calculate cluster size in bytes
  uint32_t clusterBytes = bytesPerSector * sectorsPerCluster;
  std::vector<uint8_t> buffer(clusterBytes);

  while (current < (uint32_t)(isFat32 ? 0x0FFFFFF8 : 0xFFF8)) {
    // Progress Update
    processedClusters++;
    if (progressCb && (processedClusters % 100 == 0)) {
      progressCb((int)((processedClusters * 100) / totalUsedClusters), 100,
                 userPtr);
    }

    uint64_t sector = ClusterToSector(current);
    LARGE_INTEGER li;
    li.QuadPart = sector * bytesPerSector;
    SetFilePointerEx(hVolume, li, NULL, FILE_BEGIN);

    DWORD bytesRead;
    if (!ReadFile(hVolume, buffer.data(), (DWORD)buffer.size(), &bytesRead,
                  NULL))
      break;

    for (uint32_t i = 0; i < bytesRead; i += 32) {
      FAT_DIRECTORY_ENTRY *de = (FAT_DIRECTORY_ENTRY *)&buffer[i];
      if (de->Name[0] == 0x00)
        return; // End of dir
      if (de->Name[0] == 0xE5) {
        lfn = L"";
        continue;
      } // Deleted

      if (de->Attributes == FAT_ATTR_LFN) {
        FAT_LFN_ENTRY *le = (FAT_LFN_ENTRY *)de;
        wchar_t part[14] = {0};
        memcpy(part, le->Name1, 10);
        memcpy(part + 5, le->Name2, 12);
        memcpy(part + 11, le->Name3, 4);
        lfn = std::wstring(part) + lfn;
        continue;
      }

      if (de->Attributes & FAT_ATTR_VOLUME_ID) {
        lfn = L"";
        continue;
      }

      // Skip "." and ".."
      if (de->Name[0] == '.') {
        lfn = L"";
        continue;
      }

      Entry entry;
      entry.FirstCluster =
          de->FirstClusterLow | ((uint32_t)de->FirstClusterHigh << 16);
      entry.ParentFirstCluster = cluster;

      if (!lfn.empty()) {
        size_t last = lfn.find_first_of(L"\0\xFFFF");
        if (last != std::wstring::npos)
          lfn.resize(last);
        entry.Name = lfn;
      } else {
        char sfn[13];
        int p = 0;
        for (int j = 0; j < 8; j++)
          if (de->Name[j] != ' ')
            sfn[p++] = de->Name[j];
        if (de->Name[8] != ' ') {
          sfn[p++] = '.';
          for (int j = 8; j < 11; j++)
            if (de->Name[j] != ' ')
              sfn[p++] = de->Name[j];
        }
        sfn[p] = 0;

        int len = MultiByteToWideChar(codePage, 0, sfn, -1, NULL, 0);
        wchar_t *wname = new wchar_t[len];
        MultiByteToWideChar(codePage, 0, sfn, -1, wname, len);
        entry.Name = wname;
        delete[] wname;
      }

      entry.Size = de->FileSize;
      entry.IsDirectory = (de->Attributes & FAT_ATTR_DIRECTORY) != 0;
      entry.IsValid = true;

      uint32_t id = entry.FirstCluster;
      if (id == 0)
        id = 0x80000000 + (uint32_t)fileMap.size(); // Simplified synthetic ID

      fileMap[id] = entry;
      lfn = L"";

      // If it's a file, we "processed" its clusters by skipping them
      if (!entry.IsDirectory) {
        uint32_t fileClusters = (entry.Size + clusterBytes - 1) / clusterBytes;
        processedClusters += fileClusters;
      }

      if (entry.IsDirectory && entry.FirstCluster != 0) {
        ProcessDirectory(entry.FirstCluster, cluster,
                         parentPath + L"\\" + entry.Name, codePage);
      }
    }

    current = GetNextCluster(current);
  }
}

std::wstring FatReader::BuildPath(const Entry &entry) {
  std::wstring path = entry.Name;
  uint32_t pc = entry.ParentFirstCluster;
  while (pc != 0) {
    auto it = fileMap.find(pc);
    if (it == fileMap.end())
      break;
    path = it->second.Name + L"\\" + path;
    pc = it->second.ParentFirstCluster;
  }
  std::wstring drive = L"";
  drive += currentDrive;
  drive += L":\\";
  return drive + path;
}

bool FatReader::MatchPattern(const std::wstring &str,
                             const std::wstring &pattern,
                             const SearchOptions &options) {
  if (pattern.empty())
    return true;

  bool matched = false;

  if (options.mode == MatchMode_Exact) {
    if (options.ignoreCase) {
      matched = lstrcmpiW(str.c_str(), pattern.c_str()) == 0;
    } else {
      matched = wcscmp(str.c_str(), pattern.c_str()) == 0;
    }
  } else if (options.mode == MatchMode_RegEx) {
    try {
      std::regex_constants::syntax_option_type flags = std::regex::ECMAScript;
      if (options.ignoreCase)
        flags |= std::regex::icase;
      std::wregex re(pattern, flags);
      matched = std::regex_search(str, re);
    } catch (...) {
      matched = false;
    }
  } else if (options.mode == MatchMode_SpaceDivided) {
    std::wstringstream ss(pattern);
    std::wstring token;
    bool allTokensFound = true;
    while (ss >> token) {
      bool tokenFound = false;
      if (options.ignoreCase) {
        auto it = std::search(str.begin(), str.end(), token.begin(),
                              token.end(), [](wchar_t c1, wchar_t c2) {
                                return towlower(c1) == towlower(c2);
                              });
        tokenFound = (it != str.end());
      } else {
        tokenFound = (str.find(token) != std::wstring::npos);
      }
      if (!tokenFound) {
        allTokensFound = false;
        break;
      }
    }
    matched = allTokensFound;
  } else {
    // Default: Substring
    if (options.ignoreCase) {
      auto it = std::search(
          str.begin(), str.end(), pattern.begin(), pattern.end(),
          [](wchar_t c1, wchar_t c2) { return towlower(c1) == towlower(c2); });
      matched = it != str.end();
    } else {
      matched = str.find(pattern) != std::wstring::npos;
    }
  }

  if (options.invertMatch)
    return !matched;
  return matched;
}

std::vector<FileResult>
FatReader::Search(const std::wstring &query, const std::wstring &targetFolder,
                  int codePage, const SearchOptions &options, int maxResults) {
  std::vector<FileResult> results;
  for (auto const &[id, entry] : fileMap) {
    if (MatchPattern(entry.Name, query, options)) {
      std::wstring fullPath = BuildPath(entry);

      // Filter by Target Folder
      if (!targetFolder.empty()) {
        if (fullPath.length() < targetFolder.length())
          continue;
        bool match = true;
        for (size_t i = 0; i < targetFolder.length(); ++i) {
          if (towlower(fullPath[i]) != towlower(targetFolder[i])) {
            match = false;
            break;
          }
        }
        if (!match)
          continue;
      }

      // Exclusion Filter
      if (!options.excludePattern.empty()) {
        bool excluded = false;
        std::wstring targetForExclude = fullPath;
        std::wstringstream ss(options.excludePattern);
        std::wstring pattern;
        while (std::getline(ss, pattern, L';')) {
          if (pattern.empty())
            continue;
          if (options.ignoreCase) {
            std::wstring targetLower = targetForExclude;
            std::wstring patternLower = pattern;
            for (auto &c : targetLower)
              c = towlower(c);
            for (auto &c : patternLower)
              c = towlower(c);
            if (targetLower.find(patternLower) != std::wstring::npos) {
              excluded = true;
              break;
            }
          } else {
            if (targetForExclude.find(pattern) != std::wstring::npos) {
              excluded = true;
              break;
            }
          }
        }
        if (excluded)
          continue;
      }

      // Match Query (Name or Full Path)
      if (!MatchPattern(options.matchFullPath ? fullPath : entry.Name, query,
                        options))
        continue;

      // Metadata Filters
      if (options.minSize > 0 && entry.Size < options.minSize)
        continue;
      if (options.maxSize > 0 && entry.Size > options.maxSize)
        continue;
      if (options.minDate > 0 && entry.LastWriteTime < options.minDate)
        continue;
      if (options.maxDate > 0 && entry.LastWriteTime > options.maxDate)
        continue;

      // Type Filter
      if (entry.IsDirectory) {
        if (!options.includeFolders)
          continue;
      } else {
        if (!options.includeFiles)
          continue;

        // Extension Filter (Logical AND with name match)
        if (!options.extensionFilter.empty()) {
          bool extMatch = false;
          size_t dotPos = entry.Name.find_last_of(L'.');
          std::wstring fileExt = (dotPos != std::wstring::npos)
                                     ? entry.Name.substr(dotPos + 1)
                                     : L"";
          for (auto &c : fileExt)
            c = towlower(c);

          std::wstringstream ss(options.extensionFilter);
          std::wstring extToken;
          while (std::getline(ss, extToken, L';')) {
            if (extToken.empty())
              continue;
            for (auto &c : extToken)
              c = towlower(c);
            if (fileExt == extToken) {
              extMatch = true;
              break;
            }
          }
          if (!extMatch)
            continue;
        }
      }

      FileResult res;
      res.Name = entry.Name;
      res.FullPath = fullPath;
      res.Size = entry.Size;
      res.LastWriteTime = 0; // Simplified
      res.IsDirectory = entry.IsDirectory;
      results.push_back(res);
      if (maxResults > 0 && (int)results.size() >= maxResults)
        break;
    }
  }
  return results;
}
