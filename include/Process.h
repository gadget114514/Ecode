#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <windows.h>


class Process {
public:
  Process();
  ~Process();

  bool Start(const std::wstring &cmd,
             std::function<void(const std::string &)> onOutput);
  void Write(const std::string &text);
  void Stop();
  bool IsRunning() const { return m_running; }

private:
  static DWORD WINAPI ReadThreadProc(LPVOID lpParam);

  HANDLE m_hProcess;
  HANDLE m_hInWrite;
  HANDLE m_hOutRead;
  HANDLE m_hThread;
  std::function<void(const std::string &)> m_onOutput;
  std::atomic<bool> m_running;
};
