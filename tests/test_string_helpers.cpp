#include "../include/StringHelpers.h"
#include <cassert>
#include <iostream>
#include <string>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

void TestUtf8ToShiftJis() {
  std::string result = StringHelpers::Utf8ToShiftJis("");
  VERIFY(result.empty(), "Empty string should return empty");

  std::string ascii = "Hello World";
  result = StringHelpers::Utf8ToShiftJis(ascii);
  VERIFY(result == "Hello World", "ASCII round-trip failed");

  std::cout << "Test Passed: Utf8ToShiftJis basic" << std::endl;
}

void TestShiftJisToUtf8() {
  std::string result = StringHelpers::ShiftJisToUtf8("");
  VERIFY(result.empty(), "Empty string should return empty");

  std::string ascii = "Hello World";
  result = StringHelpers::ShiftJisToUtf8(ascii);
  VERIFY(result == "Hello World", "ASCII round-trip failed");

  std::cout << "Test Passed: ShiftJisToUtf8 basic" << std::endl;
}

void TestUtf16ToUtf8() {
  std::string result = StringHelpers::Utf16ToUtf8(L"");
  VERIFY(result.empty(), "Empty wstring should return empty");

  result = StringHelpers::Utf16ToUtf8(L"Hello World");
  VERIFY(result == "Hello World", "ASCII conversion failed");

  std::wstring wide = L"Hello";
  result = StringHelpers::Utf16ToUtf8(wide);
  VERIFY(result == "Hello", "Basic UTF-16 to UTF-8 failed");

  std::cout << "Test Passed: Utf16ToUtf8" << std::endl;
}

void TestRoundTrip() {
  std::string utf8 = "Hello";
  std::string sjis = StringHelpers::Utf8ToShiftJis(utf8);
  std::string back = StringHelpers::ShiftJisToUtf8(sjis);
  VERIFY(back == utf8, "ASCII round-trip UTF-8 -> ShiftJIS -> UTF-8 failed");

  std::cout << "Test Passed: Round-trip conversion" << std::endl;
}

int main() {
  try {
    TestUtf8ToShiftJis();
    TestShiftJisToUtf8();
    TestUtf16ToUtf8();
    TestRoundTrip();
    std::cout << "=== ALL STRING HELPERS TESTS PASSED ===" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test suite failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
