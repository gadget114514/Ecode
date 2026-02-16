#include "../include/Editor.h"
#include "../include/Process.h"

#define WM_SHELL_OUTPUT (WM_USER + 101)

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
        output->text = text;
        PostMessage(g_mainHwnd, WM_SHELL_OUTPUT, (WPARAM)output, 0);
      })) {
    buffer->SetShellProcess(std::move(process));
    m_buffers.push_back(std::move(buffer));
    m_activeBufferIndex = m_buffers.size() - 1;
    return m_activeBufferIndex;
  }
  return static_cast<size_t>(-1);
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
      MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pMem, len);
      GlobalUnlock(hMem);
      SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
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
    }
    CloseClipboard();
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

Buffer *Editor::GetBufferByName(const std::wstring &name) {
  for (auto &buf : m_buffers) {
    if (buf->GetPath() == name) {
      return buf.get();
    }
  }
  return nullptr;
}
