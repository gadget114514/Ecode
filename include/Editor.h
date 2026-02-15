#pragma once

#include "Buffer.h"
#include <functional>
#include <memory>
#include <vector>
#include <windows.h>

extern HWND g_mainHwnd;

class Editor {
public:
  Editor();
  ~Editor();

  void SetProgressCallback(std::function<void(float)> cb) { m_progressCb = cb; }

  size_t OpenFile(const std::wstring &path);
  void NewFile();
  size_t OpenShell(const std::wstring &cmd);
  void CloseBuffer(size_t index);

  void SwitchToBuffer(size_t index);
  Buffer *GetActiveBuffer() const;
  size_t GetActiveBufferIndex() const { return m_activeBufferIndex; }

  void Undo();
  void Redo();

  void Cut(HWND hwnd);
  void Copy(HWND hwnd);
  void Paste(HWND hwnd);

  const std::vector<std::unique_ptr<Buffer>> &GetBuffers() const {
    return m_buffers;
  }

  void LogMessage(const std::string &msg);
  Buffer *GetBufferByName(const std::wstring &name);

private:
  std::function<void(float)> m_progressCb;
  std::vector<std::unique_ptr<Buffer>> m_buffers;
  size_t m_activeBufferIndex;
  Buffer *m_messagesBuffer = nullptr;
};
