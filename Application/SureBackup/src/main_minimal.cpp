
// Minimal Repro Step 2
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

// Define version macros explicitly
#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0600

#include <windows.h>

// Check if DWORD is defined
void CheckTypes() {
  DWORD dw = 0;
  (void)dw;
}

// Don't include commctrl.h yet
// #include <commctrl.h>

#include <iostream>

#include "BackupEngine.h"
#include "Strategies/StandardBackupStrategy.h"

// Needed libraries
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(                                                               \
    linker,                                                                    \
    "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Manually define ListView stuff if commctrl.h is busted
#ifndef WC_LISTVIEW
#define WC_LISTVIEW L"SysListView32"
#endif

class WindowLogger : public IBackupLogger {
public:
  void Log(const std::wstring &message) override {
    std::wcout << L"LOG: " << message << std::endl;
  }
  void OnFileAction(const std::wstring &action,
                    const std::wstring &path) override {
    std::wcout << action << L": " << path << std::endl;
  }
};

void DoTest() {
  BackupTask task;
  task.name = L"Dry Run Test";
  task.sourcePath = L"C:\\Source";
  task.targetPath = L"C:\\Target";
  task.mode = BackupMode::Sync;
  task.verify = false;

  WindowLogger logger;
  BackupEngine engine(&logger);
  engine.SetStrategy(std::make_unique<StandardBackupStrategy>());
  engine.Run(task, true);
}

int main() {
  std::cout << "Starting Minimal Test (No CommCtrl header)..." << std::endl;
  DoTest();
  return 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
  main();
  return 0;
}
