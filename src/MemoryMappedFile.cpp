#include "../include/MemoryMappedFile.h"
#include <iostream>

MemoryMappedFile::MemoryMappedFile()
    : m_fileHandle(INVALID_HANDLE_VALUE), m_mappingHandle(NULL),
      m_mappedView(nullptr), m_fileSize(0) {}

MemoryMappedFile::~MemoryMappedFile() { Close(); }

static constexpr size_t kReadThreshold = 16 * 1024 * 1024; // 16MB

bool MemoryMappedFile::Open(const std::wstring &filePath) {
  Close();

  m_fileHandle = CreateFileW(filePath.c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
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
    CloseHandle(m_fileHandle);
    m_fileHandle = INVALID_HANDLE_VALUE;
    return true;
  }

  if (m_fileSize <= kReadThreshold) {
    m_buffer.resize(m_fileSize);
    DWORD bytesRead = 0;
    if (!ReadFile(m_fileHandle, &m_buffer[0],
                  static_cast<DWORD>(m_fileSize), &bytesRead, NULL)) {
      Close();
      return false;
    }
    CloseHandle(m_fileHandle);
    m_fileHandle = INVALID_HANDLE_VALUE;
    m_mappedView = &m_buffer[0];
    return true;
  }

  m_mappingHandle =
      CreateFileMappingW(m_fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
  if (m_mappingHandle == NULL) {
    Close();
    return false;
  }

  CloseHandle(m_fileHandle);
  m_fileHandle = INVALID_HANDLE_VALUE;

  m_mappedView = MapViewOfFile(m_mappingHandle, FILE_MAP_READ, 0, 0, 0);
  if (m_mappedView == nullptr) {
    Close();
    return false;
  }

  return true;
}

void MemoryMappedFile::Close() {
  if (m_mappingHandle) {
    if (m_mappedView) {
      UnmapViewOfFile(m_mappedView);
    }
    CloseHandle(m_mappingHandle);
    m_mappingHandle = NULL;
  }
  if (m_fileHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(m_fileHandle);
    m_fileHandle = INVALID_HANDLE_VALUE;
  }
  m_mappedView = nullptr;
  m_fileSize = 0;
  m_buffer.clear();
  m_buffer.shrink_to_fit();
}

const char *MemoryMappedFile::GetData() const {
  return static_cast<const char *>(m_mappedView);
}

size_t MemoryMappedFile::GetSize() const { return m_fileSize; }

bool MemoryMappedFile::IsOpen() const {
  return m_mappedView != nullptr;
}
