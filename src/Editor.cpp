#include "../include/Editor.h"
#include "../include/Process.h"
#include "../include/SettingsManager.h"
#include "../include/StringHelpers.h"

enum LogLevel { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };
void DebugLog(const std::string &msg, LogLevel level = LOG_INFO);
std::string GetWin32ErrorString(DWORD errorCode);

#define WM_SHELL_OUTPUT (WM_USER + 101)

#include <fstream>
#if defined(__has_include) && __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif defined(__has_include) && __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

struct ShellOutput {
  Buffer *buffer;
  std::string text;
};

Editor::Editor() : m_activeBufferIndex(0) {}

Editor::~Editor() {}

static std::wstring StringToWString(const std::string &s) {
  if (s.empty())
    return L"";
  int size_needed =
      MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
  std::wstring wstrTo(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &wstrTo[0],
                      size_needed);
  return wstrTo;
}

size_t Editor::OpenFile(const std::wstring &path) {
  auto buffer = std::make_unique<Buffer>();
  if (m_progressCb)
    buffer->SetProgressCallback(m_progressCb);
  if (buffer->OpenFile(path)) {
    m_buffers.push_back(std::move(buffer));
    m_activeBufferIndex = m_buffers.size() - 1;
    return m_activeBufferIndex;
  }
  return static_cast<size_t>(-1);
}

void Editor::NewFile(const std::string &name) {
  auto buffer = std::make_unique<Buffer>();
  if (m_progressCb)
    buffer->SetProgressCallback(m_progressCb);
  
  // Set the name if provided
  std::wstring wname = StringToWString(name);
  buffer->SetPath(wname);
  
  // New file has empty original and added buffers
  m_buffers.push_back(std::move(buffer));
  m_activeBufferIndex = m_buffers.size() - 1;
}

size_t Editor::OpenShell(const std::wstring &cmd) {
  auto buffer = std::make_unique<Buffer>();
  buffer->SetPath(L"*shell*");
  buffer->SetScratch(true);
  buffer->SetShell(true);

  Buffer *bRaw = buffer.get();
  auto process = std::make_unique<Process>();

  if (process->Start(cmd, [bRaw](const std::string &text) {
        ShellOutput *output = new ShellOutput();
        output->buffer = bRaw;
        
        int enc = SettingsManager::Instance().GetShellEncoding();
        if (enc == 1) { // Shift-JIS
          output->text = StringHelpers::ShiftJisToUtf8(text);
        } else {
          output->text = text;
        }
        
        PostMessage(g_mainHwnd, WM_SHELL_OUTPUT, (WPARAM)output, 0);
      })) {
    buffer->SetShellProcess(std::move(process));
    m_buffers.push_back(std::move(buffer));
    m_activeBufferIndex = m_buffers.size() - 1;
    return m_activeBufferIndex;
  }
  return static_cast<size_t>(-1);
}

size_t Editor::OpenJsShell() {
  auto buffer = std::make_unique<Buffer>();
  buffer->SetPath(L"*Script Console*");
  buffer->SetScratch(true);
  buffer->SetShell(true);
  buffer->SetJsShell(true);
  buffer->Insert(0, "// Ecode Script Console\n// Type JS code and press Enter to evaluate.\n> ");
  buffer->SetInputStart(buffer->GetTotalLength());
  buffer->SetCaretPos(buffer->GetTotalLength());

  m_buffers.push_back(std::move(buffer));
  m_activeBufferIndex = m_buffers.size() - 1;
  return m_activeBufferIndex;
}

void Editor::FindInFiles(const std::wstring &dir, const std::wstring &pattern) {
  // Create or clear *Find Results* buffer
  Buffer *resultsBuf = GetBufferByName(L"*Find Results*");
  if (!resultsBuf) {
    auto buffer = std::make_unique<Buffer>();
    buffer->SetPath(L"*Find Results*");
    buffer->SetScratch(true);
    resultsBuf = buffer.get();
    m_buffers.push_back(std::move(buffer));
  } else {
    resultsBuf->Delete(0, resultsBuf->GetTotalLength());
  }
  
  // Switch to results buffer
  for(size_t i=0; i<m_buffers.size(); ++i) {
      if(m_buffers[i].get() == resultsBuf) {
          SwitchToBuffer(i);
          break;
      }
  }

  std::string patternUtf8 = StringHelpers::Utf16ToUtf8(pattern);
  std::string dirUtf8 = StringHelpers::Utf16ToUtf8(dir);
  
  resultsBuf->Insert(0, "Searching for \"" + patternUtf8 + "\" in " + dirUtf8 + "...\n");
  
  std::wstring searchDir = dir;
  
  // Simple recursive search (blocking for now)
  try {
      if (fs::exists(searchDir) && fs::is_directory(searchDir)) {
          for (const auto& entry : fs::recursive_directory_iterator(searchDir)) {
              if (entry.is_regular_file()) {
                  std::ifstream file(entry.path());
                  if (file) {
                      std::string line;
                      int lineNum = 0;
                      while (std::getline(file, line)) {
                          lineNum++;
                          if (line.find(patternUtf8) != std::string::npos) {
                              std::string out = entry.path().string() + "(" + std::to_string(lineNum) + "): " + line + "\n";
                              resultsBuf->Insert(resultsBuf->GetTotalLength(), out);
                          }
                      }
                  }
              }
          }
      }
  } catch (const std::exception& e) {
      DebugLog("Editor::FindInFiles - Exception: " + std::string(e.what()), LOG_ERROR);
      resultsBuf->Insert(resultsBuf->GetTotalLength(), "Error during search: " + std::string(e.what()) + "\n");
  } catch (...) {
      DebugLog("Editor::FindInFiles - Unknown error", LOG_ERROR);
      resultsBuf->Insert(resultsBuf->GetTotalLength(), "Error during search.\n");
  }
  resultsBuf->Insert(resultsBuf->GetTotalLength(), "Done.\n");
}

void Editor::CloseBuffer(size_t index) {
  if (index < m_buffers.size()) {
    m_buffers.erase(m_buffers.begin() + index);
    if (m_activeBufferIndex >= m_buffers.size() && !m_buffers.empty()) {
      m_activeBufferIndex = m_buffers.size() - 1;
    } else if (m_buffers.empty()) {
      m_activeBufferIndex = 0;
    }
  }
}

void Editor::SwitchToBuffer(size_t index) {
  if (index < m_buffers.size()) {
    m_activeBufferIndex = index;
  }
}

Buffer *Editor::GetActiveBuffer() const {
  if (m_activeBufferIndex < m_buffers.size()) {
    return m_buffers[m_activeBufferIndex].get();
  }
  return nullptr;
}

void Editor::Undo() {
  Buffer *active = GetActiveBuffer();
  if (active)
    active->Undo();
}

void Editor::Redo() {
  Buffer *active = GetActiveBuffer();
  if (active)
    active->Redo();
}

void Editor::Cut(HWND hwnd) {
  Copy(hwnd);
  Buffer *active = GetActiveBuffer();
  if (active)
    active->DeleteSelection();
}

void Editor::Copy(HWND hwnd) {
  Buffer *active = GetActiveBuffer();
  if (!active || !active->HasSelection())
    return;

  std::string text = active->GetSelectedText();

  if (OpenClipboard(hwnd)) {
    EmptyClipboard();
    int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
    if (hMem) {
      wchar_t *pMem = (wchar_t *)GlobalLock(hMem);
      if (pMem) {
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pMem, len);
        GlobalUnlock(hMem);
        if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
          DebugLog("Editor::Copy - SetClipboardData failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
          GlobalFree(hMem);
        }
      } else {
        DebugLog("Editor::Copy - GlobalLock failed", LOG_ERROR);
        GlobalFree(hMem);
      }
    } else {
      DebugLog("Editor::Copy - GlobalAlloc failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
    }
    CloseClipboard();
  } else {
    DebugLog("Editor::Copy - OpenClipboard failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
  }
}

void Editor::Paste(HWND hwnd) {
  if (OpenClipboard(hwnd)) {
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
      wchar_t *pMem = (wchar_t *)GlobalLock(hData);
      if (pMem) {
        int len =
            WideCharToMultiByte(CP_UTF8, 0, pMem, -1, NULL, 0, NULL, NULL);
        std::vector<char> text(len);
        WideCharToMultiByte(CP_UTF8, 0, pMem, -1, text.data(), len, NULL, NULL);
        GlobalUnlock(hData);

        Buffer *active = GetActiveBuffer();
        if (active) {
          if (active->HasSelection())
            active->DeleteSelection();

          if (active->GetSelectionMode() == SelectionMode::Box) {
            // Block paste logic: split clipboard text by lines and insert each
            // line into sequential lines starting at the caret row/column.
            std::string clipText(text.data());
            std::vector<std::string> lines;
            size_t start = 0, end;
            while ((end = clipText.find('\n', start)) != std::string::npos) {
              std::string line = clipText.substr(start, end - start);
              if (!line.empty() && line.back() == '\r')
                line.pop_back();
              lines.push_back(line);
              start = end + 1;
            }
            lines.push_back(clipText.substr(start));

            size_t startLine = active->GetLineAtOffset(active->GetCaretPos());
            size_t startCol =
                active->GetCaretPos() - active->GetLineOffset(startLine);

            for (size_t i = 0; i < lines.size(); ++i) {
              size_t currentLine = startLine + i;
              if (currentLine >= active->GetTotalLines()) {
                active->Insert(active->GetTotalLength(), "\n");
              }
              size_t lineOffset = active->GetLineOffset(currentLine);
              size_t insertPos =
                  lineOffset +
                  (std::min)(startCol, active->GetLineOffset(currentLine + 1) -
                                           lineOffset - 1);
              active->Insert(insertPos, lines[i]);
            }
          } else {
            active->Insert(active->GetCaretPos(), text.data());
            active->MoveCaret(static_cast<int>(strlen(text.data())));
          }
        }
      }
    } else {
        DebugLog("Editor::Paste - GetClipboardData failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
    }
    CloseClipboard();
  } else {
    DebugLog("Editor::Paste - OpenClipboard failed: " + GetWin32ErrorString(GetLastError()), LOG_ERROR);
  }
}
void Editor::LogMessage(const std::string &msg) {
  if (!m_messagesBuffer) {
    m_messagesBuffer = GetBufferByName(L"*Messages*");
    if (!m_messagesBuffer) {
      auto buffer = std::make_unique<Buffer>();
      buffer->SetPath(L"*Messages*");
      buffer->SetScratch(true);
      m_messagesBuffer = buffer.get();
      m_buffers.push_back(std::move(buffer));
    }
  }

  if (m_messagesBuffer) {
    m_messagesBuffer->Insert(m_messagesBuffer->GetTotalLength(), msg + "\n");
  }
}

void Editor::TagJump() {
  Buffer *active = GetActiveBuffer();
  if (!active) return;
  
  std::string targetText;
  if (active->HasSelection()) {
      targetText = active->GetSelectedText();
  } else {
      // Find word under caret
      size_t pos = active->GetCaretPos();
      size_t start = pos;
      size_t end = pos;
      std::string allText = active->GetText(0, active->GetTotalLength());
      // Expand backward
      while (start > 0 && allText[start - 1] != '\n' && allText[start - 1] != '\r') {
          start--;
      }
      // Expand forward
      while (end < allText.length() && allText[end] != '\n' && allText[end] != '\r') {
          end++;
      }
      targetText = allText.substr(start, end - start);
  }

  if (targetText.empty()) return;

  // Pattern: filename(line)
  size_t parenOpen = targetText.find('(');
  if (parenOpen != std::string::npos) {
      size_t parenClose = targetText.find(')', parenOpen);
      if (parenClose != std::string::npos) {
          std::string filename = targetText.substr(0, parenOpen);
          std::string lineStr = targetText.substr(parenOpen + 1, parenClose - parenOpen - 1);
          
          try {
              int lineNum = std::stoi(lineStr);
              std::wstring wFilename = StringToWString(filename);
              
              // Resolve relative to project path or active buffer's dir
              std::wstring basePath = active->GetPath();
              if (basePath.empty() || basePath[0] == L'*') {
                  basePath = SettingsManager::Instance().GetProjectDirectory();
              }
              
              if (!basePath.empty()) {
                  size_t lastSlash = basePath.find_last_of(L"\\/");
                  if (lastSlash != std::wstring::npos) {
                      std::wstring dir = basePath.substr(0, lastSlash + 1);
                      wFilename = dir + wFilename;
                  }
              }

              size_t newBufIdx = OpenFile(wFilename);
              if (newBufIdx != static_cast<size_t>(-1)) {
                  SwitchToBuffer(newBufIdx);
                  Buffer* newBuf = GetActiveBuffer();
                  if (newBuf) {
                      size_t offset = newBuf->GetLineOffset(lineNum - 1);
                      newBuf->SetCaretPos(offset);
                      newBuf->SetSelectionAnchor(offset);
                      newBuf->SetScrollLine(lineNum > 20 ? (size_t)lineNum - 20 : 0);
                  }
              }
          } catch (...) {
              // Ignore parse errors
          }
      }
  }
}

Buffer *Editor::GetBufferByName(const std::wstring &name) {
  for (auto &buf : m_buffers) {
    if (buf->GetPath() == name) {
      return buf.get();
    }
  }
  return nullptr;
}
