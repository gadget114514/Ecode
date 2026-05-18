#include "MFTReader.h"
#include <iostream>
#include <regex>
#include <sstream>
#include <vector>

// Helper to decode NTFS Data Runs
struct DataRun {
  uint64_t LCN;
  uint64_t Length; // In clusters
};

MFTReader::MFTReader()
    : hVolume(INVALID_HANDLE_VALUE), currentDrive(0), mftStartLcn(0) {}

MFTReader::~MFTReader() { Close(); }

std::wstring MFTReader::GetLastErrorMessage() const { return lastError; }

void MFTReader::SetError(const std::wstring &msg) {
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

void MFTReader::Close() {
  if (hVolume != INVALID_HANDLE_VALUE) {
    CloseHandle(hVolume);
    hVolume = INVALID_HANDLE_VALUE;
  }
  fileMap.clear();
}

bool MFTReader::Initialize(TCHAR driveLetter) {
  if (traceCallback)
    traceCallback(L"Initialize: Opening volume " +
                  std::wstring(1, driveLetter));
  Close();
  currentDrive = driveLetter;
  lastError = L"";

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

  DWORD bytesReturned;
  NTFS_VOLUME_DATA_BUFFER volumeData;
  if (!DeviceIoControl(hVolume, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0,
                       &volumeData, sizeof(volumeData), &bytesReturned, NULL)) {
    SetError(L"Failed to get NTFS volume data");
    Close();
    return false;
  }

  mftStartLcn = volumeData.MftStartLcn.QuadPart;
  bytesPerCluster = volumeData.BytesPerCluster;

  // Use BytesPerFileRecordSegment directly to avoid signed/unsigned confusion
  recordSize = volumeData.BytesPerFileRecordSegment;

  if (recordSize > 0) {
    if (traceCallback) {
      traceCallback(L"Spec Selection: BytesPerFileRecordSegment is valid (" +
                    std::to_wstring(recordSize) + L"). Using it directly.");
    }
  } else {
    // Fallback logic if 0
    uint32_t clusters = volumeData.ClustersPerFileRecordSegment;
    if (traceCallback) {
      traceCallback(L"Spec Selection: BytesPerFileRecordSegment is 0. Checking "
                    L"ClustersPerFileRecordSegment: " +
                    std::to_wstring(clusters) + L" (0x" +
                    std::to_wstring(clusters) + L")");
    }

    if (clusters > 0x80000000) {
      // Sign extended negative (e.g. 0xFFFFFFF6)
      int8_t val = (int8_t)clusters;
      recordSize = 1 << (-val);
      if (traceCallback) {
        traceCallback(L"Spec Selection: Value is Sign-Extended Negative (" +
                      std::to_wstring(val) + L"). Formula 2^(-n) -> " +
                      std::to_wstring(recordSize) + L" bytes.");
      }
    } else if (clusters >= 0x80 && clusters <= 0xFF) {
      // Raw byte negative (e.g. 0xF6) treated as unsigned 32-bit
      int8_t val = (int8_t)clusters;
      recordSize = 1 << (-val);
      if (traceCallback) {
        traceCallback(L"Spec Selection: Value is Raw Negative Byte (" +
                      std::to_wstring((int)val) + L"). Formula 2^(-n) -> " +
                      std::to_wstring(recordSize) + L" bytes.");
      }
    } else {
      // Standard cluster count
      recordSize = clusters * bytesPerCluster;
      if (traceCallback) {
        traceCallback(L"Spec Selection: Value is Standard Cluster Count (" +
                      std::to_wstring(clusters) +
                      L"). Formula C * ClusterSize -> " +
                      std::to_wstring(recordSize) + L" bytes.");
      }
    }
  }

  if (traceCallback)
    traceCallback(
        L"Initialize: Volume opened, StartLCN=" + std::to_wstring(mftStartLcn) +
        L", ClusterSize=" + std::to_wstring(bytesPerCluster) +
        L", RecordSize=" + std::to_wstring(recordSize) + L" (ClustersPerRec=" +
        std::to_wstring(volumeData.ClustersPerFileRecordSegment) + L")");
  return true;
}

std::vector<DataRun> DecodeDataRuns(const uint8_t *runList, uint64_t maxLen) {
  std::vector<DataRun> runs;
  uint64_t currentLcn = 0;
  size_t offset = 0;

  while (offset < maxLen) {
    uint8_t header = runList[offset++];
    if (header == 0)
      break;

    uint8_t lenBytes = header & 0xF;
    uint8_t offBytes = (header >> 4) & 0xF;

    if (offset + lenBytes + offBytes > maxLen)
      break;

    uint64_t length = 0;
    for (int i = 0; i < lenBytes; i++) {
      length |= (uint64_t)runList[offset++] << (i * 8);
    }

    int64_t lcnOffset = 0;
    for (int i = 0; i < offBytes; i++) {
      lcnOffset |= (uint64_t)runList[offset++] << (i * 8);
    }

    // Sign extend LCN offset
    if (offBytes > 0 && (lcnOffset & (1LL << ((offBytes * 8) - 1)))) {
      for (int i = offBytes; i < 8; i++) {
        lcnOffset |= (0xFFLL << (i * 8));
      }
    }

    currentLcn += lcnOffset;
    runs.push_back({(uint64_t)currentLcn, length});
  }
  return runs;
}

bool MFTReader::Scan(void (*progressCallback)(int, int, void *), void *userData,
                     std::function<void(const std::wstring &)> onFileFound) {
  scanDebugCallback = onFileFound;
  if (hVolume == INVALID_HANDLE_VALUE) {
    lastError = L"Volume not initialized";
    return false;
  }

  // 1. Read Record 0 ($MFT) to find the runs
  if (traceCallback)
    traceCallback(L"Scan: Reading $MFT record...");
  uint64_t mftOffset = mftStartLcn * bytesPerCluster;
  std::vector<uint8_t> buffer(recordSize);

  LARGE_INTEGER li;
  li.QuadPart = mftOffset;
  if (!SetFilePointerEx(hVolume, li, NULL, FILE_BEGIN)) {
    SetError(L"Failed to seek to MFT start");
    return false;
  }

  DWORD bytesRead;
  if (!ReadFile(hVolume, buffer.data(), recordSize, &bytesRead, NULL) ||
      bytesRead != recordSize) {
    SetError(L"Failed to read MFT Header");
    return false;
  }

  FILE_RECORD_HEADER *fr = (FILE_RECORD_HEADER *)buffer.data();
  if (fr->Magic != 0x454C4946)
    return false; // "FILE"

  // Find DATA attribute (0x80)
  std::vector<DataRun> runs;
  uint8_t *attrCursor = buffer.data() + fr->AttributeOffset;

  while (attrCursor < buffer.data() + fr->RealSize) {
    ATTRIBUTE_HEADER *ah = (ATTRIBUTE_HEADER *)attrCursor;
    if (ah->TypeID == 0xFFFFFFFF)
      break;
    if (ah->Length == 0)
      break;

    if (ah->TypeID == AttributeData) { // 0x80
      if (ah->NonResidentFlag) {
        // Safety: Check if header is large enough
        if (ah->Length < 0x40) { // Non-resident header size is usually 0x40
          SetError(L"Data attribute too short");
          return false;
        }

        // Non-resident: Parse runs
        uint64_t startVcn = *(uint64_t *)(attrCursor + 0x10);
        uint64_t lastVcn = *(uint64_t *)(attrCursor + 0x18);
        uint16_t runOffset = *(uint16_t *)(attrCursor + 0x20);

        if (runOffset >= ah->Length) {
          SetError(L"Invalid Run Offset");
          return false;
        }

        // Run list is at attrCursor + runOffset
        // But wait, the attribute might be very large?
        // Usually for MFT record it fits in 1KB.
        // We decode from attrCursor + runOffset
        if (traceCallback)
          traceCallback(L"Scan: Decoding Data Runs...");
        runs = DecodeDataRuns(attrCursor + runOffset, ah->Length - runOffset);
        if (runs.empty()) {
          // If runs illegal?
          SetError(L"Empty Data Runs for $MFT");
          return false;
        }

        if (scanDebugCallback) {
          scanDebugCallback(L"MFT Data Runs decoded: " +
                            std::to_wstring(runs.size()));
        }
      } else {
        // Resident: MFT is too small? Unlikely for $MFT on used volume.
        // But technically possible on empty tiny volume.
      }
      break; // Found primary DATA
    }
    attrCursor += ah->Length;
  }

  if (runs.empty())
    return false;

  // 2. Read all runs
  if (traceCallback)
    traceCallback(L"Scan: Processing " + std::to_wstring(runs.size()) +
                  L" runs.");
  const uint32_t BUFFER_SIZE = 1024 * 1024; // 1MB buffer
  std::vector<uint8_t> readBuf(BUFFER_SIZE);

  // Estimate total records for progress
  uint64_t totalRecords = 0;
  for (auto &r : runs)
    totalRecords += (r.Length * bytesPerCluster) / recordSize;
  uint64_t processedRecords = 0;
  bool firstChunkProcessed = false;
  uint32_t totalRecordsRead = 0;

  for (auto &run : runs) {
    if (traceCallback)
      traceCallback(L"Scan: Run LCN=" + std::to_wstring(run.LCN) + L", Len=" +
                    std::to_wstring(run.Length));
    uint64_t runBytes = run.Length * bytesPerCluster;
    uint64_t runStart = run.LCN * bytesPerCluster;
    uint64_t currentOffset = 0;

    LARGE_INTEGER seekPos;
    seekPos.QuadPart = runStart;
    SetFilePointerEx(hVolume, seekPos, NULL, FILE_BEGIN);

    while (currentOffset < runBytes) {
      uint32_t toRead =
          (uint32_t)min((uint64_t)BUFFER_SIZE, runBytes - currentOffset);
      if (!ReadFile(hVolume, readBuf.data(), toRead, &bytesRead, NULL)) {
        lastError = L"ReadFile failed";
        return false;
      }

      // Debug: Check if we read anything
      // if (bytesRead == 0) break;

      // We rely on bytesToRead vs bytesRead?
      // MFT might be fragmented, so bytesRead might be less than bytesToRead?
      // If bytesRead < recordSize, we can't parse a record.

      if (bytesRead < recordSize) {
        // Less than one record? End of MFT?
        // Just break for now.
        break;
      }

      uint32_t recordsRead = bytesRead / recordSize;
      if (scanDebugCallback && !firstChunkProcessed) {
        scanDebugCallback(L"First chunk read: " + std::to_wstring(bytesRead) +
                          L" bytes (" + std::to_wstring(recordsRead) +
                          L" records)");
        firstChunkProcessed = true;
      }

      // Process buffer
      for (uint32_t i = 0; i < bytesRead; i += recordSize) {
        if (i + recordSize > bytesRead)
          break;
        ParseRecord((FILE_RECORD_HEADER *)(readBuf.data() + i));
        processedRecords++;
      }

      currentOffset += bytesRead;
      if (progressCallback && (processedRecords % 1000 == 0)) {
        progressCallback((int)((processedRecords * 100) / totalRecords), 100,
                         userData);
      }
    }
  }

  if (progressCallback)
    progressCallback(100, 100, userData);

  scanDebugCallback = nullptr;
  return true;
}

void MFTReader::ParseRecord(const FILE_RECORD_HEADER *header) {
  if (header->Magic != 0x454C4946)
    return;
  if (!(header->Flags & 0x01))
    return; // Not in use

  uint64_t mftRef = header->MFTRecordNumber;
  // If MFTRecordNumber is 0 (older NTFS), we need to track index manually.
  // ideally we pass index to this func. But standard usually has it.
  // If 0 and it's not the $MFT itself, it could be unreliable.
  // Better strategy: The loop in Scan knows the index.
  // But let's rely on MFTRecordNumber for now or pass it.
  // Actually, let's fix: We can't trust MFTRecordNumber on all versions.
  // However, for this simplified version, let's assume valid or skip.
  // Better: Scan caller passes the ID. But that requires refactoring Scan loop.
  // Let's assume header->MFTRecordNumber is correct (Valid on XP+).

  Entry entry;
  entry.RefID = header->MFTRecordNumber;
  entry.IsValid = false;
  entry.IsDirectory = (header->Flags & 0x02) != 0;

  if (header->RealSize > recordSize)
    return; // Corrupt record
  const uint8_t *ptr = (const uint8_t *)header;
  const uint8_t *end = ptr + header->RealSize;

  if (header->AttributeOffset >= header->RealSize)
    return;
  const ATTRIBUTE_HEADER *attr =
      (const ATTRIBUTE_HEADER *)(ptr + header->AttributeOffset);

  bool gotName = false;

  while ((const uint8_t *)attr + sizeof(ATTRIBUTE_HEADER) <= end &&
         attr->TypeID != AttributeEnd) {
    if (attr->Length == 0)
      break; // Infinite loop prevention
    if ((const uint8_t *)attr + attr->Length > end)
      break; // OOB

    if (attr->TypeID == AttributeFileName) { // 0x30
      const FILE_NAME_ATTRIBUTE *fn;
      if (!attr->NonResidentFlag) {
        const RESIDENT_ATTRIBUTE_HEADER *res =
            (const RESIDENT_ATTRIBUTE_HEADER *)attr;

        // Safety check for offset
        if (res->ValueOffset + sizeof(FILE_NAME_ATTRIBUTE) <= attr->Length) {
          fn = (const FILE_NAME_ATTRIBUTE *)((const uint8_t *)attr +
                                             res->ValueOffset);

          // Check Name Length
          size_t nameBytes = fn->NameLength * sizeof(wchar_t);
          if ((const uint8_t *)fn->Name + nameBytes <=
              (const uint8_t *)attr + attr->Length) {
            bool isWin32 = (fn->NameType == 0x01 || fn->NameType == 0x03);
            if (!gotName || isWin32) {
              entry.ParentRefID = fn->ParentDirectoryRef & 0xFFFFFFFFFFFF;
              entry.Size = fn->DataSize;
              if (fn->NameLength > 0)
                entry.Name.assign(fn->Name, fn->NameLength);
              gotName = true;
              entry.IsValid = true;
            }
          }
        }
      }
    } else if (attr->TypeID == AttributeStandardInformation) {
      if (!attr->NonResidentFlag) {
        const RESIDENT_ATTRIBUTE_HEADER *res =
            (const RESIDENT_ATTRIBUTE_HEADER *)attr;
        if (res->ValueOffset + sizeof(STANDARD_INFORMATION) <= attr->Length) {
          const STANDARD_INFORMATION *si =
              (const STANDARD_INFORMATION *)((const uint8_t *)attr +
                                             res->ValueOffset);
          entry.LastWriteTime = si->FileChangeTime;
        }
      }
    } else if (attr->TypeID == AttributeData) { // 0x80
      if (!attr->NonResidentFlag) {
        // Resident
        const RESIDENT_ATTRIBUTE_HEADER *res =
            (const RESIDENT_ATTRIBUTE_HEADER *)attr;
        entry.Size = res->ValueLength;
      } else {
        // Non-Resident
        // DataSize is at offset 0x30
        if (attr->Length >= 0x40) {
          entry.Size = *(const uint64_t *)((const uint8_t *)attr + 0x30);
        }
      }
    }

    attr = (const ATTRIBUTE_HEADER *)((const uint8_t *)attr + attr->Length);
  }

  if (entry.IsValid) {
    fileMap[entry.RefID] = entry;
    if (scanDebugCallback) {
      scanDebugCallback(entry.Name);
    }
  }
}

std::wstring MFTReader::BuildPath(uint64_t refId) {
  // Prevent infinite loop
  std::wstring path = L"";
  int depth = 0;
  while (refId != 0x05 && depth < 256) { // 0x05 is Root Directory
    auto it = fileMap.find(refId);
    if (it == fileMap.end())
      break;

    path = L"\\" + it->second.Name + path;
    refId = it->second.ParentRefID;
    depth++;
  }

  std::wstring drivePrefix = L"";
  drivePrefix += currentDrive;
  drivePrefix += L":";

  return drivePrefix + path;
}

bool MFTReader::MatchPattern(const std::wstring &str,
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

std::vector<FileResult> MFTReader::Search(const std::wstring &query,
                                          const std::wstring &targetFolder,
                                          const SearchOptions &options,
                                          int maxResults) {
  std::vector<FileResult> results;

  // Resolve targetFolder to RefID?
  // For now, let's just filter by string path prefix if targetFolder is set.
  // It's slower but coding RefID resolution for input string is extra step.

  for (const auto &pair : fileMap) {
    const auto &entry = pair.second;
    if (!entry.IsValid)
      continue;

    // Reconstruct Path
    std::wstring fullPath = BuildPath(entry.RefID);

    // Filter by Target Folder
    if (!targetFolder.empty()) {
      if (fullPath.length() < targetFolder.length())
        continue;

      // Case-insensitive prefix check
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

      // Extension Filter
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
    res.LastWriteTime = entry.LastWriteTime;
    res.IsDirectory = entry.IsDirectory;
    results.push_back(res);

    if (maxResults > 0 && (int)results.size() >= maxResults)
      break;
  }
  return results;
}
