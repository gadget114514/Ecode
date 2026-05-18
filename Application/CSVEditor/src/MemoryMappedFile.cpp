#include "MemoryMappedFile.h"
#include <iostream> /* For debug logging if needed later */

CsvMemoryMappedFile::CsvMemoryMappedFile()
    : m_hFile(INVALID_HANDLE_VALUE)
    , m_hMapping(NULL)
    , m_pData(NULL)
    , m_size(0)
{
}

CsvMemoryMappedFile::~CsvMemoryMappedFile()
{
    Close();
}

bool CsvMemoryMappedFile::Open(const std::wstring& filePath)
{
    Close();

    m_hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (m_hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(m_hFile, &fileSize)) {
        Close();
        return false;
    }
    m_size = static_cast<uint64_t>(fileSize.QuadPart);

    if (m_size == 0) {
        // Empty file is valid but has no mapping
        return true;
    }

    m_hMapping = CreateFileMappingW(
        m_hFile,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
    );

    if (m_hMapping == NULL) {
        Close();
        return false;
    }

    m_pData = MapViewOfFile(
        m_hMapping,
        FILE_MAP_READ,
        0,
        0,
        0
    );

    if (m_pData == NULL) {
        Close();
        return false;
    }

    return true;
}

void CsvMemoryMappedFile::Close()
{
    if (m_pData) {
        UnmapViewOfFile(m_pData);
        m_pData = NULL;
    }

    if (m_hMapping) {
        CloseHandle(m_hMapping);
        m_hMapping = NULL;
    }

    if (m_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }

    m_size = 0;
}

const uint8_t* CsvMemoryMappedFile::GetData() const
{
    return static_cast<const uint8_t*>(m_pData);
}

uint64_t CsvMemoryMappedFile::GetSize() const
{
    return m_size;
}

bool CsvMemoryMappedFile::IsValid() const
{
    // Valid if file is open. Data might be null if size is 0.
    return m_hFile != INVALID_HANDLE_VALUE;
}
