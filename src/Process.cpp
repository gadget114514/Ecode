#include "../include/Process.h"

Process::Process()
    : m_hProcess(NULL), m_hInWrite(NULL), m_hOutRead(NULL), m_hThread(NULL),
      m_running(false) {}

Process::~Process() { Stop(); }

DWORD WINAPI Process::ReadThreadProc(LPVOID lpParam) {
  Process *self = (Process *)lpParam;
  char buffer[4096];
  DWORD dwRead;
  while (self->m_running) {
    if (ReadFile(self->m_hOutRead, buffer, sizeof(buffer) - 1, &dwRead, NULL) &&
        dwRead > 0) {
      if (self->m_onOutput) {
        self->m_onOutput(std::string(buffer, dwRead));
      }
    } else {
      break;
    }
  }
  self->m_running = false;
  return 0;
}

bool Process::Start(const std::wstring &cmd,
                    std::function<void(const std::string &)> onOutput) {
  if (m_running)
    return false;

  m_onOutput = onOutput;

  SECURITY_ATTRIBUTES saAttr;
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  HANDLE hOutWrite = NULL;
  if (!CreatePipe(&m_hOutRead, &hOutWrite, &saAttr, 0))
    return false;
  if (!SetHandleInformation(m_hOutRead, HANDLE_FLAG_INHERIT, 0)) {
    CloseHandle(m_hOutRead);
    CloseHandle(hOutWrite);
    return false;
  }

  HANDLE hInRead = NULL;
  if (!CreatePipe(&hInRead, &m_hInWrite, &saAttr, 0)) {
    CloseHandle(m_hOutRead);
    CloseHandle(hOutWrite);
    return false;
  }
  if (!SetHandleInformation(m_hInWrite, HANDLE_FLAG_INHERIT, 0)) {
    CloseHandle(m_hOutRead);
    CloseHandle(hOutWrite);
    CloseHandle(hInRead);
    CloseHandle(m_hInWrite);
    return false;
  }

  STARTUPINFOW siStartInfo;
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
  siStartInfo.cb = sizeof(STARTUPINFOW);
  siStartInfo.hStdError = hOutWrite;
  siStartInfo.hStdOutput = hOutWrite;
  siStartInfo.hStdInput = hInRead;
  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

  PROCESS_INFORMATION piProcInfo;
  ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

  std::vector<wchar_t> cmdLine(cmd.begin(), cmd.end());
  cmdLine.push_back(0);

  if (!CreateProcessW(NULL, cmdLine.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW,
                      NULL, NULL, &siStartInfo, &piProcInfo)) {
    CloseHandle(m_hOutRead);
    CloseHandle(hOutWrite);
    CloseHandle(hInRead);
    CloseHandle(m_hInWrite);
    return false;
  }

  CloseHandle(hOutWrite);
  CloseHandle(hInRead);

  m_hProcess = piProcInfo.hProcess;
  CloseHandle(piProcInfo.hThread);

  m_running = true;
  m_hThread = CreateThread(NULL, 0, ReadThreadProc, this, 0, NULL);

  return true;
}

void Process::Write(const std::string &text) {
  if (!m_hInWrite)
    return;
  DWORD dwWritten;
  WriteFile(m_hInWrite, text.c_str(), (DWORD)text.length(), &dwWritten, NULL);
}

void Process::Stop() {
  m_running = false;
  if (m_hProcess) {
    TerminateProcess(m_hProcess, 0);
    CloseHandle(m_hProcess);
    m_hProcess = NULL;
  }
  if (m_hInWrite) {
    CloseHandle(m_hInWrite);
    m_hInWrite = NULL;
  }
  if (m_hOutRead) {
    CloseHandle(m_hOutRead);
    m_hOutRead = NULL;
  }
  if (m_hThread) {
    WaitForSingleObject(m_hThread, 2000);
    CloseHandle(m_hThread);
    m_hThread = NULL;
  }
}
