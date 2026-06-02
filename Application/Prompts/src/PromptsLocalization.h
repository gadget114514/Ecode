#pragma once
#include <string>
#include <map>
#include <vector>

class PromptsLocalization {
public:
    PromptsLocalization();
    
    void SetLanguage(const std::string &lang);
    const std::string &GetCurrentLanguage() const { return m_currentLang; }
    std::wstring GetString(const std::string &key) const;
    
    static std::vector<std::string> SupportedLanguages();

private:
    std::string m_currentLang = "en";
    const std::map<std::string, std::map<std::string, std::wstring>> &GetTranslations() const;
};
