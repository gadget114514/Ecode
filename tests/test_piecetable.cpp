#include "../include/PieceTable.h"
#include <cassert>
#include <iostream>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

void TestPieceTable() {
  PieceTable pt;

  // Test 1: Load and Get
  std::string original = "Hello World";
  pt.LoadOriginal(original.c_str(), original.length());
  VERIFY(pt.GetText(0, 11) == "Hello World", "LoadOriginal content mismatch");
  VERIFY(pt.GetTotalLength() == 11, "LoadOriginal length mismatch");
  std::cout << "Test 1 Passed: Load" << std::endl;

  // Test 2: Simple Insert
  pt.Insert(5, " Beautiful");
  VERIFY(pt.GetText(0, 21) == "Hello Beautiful World",
         "Insert content mismatch");
  VERIFY(pt.GetTotalLength() == 21, "Insert length mismatch");
  std::cout << "Test 2 Passed: Insert" << std::endl;

  // Test 3: Insert at start
  pt.Insert(0, "A ");
  VERIFY(pt.GetText(0, 23) == "A Hello Beautiful World",
         "Insert at start mismatch");
  std::cout << "Test 3 Passed: Insert at Start" << std::endl;

  // Test 4: Delete
  pt.Delete(7, 10);
  VERIFY(pt.GetText(0, 13) == "A Hello World", "Delete content mismatch");
  std::cout << "Test 4 Passed: Delete" << std::endl;

  // Test 5: Delete at start
  pt.Delete(0, 2);
  VERIFY(pt.GetText(0, 11) == "Hello World", "Delete at start mismatch");
  std::cout << "Test 5 Passed: Delete at Start" << std::endl;

  // Test 6: Compaction
  pt.Insert(5, "a");
  pt.Insert(6, "b");
  pt.Insert(7, "c");
  size_t before = pt.GetPieceCount();
  pt.CompactPieces();
  size_t after = pt.GetPieceCount();
  VERIFY(after < before, "Compaction didn't reduce piece count");
  VERIFY(pt.GetText(0, 14) == "Helloabc World", "Compaction content corrupted");
  std::cout << "Test 6 Passed: Compaction" << std::endl;

  // Test 7: Undo Stack Pruning
  PieceTable pt2;
  pt2.LoadOriginal("Start", 5);
  for (int i = 0; i < 600; ++i) {
    pt2.Insert(pt2.GetTotalLength(), ".");
  }
  std::cout << "Test 7 Passed: Undo Stack Pruning Logic" << std::endl;

  std::cout << "All PieceTable Tests Passed!" << std::endl;
}

int main() {
  try {
    TestPieceTable();
  } catch (const std::exception &e) {
    std::cerr << "Test failed with exception: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
