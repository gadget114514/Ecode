#pragma once
#include <map>
#include <string>

class JSONEditorLocalization {
public:
  JSONEditorLocalization();

  void SetLanguage(const std::string &lang);
  std::wstring GetLocalizedString(const std::string &key) const;
  const std::string &GetCurrentLanguage() const { return m_currentLang; }

private:
  std::string m_currentLang;
  static const std::map<std::string, std::map<std::string, std::wstring>> &
  GetTranslations();
};
