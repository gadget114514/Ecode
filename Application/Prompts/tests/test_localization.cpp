#include "../src/PromptsLocalization.h"
#include <cassert>
#include <iostream>
#include <string>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

void TestDefaultLanguage() {
    PromptsLocalization loc;
    VERIFY(loc.GetCurrentLanguage() == "en", "default language should be English");
    std::wstring name = loc.GetString("AppName");
    VERIFY(name == L"Prompts", "English AppName should be Prompts");
    std::cout << "Test Passed: Default Language (English)" << std::endl;
}

void TestJapanese() {
    PromptsLocalization loc;
    loc.SetLanguage("ja");
    VERIFY(loc.GetCurrentLanguage() == "ja", "language should be ja");
    std::wstring name = loc.GetString("AppName");
    VERIFY(name == L"プロンプト", "Japanese AppName should be プロンプト");
    std::wstring save = loc.GetString("Save");
    VERIFY(save == L"保存", "Japanese Save should be 保存");
    std::wstring run = loc.GetString("RunPipeline");
    VERIFY(run == L"パイプライン実行", "Japanese RunPipeline");
    std::cout << "Test Passed: Japanese" << std::endl;
}

void TestFrench() {
    PromptsLocalization loc;
    loc.SetLanguage("fr");
    std::wstring edit = loc.GetString("Editor");
    VERIFY(edit == L"Éditeur", "French Editor should be Éditeur");
    std::wstring search = loc.GetString("Search");
    VERIFY(search == L"Rechercher", "French Search should be Rechercher");
    std::cout << "Test Passed: French" << std::endl;
}

void TestSpanish() {
    PromptsLocalization loc;
    loc.SetLanguage("es");
    std::wstring open = loc.GetString("Open");
    VERIFY(open == L"Abrir", "Spanish Open should be Abrir");
    std::wstring msg = loc.GetString("Messages");
    VERIFY(msg == L"Mensajes", "Spanish Messages should be Mensajes");
    std::cout << "Test Passed: Spanish" << std::endl;
}

void TestPortuguese() {
    PromptsLocalization loc;
    loc.SetLanguage("pt");
    std::wstring save = loc.GetString("Save");
    VERIFY(save == L"Salvar", "Portuguese Save should be Salvar");
    std::wstring tree = loc.GetString("Tree");
    VERIFY(tree == L"Árvore", "Portuguese Tree should be Árvore");
    std::cout << "Test Passed: Portuguese" << std::endl;
}

void TestGerman() {
    PromptsLocalization loc;
    loc.SetLanguage("de");
    std::wstring open = loc.GetString("Open");
    VERIFY(open == L"Öffnen", "German Open should be Öffnen");
    std::wstring cancel = loc.GetString("Cancel");
    VERIFY(cancel == L"Abbrechen", "German Cancel should be Abbrechen");
    std::cout << "Test Passed: German" << std::endl;
}

void TestEnglishFallback() {
    PromptsLocalization loc;
    loc.SetLanguage("ja");
    std::wstring copied = loc.GetString("Copied");
    VERIFY(copied == L"コピーしました！", "Japanese Copied should exist");

    loc.SetLanguage("fr");
    std::wstring history = loc.GetString("History");
    VERIFY(history == L"Historique", "French History should exist");

    loc.SetLanguage("en");
    std::wstring settings = loc.GetString("Settings");
    VERIFY(settings == L"Settings", "English Settings");
    std::cout << "Test Passed: English Fallback" << std::endl;
}

void TestMissingKey() {
    PromptsLocalization loc;
    loc.SetLanguage("en");
    std::wstring missing = loc.GetString("nonexistent_key_xyz");
    VERIFY(missing.find(L"nonexistent_key_xyz") != std::string::npos,
           "missing key should return bracketed key name");
    std::cout << "Test Passed: Missing Key" << std::endl;
}

void TestSupportedLanguages() {
    auto langs = PromptsLocalization::SupportedLanguages();
    VERIFY(langs.size() == 6, "should have 6 supported languages");
    bool hasJa = false;
    for (auto &l : langs) {
        if (l == "ja") hasJa = true;
    }
    VERIFY(hasJa, "Japanese should be in supported languages");
    std::cout << "Test Passed: Supported Languages" << std::endl;
}

void TestInvalidLanguage() {
    PromptsLocalization loc;
    loc.SetLanguage("ko");
    VERIFY(loc.GetCurrentLanguage() == "en", "invalid language should fall back to English");
    std::wstring name = loc.GetString("AppName");
    VERIFY(name == L"Prompts", "fallback language should produce English string");
    std::cout << "Test Passed: Invalid Language Fallback" << std::endl;
}

void TestAllLanguagesHaveAllKeys() {
    auto langs = PromptsLocalization::SupportedLanguages();
    PromptsLocalization loc;
    loc.SetLanguage("en");
    std::wstring enName = loc.GetString("AppName");

    for (auto &lang : langs) {
        loc.SetLanguage(lang);
        std::wstring name = loc.GetString("AppName");
        VERIFY(!name.empty(), "AppName should not be empty for language " + lang);
    }
    std::cout << "Test Passed: All Languages Have AppName" << std::endl;
}

int main() {
    try {
        TestDefaultLanguage();
        TestJapanese();
        TestFrench();
        TestSpanish();
        TestPortuguese();
        TestGerman();
        TestEnglishFallback();
        TestMissingKey();
        TestSupportedLanguages();
        TestInvalidLanguage();
        TestAllLanguagesHaveAllKeys();
        std::cout << "=== ALL LOCALIZATION TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
