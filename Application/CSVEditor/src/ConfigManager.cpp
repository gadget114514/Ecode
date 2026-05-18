#include "ConfigManager.h"
#include <shlwapi.h>
#include <cstdio>

#pragma comment(lib, "Shlwapi.lib")

CsvConfigManager& CsvConfigManager::Instance() {
    static CsvConfigManager instance;
    return instance;
}

CsvConfigManager::CsvConfigManager() {
    m_configPath = GetConfigPath();
}

std::wstring CsvConfigManager::GetConfigPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, 0, path))) {
        std::wstring configDir = std::wstring(path) + L"\\CSVEditor";
        CreateDirectory(configDir.c_str(), NULL);
        return configDir + L"\\config.ini";
    }
    return L"config.ini"; // Fallback to current dir
}

void CsvConfigManager::Load() {
    // Nothing to do explicitly, GetPrivateProfileString reads from file on demand or cached by OS
}

void CsvConfigManager::Save() {
    // Nothing to do explicitly, WritePrivateProfileString writes to file
}

std::wstring CsvConfigManager::GetString(const std::wstring& section, const std::wstring& key, const std::wstring& defaultVal) {
    wchar_t buf[1024];
    GetPrivateProfileString(section.c_str(), key.c_str(), defaultVal.c_str(), buf, 1024, m_configPath.c_str());
    return buf;
}

int CsvConfigManager::GetInt(const std::wstring& section, const std::wstring& key, int defaultVal) {
    return GetPrivateProfileInt(section.c_str(), key.c_str(), defaultVal, m_configPath.c_str());
}

float CsvConfigManager::GetFloat(const std::wstring& section, const std::wstring& key, float defaultVal) {
    std::wstring val = GetString(section, key, L"");
    if (val.empty()) return defaultVal;
    try {
        return std::stof(val);
    } catch (...) {
        return defaultVal;
    }
}

void CsvConfigManager::SetString(const std::wstring& section, const std::wstring& key, const std::wstring& val) {
    WritePrivateProfileString(section.c_str(), key.c_str(), val.c_str(), m_configPath.c_str());
}

void CsvConfigManager::SetInt(const std::wstring& section, const std::wstring& key, int val) {
    SetString(section, key, std::to_wstring(val));
}

void CsvConfigManager::SetFloat(const std::wstring& section, const std::wstring& key, float val) {
    SetString(section, key, std::to_wstring(val));
}
