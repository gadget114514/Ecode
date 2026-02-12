#pragma once

#include <map>
#include <string>
#include <vector>

enum class Language { English, Japanese, Spanish, French, German };

class Localization {
public:
  static Localization &Instance();

  void SetLanguage(Language lang);
  Language GetCurrentLanguage() const { return m_currentLanguage; }
  std::wstring GetLocaleName() const;

  std::wstring GetString(const std::string &key) const;

private:
  Localization();
  Language m_currentLanguage;

  std::map<Language, std::map<std::string, std::wstring>> m_translations;

  void LoadTranslations();
};

#define L10N(key) Localization::Instance().GetString(key).c_str()
