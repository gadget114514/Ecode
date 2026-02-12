#include "../include/PieceTable.h"
#include <cassert>
#include <iostream>

void TestPieceTable() {
  PieceTable pt;

  // Test 1: Load and Get
  std::string original = "Hello World";
  pt.LoadOriginal(original.c_str(), original.length());
  assert(pt.GetText(0, 11) == "Hello World");
  assert(pt.GetTotalLength() == 11);
  std::cout << "Test 1 Passed: Load" << std::endl;

  // Test 2: Simple Insert
  pt.Insert(5, " Beautiful");
  assert(pt.GetText(0, 21) == "Hello Beautiful World");
  assert(pt.GetTotalLength() == 21);
  std::cout << "Test 2 Passed: Insert" << std::endl;

  // Test 3: Insert at start
  pt.Insert(0, "A ");
  assert(pt.GetText(0, 23) == "A Hello Beautiful World");
  std::cout << "Test 3 Passed: Insert at Start" << std::endl;

  // Test 4: Delete
  // "A Hello Beautiful World"
  // Delete " Beautiful" (starts at index 7, length 10)
  pt.Delete(7, 10);
  assert(pt.GetText(0, 13) == "A Hello World");
  std::cout << "Test 4 Passed: Delete" << std::endl;

  // Test 5: Delete at start
  pt.Delete(0, 2);
  assert(pt.GetText(0, 11) == "Hello World");
  std::cout << "Test 5 Passed: Delete at Start" << std::endl;

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
