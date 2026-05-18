#include "../src/Globals.inl"
#include <cassert>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

int main() {
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

  // Verify an app tab was created
  VERIFY(!g_appTabs.empty(), "No grep results tab was created");
  std::cout << "Grep tab created: " << g_appTabs.size() << " app tabs, type=" << g_appTabs.back().type << std::endl;

  // Cleanup
  fs::remove_all(testDir);

  std::cout << "test_find_in_files passed!" << std::endl;
  return 0;
}
