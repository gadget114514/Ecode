#include "../include/Process.h"
#include <vector>

enum LogLevel { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };
void DebugLog(const std::string &msg, LogLevel level = LOG_INFO);
std::string GetWin32ErrorString(DWORD errorCode);

Process::Process()
    : m_hProcess(NULL), m_hInWrite(NULL), m_hOutRead(NULL), m_hThread(NULL),
      m_running(false), m_codePage(CP_UTF8) {}

Process::~Process() { Stop(); }

DWORD WINAPI Process::ReadThreadProc(LPVOID lpParam) {
  Process *self = (Process *)lpParam;
  char buffer[4096];
  DWORD dwRead;
  self->m_pendingOutput.clear();
  while (self->m_running) {
    if (ReadFile(self->m_hOutRead, buffer, sizeof(buffer) - 1, &dwRead, NULL) &&
        dwRead > 0) {
      std::string combined = self->m_pendingOutput + std::string(buffer, dwRead);
      self->m_pendingOutput.clear();

      size_t validLen = combined.size();
      if (self->m_codePage == CP_UTF8) {
        // UTF-8: scan from the end to detect an incomplete trailing sequence.
        if (validLen > 0) {
          const unsigned char *u = (const unsigned char *)combined.data();
          size_t pos = validLen;
          int contCount = 0;
          while (pos > 0 && (u[pos - 1] & 0xC0) == 0x80) {
            ++contCount;
            --pos;
          }
          if (pos > 0) {
            unsigned char lead = u[pos - 1];
            if ((lead & 0xE0) == 0xC0) {
              if (contCount < 1) {
                self->m_pendingOutput = combined.substr(pos - 1);
                validLen = pos - 1;
              }
            } else if ((lead & 0xF0) == 0xE0) {
              if (contCount < 2) {
                self->m_pendingOutput = combined.substr(pos - 1);
                validLen = pos - 1;
              }
            } else if ((lead & 0xF8) == 0xF0) {
              if (contCount < 3) {
                self->m_pendingOutput = combined.substr(pos - 1);
                validLen = pos - 1;
              }
            }
          } else if (contCount > 0) {
            if (combined.size() <= 3) {
              self->m_pendingOutput = std::move(combined);
              validLen = 0;
            }
          }
        }
      } else {
        // Non-UTF-8 codepage (e.g. CP_932=Shift_JIS):
        // try-convert with MB_ERR_INVALID_CHARS, truncating from the end
        // until we find a suffix that decodes successfully.
        // For non-UTF-8 DBCS/MBCS, a split trailing character can be at most 1 byte.
        bool decoded = false;
        int needed = MultiByteToWideChar(self->m_codePage, MB_ERR_INVALID_CHARS,
                                         combined.data(), (int)combined.size(), nullptr, 0);
        if (needed > 0) {
          validLen = combined.size();
          decoded = true;
        } else if (combined.size() > 1) {
          needed = MultiByteToWideChar(self->m_codePage, MB_ERR_INVALID_CHARS,
                                       combined.data(), (int)(combined.size() - 1), nullptr, 0);
          if (needed > 0) {
            self->m_pendingOutput = combined.substr(combined.size() - 1);
            validLen = combined.size() - 1;
            decoded = true;
          }
        }
        if (!decoded) {
          // Permanently invalid character or single-byte invalid char.
          // Decode everything, letting the converter handle invalid bytes.
          validLen = combined.size();
        }
      }

      if (validLen > 0 && self->m_onOutput) {
        self->m_onOutput(combined.substr(0, validLen));
      }
    } else {
      break;
    }
  }
  // Flush any remaining pending bytes at the end
  if (!self->m_pendingOutput.empty() && self->m_onOutput) {
    self->m_onOutput(self->m_pendingOutput);
    self->m_pendingOutput.clear();
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
  if (!CreatePipe(&m_hOutRead, &hOutWrite, &saAttr, 0)) {
    DebugLog("Process::Start - CreatePipe (out) failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
    return false;
  }
  if (!SetHandleInformation(m_hOutRead, HANDLE_FLAG_INHERIT, 0)) {
    DebugLog("Process::Start - SetHandleInformation (out) failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
    CloseHandle(m_hOutRead);
    CloseHandle(hOutWrite);
    return false;
  }

  HANDLE hInRead = NULL;
  if (!CreatePipe(&hInRead, &m_hInWrite, &saAttr, 0)) {
    DebugLog("Process::Start - CreatePipe (in) failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
    CloseHandle(m_hOutRead);
    CloseHandle(hOutWrite);
    return false;
  }
  if (!SetHandleInformation(m_hInWrite, HANDLE_FLAG_INHERIT, 0)) {
    DebugLog("Process::Start - SetHandleInformation (in) failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
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
    DebugLog("Process::Start - CreateProcessW failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
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
  if (!m_hThread) {
    DebugLog("Process::Start - CreateThread failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
    m_running = false;
    Stop(); // Cleanup
    return false;
  }

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
