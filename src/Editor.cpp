#include "../include/Editor.h"
#include "../include/Process.h"
#include "../include/SettingsManager.h"
#include "../include/StringHelpers.h"
#include "Globals.inl"

#include <filesystem>
#include <regex>
#include <algorithm>
// namespace fs alias is already in Globals.inl

Editor::Editor() : m_activeBufferIndex(0) {}
Editor::~Editor() {}

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
  buffer->Insert(0, "// Ecode Script Console\n// Type JS code and press Enter to evaluate.\n> ");
  buffer->SetInputStart(buffer->GetTotalLength());
  buffer->SetCaretPos(buffer->GetTotalLength());

  m_buffers.push_back(std::move(buffer));
  m_activeBufferIndex = m_buffers.size() - 1;
  return m_activeBufferIndex;
}

static void SendResultText(const std::string &text) {
  if (text.empty()) return;
  auto *batch = new std::string(text);
  PostMessage(g_mainHwnd, WM_GREP_RESULT, 0, (LPARAM)batch);
}

static DWORD WINAPI GrepSearchThread(LPVOID param) {
  GrepSearchParams *params = (GrepSearchParams*)param;

  std::wregex re;
  bool hasRegex = false;
  if (params->useRegex && !params->pattern.empty()) {
    try {
      std::wregex::flag_type flags = std::wregex::ECMAScript;
      if (!params->matchCase) flags |= std::wregex::icase;
      re.assign(params->pattern, flags);
      hasRegex = true;
    } catch (...) {
      hasRegex = false;
    }
  }

  std::string batchText;
  int totalMatches = 0;
  int filesProcessed = 0;

  auto MatchLine = [&](const std::string &line) -> bool {
    if (hasRegex)
      return std::regex_search(StringToWString(line), re);
    std::string pat = WStringToString(params->pattern);
    if (params->matchCase)
      return line.find(pat) != std::string::npos;
    std::string lowerLine = line;
    std::string lowerPat = pat;
    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
    std::transform(lowerPat.begin(), lowerPat.end(), lowerPat.begin(), ::tolower);
    return lowerLine.find(lowerPat) != std::string::npos;
  };

  try {
    if (fs::exists(params->dir) && fs::is_directory(params->dir)) {
      for (const auto &entry : fs::recursive_directory_iterator(params->dir)) {
        if (InterlockedCompareExchange(&g_grepCancelFlag, 1, 1))
          break;
        if (!fs::is_regular_file(entry.status()))
          continue;
        if (!params->extFilter.empty()) {
          std::wstring ext = entry.path().extension().wstring();
          if (ext.empty()) continue;
          bool found = false;
          std::wstring filter = params->extFilter;
          size_t p = 0;
          while ((p = filter.find(L';')) != std::wstring::npos) {
            std::wstring e = filter.substr(0, p);
            if (_wcsicmp(ext.c_str(), e.c_str()) == 0) { found = true; break; }
            filter.erase(0, p + 1);
          }
          if (!found && _wcsicmp(ext.c_str(), filter.c_str()) != 0) continue;
        }
        std::ifstream file(entry.path());
        if (!file) continue;
        std::string line;
        int lineNum = 0;
        std::wstring wPath = entry.path().wstring();
        while (std::getline(file, line)) {
          if (line.length() > 4096) break;
          lineNum++;
          if (MatchLine(line)) {
            batchText += WStringToString(wPath) + "(" + std::to_string(lineNum) + "): " + line + "\n";
            totalMatches++;
            if (batchText.length() >= 4096) {
              SendResultText(batchText);
              batchText.clear();
            }
          }
        }
        filesProcessed++;
        if (filesProcessed % 25 == 0) {
          PostMessage(g_mainHwnd, WM_GREP_PROGRESS, 0, filesProcessed);
        }
      }
    }
  } catch (const fs::filesystem_error &) {
  } catch (...) {
    DebugLog("GrepSearchThread - Exception during search", LOG_ERROR);
  }

  SendResultText(batchText);
  PostMessage(g_mainHwnd, WM_GREP_COMPLETE, totalMatches, 0);

  delete params;
  return 0;
}

void Editor::FindInFiles(const std::wstring &dir, const std::wstring &pattern,
                         const std::wstring &extFilter,
                         bool useRegex, bool matchCase) {
  extern HWND g_mainHwnd;

  Buffer *results = GetBufferByName(L"*Find Results*");
  if (results) {
    results->Delete(0, results->GetTotalLength());
    for (size_t i = 0; i < m_buffers.size(); i++) {
      if (m_buffers[i].get() == results) {
        SwitchToBuffer(i);
        break;
      }
    }
  } else {
    auto buf = std::make_unique<Buffer>();
    buf->SetPath(L"*Find Results*");
    buf->SetScratch(true);
    results = buf.get();
    m_buffers.push_back(std::move(buf));
    SwitchToBuffer(m_buffers.size() - 1);
  }

  std::string header = "Grep: \"" + WStringToString(pattern) + "\" in " + WStringToString(dir) + "\n";
  if (!extFilter.empty())
    header += "Filter: " + WStringToString(extFilter) + "\n";
  header += "\n";
  results->Insert(0, header);
  results->SetCaretPos(results->GetTotalLength());
  results->SetSelectionAnchor(results->GetCaretPos());
  UpdateMenu(g_mainHwnd);

  auto *params = new GrepSearchParams{
    dir, pattern, extFilter, useRegex, matchCase
  };

  g_grepSearchActive = true;
  InterlockedExchange(&g_grepCancelFlag, 0);
  CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GrepSearchThread, params, 0, NULL);

  DebugLog("FindInFiles: background search started", LOG_INFO);
}

void Editor::CancelFindInFiles() {
  InterlockedExchange(&g_grepCancelFlag, 1);
  g_grepSearchActive = false;
}

void Editor::FindFile(const std::wstring &pattern, const std::wstring &dir) {
  extern HWND g_mainHwnd;
  extern std::vector<AppTabInfo> g_appTabs;
  extern int g_activeAppTab;

  HWND hListView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
      WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
      0, 0, 100, 100, g_mainHwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

  LVCOLUMNW lvc = {0};
  lvc.mask = LVCF_TEXT | LVCF_WIDTH;
  lvc.cx = 250; lvc.pszText = L"Name"; ListView_InsertColumn(hListView, 0, &lvc);
  lvc.cx = 400; lvc.pszText = L"Path"; ListView_InsertColumn(hListView, 1, &lvc);

  AppTabInfo tab;
  tab.hwnd = hListView;
  tab.label = L"Files: " + pattern;
  tab.type = 0;
  tab.data = nullptr;
  g_appTabs.push_back(std::move(tab));
  int appIdx = static_cast<int>(g_appTabs.size()) - 1;
  g_activeAppTab = appIdx;
  UpdateMenu(g_mainHwnd);

  int idx = 0;
  try {
    if (fs::exists(dir) && fs::is_directory(dir)) {
      for (const auto &entry : fs::recursive_directory_iterator(dir)) {
        if (!fs::is_regular_file(entry.status())) continue;
        std::wstring fname = entry.path().filename().wstring();
        if (fname.find(pattern) == std::wstring::npos) continue;
        LVITEMW item = {0};
        item.mask = LVIF_TEXT;
        item.iItem = idx;
        item.pszText = (LPWSTR)fname.c_str();
        ListView_InsertItem(hListView, &item);
        ListView_SetItemText(hListView, idx, 1, (LPWSTR)entry.path().wstring().c_str());
        idx++;
      }
    }
  } catch (...) {}
  DebugLog("FindFile: " + std::to_string(idx) + " files found", LOG_INFO);
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

bool Editor::IsValidBuffer(Buffer *buf) const {
  for (const auto &b : m_buffers) {
    if (b.get() == buf)
      return true;
  }
  return false;
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
          DebugLog("Editor::Copy - SetClipboardData failed: " +
                       GetWin32ErrorString(GetLastError()),
                   LOG_ERROR);
          GlobalFree(hMem);
        }
      } else {
        DebugLog("Editor::Copy - GlobalLock failed", LOG_ERROR);
        GlobalFree(hMem);
      }
    } else {
      DebugLog("Editor::Copy - GlobalAlloc failed: " +
                   GetWin32ErrorString(GetLastError()),
               LOG_ERROR);
    }
    CloseClipboard();
  } else {
    DebugLog("Editor::Copy - OpenClipboard failed: " +
                 GetWin32ErrorString(GetLastError()),
             LOG_ERROR);
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
      DebugLog("Editor::Paste - GetClipboardData failed: " +
                   GetWin32ErrorString(GetLastError()),
               LOG_ERROR);
    }
    CloseClipboard();
  } else {
    DebugLog("Editor::Paste - OpenClipboard failed: " +
                 GetWin32ErrorString(GetLastError()),
             LOG_ERROR);
  }
}
static const size_t kMaxMessageBufferSize = 1024 * 1024; // 1MB limit
static const size_t kMessageTrimSize = 64 * 1024;        // trim 64KB when exceeded

void Editor::LogMessage(const std::string &msg) {
  std::lock_guard<std::mutex> lock(m_logMutex);

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
    // Trim buffer if it exceeds size limit (ring-buffer behavior)
    if (m_messagesBuffer->GetTotalLength() > kMaxMessageBufferSize) {
      m_messagesBuffer->Delete(0, kMessageTrimSize);
      m_messagesBuffer->CompactAddedBuffer();
    }
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
              
              // If not an absolute path, resolve relative to active buffer/project dir
              if (wFilename.length() < 2 || (wFilename[1] != L':' && wFilename[0] != L'\\')) {
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

SessionBufferState Editor::GetBufferState(size_t index) const {
  SessionBufferState state;
  if (index >= m_buffers.size()) return state;

  auto *buf = m_buffers[index].get();
  state.path = buf->GetPath();
  state.caretPos = buf->GetCaretPos();
  state.selectionAnchor = buf->GetSelectionAnchor();
  state.scrollLine = (int)buf->GetScrollLine();
  state.scrollX = (int)buf->GetScrollX();
  state.encoding = (int)buf->GetEncoding();
  state.isDirty = buf->IsDirty();
  state.isScratch = buf->IsScratch();
  state.isShell = buf->IsShell();

  for (auto line : buf->GetFoldedLines())
    state.foldedLines.push_back((int)line);

  return state;
}

void Editor::RestoreBufferState(size_t index, const SessionBufferState &state) {
  if (index >= m_buffers.size()) return;

  auto *buf = m_buffers[index].get();
  buf->SetCaretPos(state.caretPos);
  buf->SetSelectionAnchor(state.selectionAnchor);
  buf->SetScrollLine(state.scrollLine);
  buf->SetScrollX((float)state.scrollX);

  if (state.isScratch) buf->SetScratch(true);
  if (state.isShell) buf->SetShell(true);

  for (int line : state.foldedLines)
    buf->FoldLine((size_t)line);
}
