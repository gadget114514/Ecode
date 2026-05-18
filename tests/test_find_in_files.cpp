#include "../src/Globals.inl"
#include <cassert>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

// Simple window proc to handle grep result messages
LRESULT CALLBACK TestWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_GREP_RESULT) {
    GrepSearchResult *batch = (GrepSearchResult*)lp;
    if (batch) delete batch;
    return 0;
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}

int main() {
  // Register a minimal window class and create a window so PostMessage works
  WNDCLASSEX wc = {sizeof(WNDCLASSEX)};
  wc.lpfnWndProc = TestWndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = L"TestGrepWindow";
  RegisterClassEx(&wc);

  HWND hwnd = CreateWindowEx(0, L"TestGrepWindow", L"", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                             NULL, NULL, wc.hInstance, NULL);
  g_mainHwnd = hwnd;

  // Setup temporary directory structure
  fs::path testDir = fs::current_path() / "test_grep_root";
  if (fs::exists(testDir))
    fs::remove_all(testDir);
  fs::create_directories(testDir / "sub");

  {
    std::ofstream f(testDir / "file1.txt");
    f << "This is a match\nOther line\nAnother match here\n";
  }
  {
    std::ofstream f(testDir / "sub" / "file2.txt");
    f << "No matches in this one\nExcept maybe here? match!\n";
  }

  Editor editor;
  editor.FindInFiles(testDir.wstring(), L"match");

  // Run message loop until search completes (async)
  while (g_grepSearchActive) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    Sleep(10);
  }
  // Process any remaining messages (WM_GREP_COMPLETE)
  MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Verify results in the app tab
  VERIFY(!g_appTabs.empty(), "No grep results tab was created");
  VERIFY(g_appTabs.back().type == 1, "Grep results tab type mismatch");

  GrepResultData *data = (GrepResultData*)g_appTabs.back().data;
  VERIFY(data != nullptr, "Grep result data was not created");

  std::wcout << L"Grep results: " << data->files.size() << L" matches" << std::endl;
  for (size_t i = 0; i < data->files.size(); i++) {
    std::wcout << data->files[i] << L"(" << data->lines[i] << L"): "
               << data->contents[i] << std::endl;
  }

  VERIFY(data->files.size() >= 3, "Expected at least 3 matches");

  // Cleanup
  DestroyWindow(hwnd);
  UnregisterClass(L"TestGrepWindow", wc.hInstance);
  fs::remove_all(testDir);

  std::cout << "test_find_in_files passed!" << std::endl;
  return 0;
}
