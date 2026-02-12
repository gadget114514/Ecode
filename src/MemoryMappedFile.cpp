#include "../include/MemoryMappedFile.h"
#include <iostream>

MemoryMappedFile::MemoryMappedFile()
    : m_fileHandle(INVALID_HANDLE_VALUE), m_mappingHandle(NULL),
      m_mappedView(nullptr), m_fileSize(0) {}

MemoryMappedFile::~MemoryMappedFile() { Close(); }

bool MemoryMappedFile::Open(const std::wstring &filePath) {
  Close();

  m_fileHandle = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (m_fileHandle == INVALID_HANDLE_VALUE) {
    return false;
  }

  LARGE_INTEGER size;
  if (!GetFileSizeEx(m_fileHandle, &size)) {
    Close();
    return false;
  }
  m_fileSize = static_cast<size_t>(size.QuadPart);

  if (m_fileSize == 0) {
    // Mapping zero-sized file is not allowed
    return true;
  }

  m_mappingHandle =
      CreateFileMappingW(m_fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
  if (m_mappingHandle == NULL) {
    Close();
    return false;
  }

  m_mappedView = MapViewOfFile(m_mappingHandle, FILE_MAP_READ, 0, 0, 0);
  if (m_mappedView == nullptr) {
    Close();
    return false;
  }

  return true;
}

void MemoryMappedFile::Close() {
  if (m_mappedView) {
    UnmapViewOfFile(m_mappedView);
    m_mappedView = nullptr;
  }
  if (m_mappingHandle) {
    CloseHandle(m_mappingHandle);
    m_mappingHandle = NULL;
  }
  if (m_fileHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(m_fileHandle);
    m_fileHandle = INVALID_HANDLE_VALUE;
  }
  m_fileSize = 0;
}

const char *MemoryMappedFile::GetData() const {
  return static_cast<const char *>(m_mappedView);
}

size_t MemoryMappedFile::GetSize() const { return m_fileSize; }

bool MemoryMappedFile::IsOpen() const {
  return m_fileHandle != INVALID_HANDLE_VALUE;
}
