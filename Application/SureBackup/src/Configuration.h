#pragma once
#ifndef _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#endif

#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
#include <fstream>
#include <shlobj.h>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

#include "Strategies/Types.h"

enum class ScheduleFrequency { Manual, Daily, Weekly };

struct BackupUnit {
  std::wstring name;
  std::wstring source;
  std::wstring target;
  BackupMode mode = BackupMode::Copy; // Default to Copy
  // Deprecated syncMode bool, replaced by mode enum logic below.
  // bool syncMode = false;
  bool verify = false;
  ErrorPolicy errorPolicy = ErrorPolicy::Continue;
  bool parallelMode = false;
  bool blockCloneMode = false; // Delta Block Strategy
  bool shadowCopyMode = false; // VSS Shadow Copy Strategy
  bool comparisonMode = false; // Brand new Comparing Engine
  bool criteriaSize = true;
  bool criteriaTime = true;
  bool criteriaData = false;
};

struct BackupSet {
  std::wstring name;
  std::wstring description;
  std::vector<BackupUnit> units;

  ScheduleFrequency scheduleFreq = ScheduleFrequency::Manual;
  int scheduleHour = 0;
  int scheduleMinute = 0;
  int scheduleDayOfWeek = 0;     // 0=Sun, 1=Mon,...
  std::wstring lastScheduledRun; // To prevent multiple runs in same slot
};

class ConfigManager {
public:
  static std::wstring GetConfigPath() {
    PWSTR pszPath = NULL;
    if (SUCCEEDED(
            SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &pszPath))) {
      fs::path p(pszPath);
      CoTaskMemFree(pszPath);
      fs::path configDir = p / L"SureBackup";
      if (!fs::exists(configDir)) {
        fs::create_directories(configDir);
      }
      return (configDir / L"config.txt").wstring();
    }
    return L"config.txt";
  }

  static std::vector<BackupSet> Load() {
    std::vector<BackupSet> sets;
    std::wifstream fin(GetConfigPath());
    if (!fin.is_open()) {
      BackupSet defaultSet;
      defaultSet.name = L"Default Backup Set";
      defaultSet.description = L"New backup set description";
      defaultSet.units.push_back({L"My Documents",
                                  L"C:\\Users\\Example\\Documents",
                                  L"D:\\Backups\\Documents", BackupMode::Copy,
                                  false, ErrorPolicy::Continue});
      sets.push_back(defaultSet);
      return sets;
    }

    std::wstring line;
    BackupSet *currentSet = nullptr;
    while (std::getline(fin, line)) {
      if (line.empty())
        continue;
      if (line[0] == L'[') {
        BackupSet s;
        // Format: [Name|Description|Frequency|Hour|Minute|DayOfWeek|LastRun]
        std::wstring content = line.substr(1, line.find(L']') - 1);
        std::wstringstream ss(content);
        std::wstring part;
        int i = 0;
        while (std::getline(ss, part, L'|')) {
          if (i == 0)
            s.name = part;
          else if (i == 1)
            s.description = part;
          else if (i == 2)
            s.scheduleFreq = (ScheduleFrequency)_wtoi(part.c_str());
          else if (i == 3)
            s.scheduleHour = _wtoi(part.c_str());
          else if (i == 4)
            s.scheduleMinute = _wtoi(part.c_str());
          else if (i == 5)
            s.scheduleDayOfWeek = _wtoi(part.c_str());
          else if (i == 6)
            s.lastScheduledRun = part;
          i++;
        }
        sets.push_back(s);
        currentSet = &sets.back();
      } else if (currentSet && line.find(L'|') != std::wstring::npos) {
        std::wstringstream ss(line);
        std::wstring name, src, dst, mode, v, policy, threaded;
        std::getline(ss, name, L'|');
        std::getline(ss, src, L'|');
        std::getline(ss, dst, L'|');
        std::getline(ss, mode, L'|');
        std::getline(ss, v, L'|');
        std::getline(ss, policy, L'|');
        std::getline(ss, threaded, L'|');

        std::wstring p_size, p_time, p_data;
        std::getline(ss, p_size, L'|');
        std::getline(ss, p_time, L'|');
        std::getline(ss, p_data, L'|');

        BackupMode modeEnum = BackupMode::Copy;
        if (mode == L"SYNC")
          modeEnum = BackupMode::Sync;
        else if (mode == L"VERIFY")
          modeEnum = BackupMode::Verify;

        bool verify = (v == L"VERIFY");
        ErrorPolicy ep = (policy == L"SUSPEND") ? ErrorPolicy::Suspend
                                                : ErrorPolicy::Continue;

        bool parallel = (threaded == L"PARALLEL");
        bool blockClone = (threaded == L"BLOCK");
        bool shadowCopy = (threaded == L"VSS");
        bool comparing = (threaded == L"COMPARE");

        BackupUnit unit;
        unit.name = name;
        unit.source = src;
        unit.target = dst;
        unit.mode = modeEnum;
        unit.verify = verify;
        unit.errorPolicy = ep;
        unit.parallelMode = parallel;
        unit.blockCloneMode = blockClone;
        unit.shadowCopyMode = shadowCopy;
        unit.comparisonMode = comparing;
        unit.criteriaSize = (p_size == L"1");
        unit.criteriaTime = (p_time == L"1");
        unit.criteriaData = (p_data == L"1");

        currentSet->units.push_back(unit);
      }
    }
    return sets;
  }

  static void Save(const std::vector<BackupSet> &sets) {
    std::wofstream fout(GetConfigPath());
    for (const auto &s : sets) {
      fout << L"[" << s.name << L"|" << s.description << L"|"
           << (int)s.scheduleFreq << L"|" << s.scheduleHour << L"|"
           << s.scheduleMinute << L"|" << s.scheduleDayOfWeek << L"|"
           << s.lastScheduledRun << L"]" << std::endl;
      for (const auto &u : s.units) {
        std::wstring modeStr = L"COPY";
        if (u.mode == BackupMode::Sync)
          modeStr = L"SYNC";
        else if (u.mode == BackupMode::Verify)
          modeStr = L"VERIFY";

        fout << u.name << L"|" << u.source << L"|" << u.target << L"|"
             << modeStr << L"|" << (u.verify ? L"VERIFY" : L"NO_VERIFY") << L"|"
             << (u.errorPolicy == ErrorPolicy::Suspend ? L"SUSPEND"
                                                       : L"CONTINUE")
             << L"|"
             << (u.blockCloneMode
                     ? L"BLOCK"
                     : (u.shadowCopyMode
                            ? L"VSS"
                            : (u.comparisonMode
                                   ? L"COMPARE"
                                   : (u.parallelMode ? L"PARALLEL"
                                                     : L"STANDARD"))))
             << L"|" << (u.criteriaSize ? L"1" : L"0") << L"|"
             << (u.criteriaTime ? L"1" : L"0") << L"|"
             << (u.criteriaData ? L"1" : L"0") << std::endl;
      }
      fout << std::endl;
    }
  }
};
