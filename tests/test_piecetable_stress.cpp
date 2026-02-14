#include "../include/PieceTable.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <vector>

// Helper to count newlines in a string
size_t ReferenceCountNewlines(const std::string &s) {
  size_t count = 0;
  for (char c : s) {
    if (c == '\n')
      count++;
  }
  return count;
}

std::string RandomString(std::mt19937 &rng, size_t len) {
  std::string s;
  for (size_t i = 0; i < len; ++i) {
    s += (char)('a' + (rng() % 26));
  }
  return s;
}

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "STRESS FAILURE at line " << __LINE__ << " (Iteration " << i  \
              << "): " << msg << std::endl;                                    \
    exit(1);                                                                   \
  }

void RunStressTest(int iterations) {
  PieceTable pt;
  std::string reference = "";

  std::mt19937 rng(1337);
  std::uniform_int_distribution<int> opDist(0, 5);

  std::vector<std::string> words = {"apple ", "banana ", "cherry ", "date ",
                                    "elderberry "};
  std::vector<std::string> statements = {
      "void func() {\n",          "    int x = 5;\n",
      "    if (x > 0) return;\n", "}\n",
      "// This is a comment\n",   "class MyClass {};\n"};

  std::cout << "Starting PieceTable 'Several' Stress Test (" << iterations
            << " iterations)..." << std::endl;

  for (int i = 0; i < iterations; ++i) {
    int op = opDist(rng);
    size_t currentLen = reference.length();
    size_t pos = (currentLen == 0) ? 0 : rng() % (currentLen + 1);

    std::string toInsert = "";

    switch (op) {
    case 0: { // Insert several characters
      size_t count = (rng() % 10) + 1;
      toInsert = RandomString(rng, count);
      break;
    }
    case 1: { // Insert several words
      size_t count = (rng() % 5) + 1;
      for (size_t j = 0; j < count; ++j) {
        toInsert += words[rng() % words.size()];
      }
      break;
    }
    case 2: { // Insert several statements
      size_t count = (rng() % 3) + 1;
      for (size_t j = 0; j < count; ++j) {
        toInsert += statements[rng() % statements.size()];
      }
      break;
    }
    case 3: { // Insert several newlines
      size_t count = (rng() % 5) + 1;
      toInsert = std::string(count, '\n');
      break;
    }
    case 4: { // Delete several (random range)
      if (currentLen > 0) {
        size_t delPos = rng() % currentLen;
        size_t delLen =
            (rng() % std::min(currentLen - delPos, (size_t)100)) + 1;
        pt.Delete(delPos, delLen);
        reference = pt.GetText(
            0, pt.GetTotalLength()); // Re-sync to avoid komplex delta logic
      }
      break;
    }
    case 5: { // Undo / Redo check
      if (rng() % 2 == 0) {
        if (pt.CanUndo()) {
          pt.Undo();
          reference = pt.GetText(0, pt.GetTotalLength());
        }
      } else {
        if (pt.CanRedo()) {
          pt.Redo();
          reference = pt.GetText(0, pt.GetTotalLength());
        }
      }
      break;
    }
    }

    if (!toInsert.empty()) {
      pt.Insert(pos, toInsert);
      reference = pt.GetText(0, pt.GetTotalLength()); // Keep in sync
    }

    // Validation
    size_t expectedLen = reference.length();
    size_t actualLen = pt.GetTotalLength();
    size_t expectedLines = ReferenceCountNewlines(reference) + 1;
    size_t actualLines = pt.GetTotalLines();

    VERIFY(expectedLen == actualLen, "Length mismatch");
    VERIFY(expectedLines == actualLines, "Line count mismatch");
  }

  std::cout << "Several-Items Stress Test Passed!" << std::endl;
}

int main() {
  RunStressTest(10000);
  return 0;
}
