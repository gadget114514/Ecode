#pragma once

#include <string>
#include <vector>
#include <windows.h>

class StringHelpers {
public:
  static std::string Utf8ToShiftJis(const std::string &utf8) {
    if (utf8.empty())
      return "";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    if (len <= 0)
      return "";
    std::vector<wchar_t> wstr(len);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wstr.data(), len);

    int sjisLen = WideCharToMultiByte(932, 0, wstr.data(), -1, NULL, 0, NULL, NULL);
    if (sjisLen <= 0)
      return "";
    std::vector<char> sjis(sjisLen);
    WideCharToMultiByte(932, 0, wstr.data(), -1, sjis.data(), sjisLen, NULL, NULL);

    return std::string(sjis.data());
  }

  static std::string ShiftJisToUtf8(const std::string &sjis) {
    if (sjis.empty())
      return "";
    int len = MultiByteToWideChar(932, 0, sjis.c_str(), -1, NULL, 0);
    if (len <= 0)
      return "";
    std::vector<wchar_t> wstr(len);
    MultiByteToWideChar(932, 0, sjis.c_str(), -1, wstr.data(), len);

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, NULL, 0, NULL, NULL);
    if (utf8Len <= 0)
      return "";
    std::vector<char> utf8(utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, utf8.data(), utf8Len, NULL, NULL);

    return std::string(utf8.data());
  }

  static std::string Utf16ToUtf8(const std::wstring &wstr) {
    if (wstr.empty())
      return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 0)
      return "";
    std::vector<char> utf8(len);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, utf8.data(), len, NULL, NULL);
    return std::string(utf8.data());
  }
};
