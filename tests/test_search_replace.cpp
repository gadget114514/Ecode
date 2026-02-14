#include "../include/Buffer.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "SEARCH FAILURE at line " << __LINE__ << ": " << msg          \
              << std::endl;                                                    \
    exit(1);                                                                   \
  }

#include <functional>

// Mock SafeSave
bool SafeSave(const std::wstring &path, const std::string &content) {
  return true;
}

bool SafeSaveStreaming(
    const std::wstring &targetPath,
    const std::function<void(std::function<void(const char *, size_t)>)>
        &source) {
  source([](const char *, size_t) {});
  return true;
}

void TestFunctionalSearch() {
  Buffer buf;
  buf.Insert(0, "The quick brown fox jumps over the lazy dog. Fox is brown.");

  // Forward Search
  VERIFY(buf.Find("fox", 0, true, false, true) == 16,
         "Forward search 'fox' failed");
  VERIFY(buf.Find("fox", 17, true, false, true) == std::string::npos,
         "Forward search 'fox' out of range failed");

  // Case Insensitive
  VERIFY(buf.Find("fox", 0, true, false, false) == 16,
         "Case insensitive 'fox' 1 failed");
  VERIFY(buf.Find("Fox", 0, true, false, false) == 16,
         "Case insensitive 'fox' 2 failed");
  VERIFY(buf.Find("Fox", 17, true, false, false) == 45,
         "Case insensitive 'Fox' 3 failed");

  // Backward Search
  VERIFY(buf.Find("brown", 58, false, false, true) == 52,
         "Backward search 'brown' 1 failed");
  VERIFY(buf.Find("brown", 51, false, false, true) == 10,
         "Backward search 'brown' 2 failed");

  // Replace
  buf.Replace(16, 19, "cat"); // fox -> cat
  VERIFY(buf.GetText(0, buf.GetTotalLength()).find("cat") == 16,
         "Replace failed");
  VERIFY(buf.Find("fox", 0) == std::string::npos,
         "Fox should be gone after replace");

  std::cout << "Functional Search/Replace Passed" << std::endl;
}

void TestLargeFileSearch() {
  Buffer buf;
  // Buffer::Find uses 64KB chunks. Let's create a pattern near 64KB.
  const size_t CHUNK_SIZE = 64 * 1024;
  std::string padding(CHUNK_SIZE - 2, 'a');
  buf.Insert(0, padding);
  buf.Insert(buf.GetTotalLength(), "FINDME");

  // Pattern "FINDME" starts at CHUNK_SIZE - 2, so it spans across chunks.
  VERIFY(buf.Find("FINDME", 0) == CHUNK_SIZE - 2, "Cross-chunk search failed");

  // Backward across chunks
  VERIFY(buf.Find("FINDME", buf.GetTotalLength(), false) == CHUNK_SIZE - 2,
         "Cross-chunk backward search failed");

  std::cout << "Large File Search Passed" << std::endl;
}

void RunSearchStressTest(int iterations) {
  Buffer buf;
  std::string reference = "";
  std::mt19937 rng(42);

  std::vector<std::string> patterns = {"fox", "dog", "cat",
                                       "abc", "XYZ", "123"};

  std::cout << "Starting Search/Replace Stress Test (" << iterations
            << " iterations)..." << std::endl;

  for (int i = 0; i < iterations; ++i) {
    int op = rng() % 3;
    if (op == 0) { // Insert random text
      size_t pos = reference.empty() ? 0 : rng() % (reference.length() + 1);
      std::string s = patterns[rng() % patterns.size()] + " ";
      buf.Insert(pos, s);
      reference.insert(pos, s);
    } else if (op == 1 && reference.length() > 0) { // Random Search
      std::string p = patterns[rng() % patterns.size()];
      bool forward = rng() % 2 == 0;
      bool matchCase = rng() % 2 == 0;
      size_t startPos = forward ? (rng() % reference.length())
                                : (rng() % (reference.length() + 1));

      size_t actual = buf.Find(p, startPos, forward, false, matchCase);

      size_t expected = std::string::npos;
      if (matchCase) {
        expected = forward ? reference.find(p, startPos)
                           : reference.rfind(p, startPos);
      } else {
        std::string rLower = reference;
        std::string pLower = p;
        std::transform(rLower.begin(), rLower.end(), rLower.begin(), ::tolower);
        std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
        expected = forward ? rLower.find(pLower, startPos)
                           : rLower.rfind(pLower, startPos);
      }

      if (actual != expected) {
        std::cerr << "SEARCH MISMATCH iteration " << i << std::endl;
        std::cerr << "Pattern: " << p << " Start: " << startPos
                  << " Fwd: " << forward << " Case: " << matchCase << std::endl;
        std::cerr << "Expected: " << expected << " Actual: " << actual
                  << std::endl;
        exit(1);
      }
    } else if (op == 2 && reference.length() > 10) { // Replace
      std::string p = patterns[rng() % patterns.size()];
      size_t pos = buf.Find(p, 0);
      if (pos != std::string::npos) {
        std::string rep = "REPLACED";
        buf.Replace(pos, pos + p.length(), rep);
        reference.replace(pos, p.length(), rep);
      }
    }

    if (i % 500 == 0) {
      VERIFY(buf.GetText(0, buf.GetTotalLength()) == reference,
             "Content out of sync");
    }
  }
  std::cout << "Search/Replace Stress Test Passed!" << std::endl;
}

int main() {
  TestFunctionalSearch();
  TestLargeFileSearch();
  RunSearchStressTest(2000);
  return 0;
}
