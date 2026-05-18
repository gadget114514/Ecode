#pragma once
#include <windows.h>
#include <string>
#include <shlobj.h>

class CsvConfigManager {
public:
    static CsvConfigManager& Instance();

    void Load();
    void Save();

    // Generic Getters/Setters
    std::wstring GetString(const std::wstring& section, const std::wstring& key, const std::wstring& defaultVal = L"");
    int GetInt(const std::wstring& section, const std::wstring& key, int defaultVal);
    float GetFloat(const std::wstring& section, const std::wstring& key, float defaultVal);

    void SetString(const std::wstring& section, const std::wstring& key, const std::wstring& val);
    void SetInt(const std::wstring& section, const std::wstring& key, int val);
    void SetFloat(const std::wstring& section, const std::wstring& key, float val);

private:
    CsvConfigManager();
    std::wstring m_configPath;
    
    std::wstring GetConfigPath();
};
