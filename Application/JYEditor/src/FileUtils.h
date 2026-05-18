#pragma once
#include <string>

class JSONEditorFileUtils {
public:
  enum EolMode { CRLF = 0, LF = 1, CR = 2 };

  static std::wstring ReadFileUtf8(const std::wstring &path);
  static bool WriteFileUtf8(const std::wstring &path,
                            const std::wstring &content, EolMode eol);
  static std::wstring NormalizeToCrlf(const std::wstring &text);
};
