#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <shlwapi.h>
#include <windows.h>

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <io.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "BackupEngine.h"
#include "Strategies/StandardBackupStrategy.h"

class ConsoleLogger : public IBackupLogger {
public:
  void Log(const std::wstring &message) override {
    std::wcout << L"[LOG] " << message << std::endl;
  }
  void OnFileAction(const std::wstring &action,
                    const std::wstring &path) override {
    std::wcout << L"[" << action << L"] " << path << std::endl;
  }
  void OnProgress(const std::wstring & /*path*/) override {}
  void OnProgressDetailed(const TaskProgress & /*progress*/) override {
    // Optionally log progress details
  }
  void OnWorkerProgress(int /*workerId*/, const std::wstring & /*file*/,
                        int /*percent*/) override {}
};

void RunTest(IBackupStrategy *strategy, const std::wstring &name) {
  namespace fs = std::filesystem;
  std::wcout << L"\n--- Testing Strategy: " << name << L" ---" << std::endl;

  fs::path source = fs::current_path() / L"console_test_source";
  fs::path target = fs::current_path() / L"console_test_target";

  fs::create_directories(source);
  fs::create_directories(target);

  {
    std::wofstream f1(source / L"file1.txt");
    f1 << L"Hello1";
    std::wofstream f2(source / L"file2.txt");
    f2 << L"Hello2";
    fs::create_directories(source / L"subfolder");
    std::wofstream f3(source / L"subfolder" / L"nested.txt");
    f3 << L"Nested";
    std::wofstream f4(target / L"extra.txt");
    f4 << L"Delete me";
  }

  BackupTask task;
  task.name = name;
  task.sourcePath = source.wstring();
  task.targetPath = target.wstring();
  task.mode = BackupMode::Sync;
  task.verify = false;

  ConsoleLogger logger;
  BackupEngine engine(&logger);

  std::wcout << L"Executing DRY RUN..." << std::endl;
  strategy->Execute(task, &logger, true);
}

int main() {
  _setmode(_fileno(stdout), _O_U16TEXT);

  std::wcout << L"SureBackup Engine Console Tester" << std::endl;
  std::wcout << L"===============================" << std::endl;

  StandardBackupStrategy stdStrategy;
  RunTest(&stdStrategy, L"Standard Recursive");

  std::wcout << L"\nTests finished. Press Enter." << std::endl;
  std::cin.get();
  return 0;
}
