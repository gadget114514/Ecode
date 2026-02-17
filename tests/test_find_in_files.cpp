#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <memory>
#include "../include/Editor.h"
#include "../include/Buffer.h"

namespace fs = std::filesystem;

// Mock globals required by Editor and its components
HWND g_mainHwnd = NULL;
HWND g_statusHwnd = NULL;
HWND g_progressHwnd = NULL;
HWND g_tabHwnd = NULL;
HWND g_minibufferHwnd = NULL;

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

int main() {
    // Setup temporary directory structure
    fs::path testDir = fs::current_path() / "test_grep_root";
    if (fs::exists(testDir)) fs::remove_all(testDir);
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
    
    Buffer* results = editor.GetBufferByName(L"*Find Results*");
    VERIFY(results != nullptr, "Find results buffer was not created");
    
    std::string content = results->GetText(0, results->GetTotalLength());
    std::cout << "Grep results:\n" << content << std::endl;
    
    // Check if expected matches are found
    VERIFY(content.find("file1.txt(1)") != std::string::npos, "file1.txt line 1 missing");
    VERIFY(content.find("file1.txt(3)") != std::string::npos, "file1.txt line 3 missing");
    VERIFY(content.find("file2.txt(2)") != std::string::npos, "file2.txt line 2 missing");
    VERIFY(content.find("Done.") != std::string::npos, "Search did not finish correctly");

    // Cleanup
    fs::remove_all(testDir);
    
    std::cout << "test_find_in_files passed!" << std::endl;
    return 0;
}
