#pragma once

#include <string>
#include <windows.h>

class MemoryMappedFile {
public:
  MemoryMappedFile();
  ~MemoryMappedFile();

  bool Open(const std::wstring &filePath);
  void Close();

  const char *GetData() const;
  size_t GetSize() const;

  bool IsOpen() const;

private:
  HANDLE m_fileHandle;
  HANDLE m_mappingHandle;
  void *m_mappedView;
  size_t m_fileSize;
};
