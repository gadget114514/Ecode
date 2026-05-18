#include "../include/Localization.h"
#include <cassert>
#include <iostream>
#include <string>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "FAILURE at line " << __LINE__ << ": " << msg << std::endl;   \
    exit(1);                                                                   \
  }

void TestDefaultLanguage() {
  Localization &loc = Localization::Instance();
  VERIFY(loc.GetCurrentLanguage() == Language::English,
         "Default language should be English");

  std::wstring title = loc.GetString("title");
  VERIFY(title.find(L"Ecode") != std::string::npos,
         "English title should contain Ecode");
  std::wstring file = loc.GetString("menu_file");
  VERIFY(file.find(L"File") != std::string::npos,
         "English menu_file should contain File");

  std::wstring locale = loc.GetLocaleName();
  VERIFY(locale == L"en-US", "English locale should be en-US");

  std::cout << "Test Passed: Default Language (English)" << std::endl;
}

void TestJapanese() {
  Localization &loc = Localization::Instance();
  loc.SetLanguage(Language::Japanese);
  VERIFY(loc.GetCurrentLanguage() == Language::Japanese,
         "Language should be Japanese");

  std::wstring title = loc.GetString("title");
  VERIFY(title.find(L"テキストエディタ") != std::string::npos,
         "Japanese title should contain テキストエディタ");

  std::wstring locale = loc.GetLocaleName();
  VERIFY(locale == L"ja-JP", "Japanese locale should be ja-JP");

  std::wstring kb = loc.GetString("menu_help_keybindings");
  VERIFY(kb.find(L"キーバインド") != std::string::npos,
         "Japanese keybindings string should be translated");

  std::cout << "Test Passed: Japanese" << std::endl;
}

void TestSpanish() {
  Localization &loc = Localization::Instance();
  loc.SetLanguage(Language::Spanish);
  VERIFY(loc.GetCurrentLanguage() == Language::Spanish,
         "Language should be Spanish");

  std::wstring file = loc.GetString("menu_file");
  VERIFY(file.find(L"Archivo") != std::string::npos,
         "Spanish menu_file should be Archivo");

  std::wstring locale = loc.GetLocaleName();
  VERIFY(locale == L"es-ES", "Spanish locale should be es-ES");

  std::cout << "Test Passed: Spanish" << std::endl;
}

void TestFrench() {
  Localization &loc = Localization::Instance();
  loc.SetLanguage(Language::French);

  std::wstring file = loc.GetString("menu_file");
  VERIFY(file.find(L"Fichier") != std::string::npos,
         "French menu_file should be Fichier");

  std::wstring locale = loc.GetLocaleName();
  VERIFY(locale == L"fr-FR", "French locale should be fr-FR");

  std::cout << "Test Passed: French" << std::endl;
}

void TestGerman() {
  Localization &loc = Localization::Instance();
  loc.SetLanguage(Language::German);

  std::wstring file = loc.GetString("menu_file");
  VERIFY(file.find(L"Datei") != std::string::npos,
         "German menu_file should be Datei");

  std::wstring locale = loc.GetLocaleName();
  VERIFY(locale == L"de-DE", "German locale should be de-DE");

  std::cout << "Test Passed: German" << std::endl;
}

void TestEnglishFallback() {
  Localization &loc = Localization::Instance();
  loc.SetLanguage(Language::Japanese);

  std::wstring missing = loc.GetString("menu_buffers");
  VERIFY(!missing.empty(), "Japanese should have menu_buffers");

  loc.SetLanguage(Language::Spanish);

  std::wstring vs = loc.GetString("menu_view_toggle_ui");
  VERIFY(!vs.empty(), "Fallback for missing Spanish key should not be empty");
  VERIFY(vs.find(L"Toggle") != std::string::npos,
         "Spanish fallback for menu_view_toggle_ui should return English");

  loc.SetLanguage(Language::German);

  std::wstring zi = loc.GetString("menu_view_zoom_in");
  VERIFY(!zi.empty(),
         "Fallback for missing German key should not be empty");

  std::cout << "Test Passed: English Fallback" << std::endl;
}

void TestMissingKey() {
  Localization &loc = Localization::Instance();
  loc.SetLanguage(Language::English);

  std::wstring result = loc.GetString("nonexistent_key_xyz");
  VERIFY(result.find(L"nonexistent_key_xyz") != std::string::npos,
         "Missing key should return the key string itself");

  std::cout << "Test Passed: Missing Key Returns Key" << std::endl;
}

int main() {
  try {
    TestDefaultLanguage();
    TestJapanese();
    TestSpanish();
    TestFrench();
    TestGerman();
    TestEnglishFallback();
    TestMissingKey();
    std::cout << "=== ALL LOCALIZATION TESTS PASSED ===" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test suite failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
