#pragma once

#include <windows.h>
#include <string>
#include <cstdint>

class CsvMemoryMappedFile {
public:
    CsvMemoryMappedFile();
    ~CsvMemoryMappedFile();

    bool Open(const std::wstring& filePath);
    void Close();

    const uint8_t* GetData() const;
    uint64_t GetSize() const;
    bool IsValid() const;

private:
    HANDLE m_hFile;
    HANDLE m_hMapping;
    void* m_pData;
    uint64_t m_size;
};
