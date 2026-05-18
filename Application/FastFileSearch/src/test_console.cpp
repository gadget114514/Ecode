#include "FatReader.h"
#include "MFTReader.h"
#include "exFatReader.h"
#include <functional> // Added for std::function
#include <iostream>
#include <locale.h>
#include <vector> // Added for std::vector
#include <windows.h>

int wmain(int argc, wchar_t *argv[]) {
  setlocale(LC_ALL, ""); // Use system locale
  std::wcout << L"Starting Test Program..." << std::endl;

  std::vector<std::wstring> args;
  for (int i = 1; i < argc; i++)
    args.push_back(argv[i]);

  bool verbose = false;
  bool trace = false;
  std::wstring target = L"D:";
  std::wstring query = L"ws";
  SearchOptions options;
  options.ignoreCase = false; // Default to sensitive in CLI for common usage?
  // User said "case ignore option is global option".
  // In GUI I defaulted it to true. Let's keep consistency.
  options.ignoreCase = true;

  // Extract flags
  for (auto it = args.begin(); it != args.end();) {
    if (*it == L"-v") {
      verbose = true;
      it = args.erase(it);
    } else if (*it == L"-t") {
      trace = true;
      it = args.erase(it);
    } else if (*it == L"-e") {
      options.mode = MatchMode_Exact;
      it = args.erase(it);
    } else if (*it == L"-s") {
      options.mode = MatchMode_SpaceDivided;
      it = args.erase(it);
    } else if (*it == L"-r") {
      options.mode = MatchMode_RegEx;
      it = args.erase(it);
    } else if (*it == L"-i") {
      options.ignoreCase = true;
      it = args.erase(it);
    } else if (*it == L"-I") { // CAPITAL I for case sensitive?
      options.ignoreCase = false;
      it = args.erase(it);
    } else if (*it == L"--min-size" && std::next(it) != args.end()) {
      it = args.erase(it);
      options.minSize = _wtoi64(it->c_str());
      it = args.erase(it);
    } else if (*it == L"--max-size" && std::next(it) != args.end()) {
      it = args.erase(it);
      options.maxSize = _wtoi64(it->c_str());
      it = args.erase(it);
    } else if (*it == L"--min-date" && std::next(it) != args.end()) {
      it = args.erase(it);
      SYSTEMTIME st = {0};
      swscanf(it->c_str(), L"%hu-%hu-%hu", &st.wYear, &st.wMonth, &st.wDay);
      FILETIME ft; SystemTimeToFileTime(&st, &ft);
      ULARGE_INTEGER uli; uli.LowPart = ft.dwLowDateTime; uli.HighPart = ft.dwHighDateTime;
      options.minDate = uli.QuadPart;
      it = args.erase(it);
    } else if (*it == L"--max-date" && std::next(it) != args.end()) {
      it = args.erase(it);
      SYSTEMTIME st = {0};
      swscanf(it->c_str(), L"%hu-%hu-%hu", &st.wYear, &st.wMonth, &st.wDay);
      st.wHour = 23; st.wMinute = 59; st.wSecond = 59;
      FILETIME ft; SystemTimeToFileTime(&st, &ft);
      ULARGE_INTEGER uli; uli.LowPart = ft.dwLowDateTime; uli.HighPart = ft.dwHighDateTime;
      options.maxDate = uli.QuadPart;
      it = args.erase(it);
    } else if (*it == L"--files-only") {
      options.includeFiles = true;
      options.includeFolders = false;
      it = args.erase(it);
    } else if (*it == L"--folders-only") {
      options.includeFiles = false;
      options.includeFolders = true;
      it = args.erase(it);
    } else if (*it == L"--ext" && std::next(it) != args.end()) {
      it = args.erase(it);
      options.extensionFilter = *it;
      it = args.erase(it);
    } else if (*it == L"--full-path") {
      options.matchFullPath = true;
      it = args.erase(it);
    } else if (*it == L"--exclude" && std::next(it) != args.end()) {
      it = args.erase(it);
      options.excludePattern = *it;
      it = args.erase(it);
    } else {
      ++it;
    }
  }

  // Parse positional
  if (args.size() > 0)
    target = args[0];
  if (args.size() > 1)
    query = args[1];
  else if (args.size() == 1 && target.find(L":") == std::wstring::npos) {
    query = target;
    target = L"D:";
  }

  std::wcout << L"Verbose: " << (verbose ? L"Yes" : L"No") << L"\nTrace: "
             << (trace ? L"Yes" : L"No") << L"\nMode: " 
             << (options.mode == MatchMode_Exact ? L"Exact" : 
                 options.mode == MatchMode_SpaceDivided ? L"SpaceDivided" : 
                 options.mode == MatchMode_RegEx ? L"RegEx" : L"Substring")
             << L"\nIgnoreCase: " << (options.ignoreCase ? L"Yes" : L"No")
             << L"\nMinSize: " << options.minSize << L" bytes"
             << L"\nMaxSize: " << options.maxSize << L" bytes"
             << L"\nMinDate: " << options.minDate
             << L"\nMaxDate: " << options.maxDate
             << L"\nIncludeFiles: " << (options.includeFiles ? L"Yes" : L"No")
             << L"\nIncludeFolders: " << (options.includeFolders ? L"Yes" : L"No")
             << L"\nExtensionFilter: " << options.extensionFilter
             << L"\nMatchFullPath: " << (options.matchFullPath ? L"Yes" : L"No")
             << L"\nExcludePattern: " << options.excludePattern
             << std::endl;

  wchar_t drive = towupper(target[0]);

  std::wcout << L"Initializing Drive " << drive << L"..." << std::endl;

  wchar_t driveRoot[] = {drive, L':', L'\\', L'\0'};
  wchar_t fsName[MAX_PATH];
  GetVolumeInformationW(driveRoot, NULL, 0, NULL, NULL, NULL, fsName, MAX_PATH);

  std::wcout << L"Detected File System: " << fsName << std::endl;

  std::vector<FileResult> searchResults;

  // Lambda for search/print
  auto PerformSearch = [&](auto &rdr) {
    if (trace) {
      rdr.SetTraceCallback([](const std::wstring &msg) {
        std::wcout << L"[TRACE] " << msg << std::endl;
      });
    }

    if (!rdr.Initialize(drive)) {
      std::wcout << L"Failed to initialize: " << rdr.GetLastErrorMessage()
                 << std::endl;
      return false;
    }

    auto callback = [](int percent, int max, void *userData) {
      if (percent % 10 == 0)
        std::wcout << L"Scan Progress: " << percent << L"%" << std::endl;
    };

    // FAT specific Scan signature check
    // For generic template usage we might need `if constexpr` or specialized
    // override But here we can manually call.
    return true; // placeholder
  };

  auto callback = [](int percent, int max, void *userData) {
    (void)max;
    (void)userData;
    if (percent % 10 == 0)
      std::wcout << L"Scan Progress: " << percent << L"%" << std::endl;
  };

  std::function<void(const std::wstring &)> verboseCb = nullptr;
  if (verbose) {
    verboseCb = [](const std::wstring &name) {
      std::wcout << L"File: " << name << L"\n";
    };
  }

  if (wcscmp(fsName, L"NTFS") == 0) {
    MFTReader r;
    if (trace)
      r.SetTraceCallback([](const std::wstring &msg) {
        std::wcout << L"[TRACE] " << msg << std::endl;
      });
    if (!r.Initialize(drive)) {
      std::wcout << L"Init Failed: " << r.GetLastErrorMessage() << std::endl;
      return 1;
    }

    if (!r.Scan(callback, nullptr, verboseCb)) {
      std::wcout << L"Scan Failed: " << r.GetLastErrorMessage() << std::endl;
      return 1;
    }
    searchResults = r.Search(query, target, options);

  } else if (wcscmp(fsName, L"FAT") == 0 || wcscmp(fsName, L"FAT32") == 0) {
    FatReader r;
    if (trace)
      r.SetTraceCallback([](const std::wstring &msg) {
        std::wcout << L"[TRACE] " << msg << std::endl;
      });
    if (!r.Initialize(drive)) {
      std::wcout << L"Init Failed: " << r.GetLastErrorMessage() << std::endl;
      return 1;
    }

    // Use default Code Page (OEM)
    if (!r.Scan(CP_OEMCP, callback, nullptr, verboseCb)) {
      std::wcout << L"Scan Failed: " << r.GetLastErrorMessage() << std::endl;
      return 1;
    }
    searchResults = r.Search(query, target, CP_OEMCP, options);

  } else if (wcscmp(fsName, L"exFAT") == 0) {
    exFatReader r;
    if (trace)
      r.SetTraceCallback([](const std::wstring &msg) {
        std::wcout << L"[TRACE] " << msg << std::endl;
      });
    if (!r.Initialize(drive)) {
      std::wcout << L"Init Failed: " << r.GetLastErrorMessage() << std::endl;
      return 1;
    }

    if (!r.Scan(callback, nullptr, verboseCb)) {
      std::wcout << L"Scan Failed: " << r.GetLastErrorMessage() << std::endl;
      return 1;
    }
    searchResults = r.Search(query, target, options);
  } else {
    std::wcout << L"Unsupported File System." << std::endl;
    return 1;
  }

  std::wcout << L"Scan Complete." << std::endl;
  if (verbose) {
    std::wcout << L"--------------------------------------------------\n";
  }

  std::wcout << L"Searching for '" << query << L"' in '" << target << L"'..."
             << std::endl;

  std::wcout << L"Found " << searchResults.size() << L" results." << std::endl;

  if (searchResults.empty()) {
    std::wcout << L"No items found matching the query." << std::endl;
  } else {
    std::wcout << L"Listing results (limit 100):" << std::endl;
    for (size_t i = 0; i < min(searchResults.size(), 100); ++i) {
      std::wcout << searchResults[i].FullPath << L"\n";
    }
  }

  return 0;
}
