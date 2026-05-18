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
    std::string *text = (std::string*)lp;
    if (text) {
      Buffer *buf = g_editor ? g_editor->GetBufferByName(L"*Find Results*") : nullptr;
      if (buf) {
        buf->Insert(buf->GetTotalLength(), *text);
      }
      delete text;
    }
    return 0;
  }
  if (msg == WM_GREP_COMPLETE) {
    int totalMatches = (int)wp;
    g_grepSearchActive = false;
    Buffer *buf = g_editor ? g_editor->GetBufferByName(L"*Find Results*") : nullptr;
    if (buf) {
      std::string done = "\n--- Done. " + std::to_string(totalMatches) + " matches found. ---\n";
      buf->Insert(buf->GetTotalLength(), done);
    }
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
  g_editor = &editor;
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

  // Verify results in the *Find Results* buffer
  Buffer *results = editor.GetBufferByName(L"*Find Results*");
  VERIFY(results != nullptr, "Find results buffer was not created");

  std::string content = results->GetText(0, results->GetTotalLength());
  std::cout << "Grep results:\n" << content << std::endl;

  VERIFY(content.find("file1.txt(1)") != std::string::npos,
         "file1.txt line 1 missing");
  VERIFY(content.find("file1.txt(3)") != std::string::npos,
         "file1.txt line 3 missing");
  VERIFY(content.find("file2.txt(2)") != std::string::npos,
         "file2.txt line 2 missing");
  VERIFY(content.find("Done.") != std::string::npos,
         "Search did not finish correctly");

  // Cleanup
  DestroyWindow(hwnd);
  UnregisterClass(L"TestGrepWindow", wc.hInstance);
  fs::remove_all(testDir);

  std::cout << "test_find_in_files passed!" << std::endl;
  return 0;
}
