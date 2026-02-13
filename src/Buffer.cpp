#include "../include/Buffer.h"

Buffer::Buffer()
    : m_caretPos(0), m_selectionAnchor(0), m_scrollLine(0), m_scrollX(0.0f),
      m_desiredColumn(0), m_encoding(Encoding::UTF8), m_isDirty(false),
      m_isScratch(false) {
  m_mmFile = std::make_unique<MemoryMappedFile>();
}

Buffer::~Buffer() {}

bool Buffer::OpenFile(const std::wstring &path) {
  if (m_mmFile->Open(path)) {
    m_filePath = path;
    const char *data = m_mmFile->GetData();
    size_t size = m_mmFile->GetSize();
    m_encoding = Encoding::UTF8;
    m_convertedData.clear();

    if (size >= 2) {
      if (static_cast<unsigned char>(data[0]) == 0xFF &&
          static_cast<unsigned char>(data[1]) == 0xFE) {
        m_encoding = Encoding::UTF16LE;
      } else if (static_cast<unsigned char>(data[0]) == 0xFE &&
                 static_cast<unsigned char>(data[1]) == 0xFF) {
        m_encoding = Encoding::UTF16BE;
      }
    }
    if (size >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF) {
      m_encoding = Encoding::UTF8; // UTF8 with BOM
      data += 3;
      size -= 3;
    }

    if (m_encoding == Encoding::UTF16LE || m_encoding == Encoding::UTF16BE) {
      bool be = (m_encoding == Encoding::UTF16BE);
      const wchar_t *wdata = reinterpret_cast<const wchar_t *>(data + 2);
      size_t wlen = (size - 2) / 2;

      std::vector<wchar_t> switched;
      if (be) {
        switched.resize(wlen);
        for (size_t i = 0; i < wlen; ++i) {
          switched[i] = ((wdata[i] & 0xFF) << 8) | ((wdata[i] >> 8) & 0xFF);
        }
        wdata = switched.data();
      }

      int utf8len = WideCharToMultiByte(
          CP_UTF8, 0, wdata, static_cast<int>(wlen), NULL, 0, NULL, NULL);
      m_convertedData.resize(utf8len);
      WideCharToMultiByte(CP_UTF8, 0, wdata, static_cast<int>(wlen),
                          &m_convertedData[0], utf8len, NULL, NULL);
      m_pieceTable.LoadOriginal(m_convertedData.data(), m_convertedData.size());
    } else {
      m_pieceTable.LoadOriginal(data, size);
    }

    m_isDirty = false;
    m_caretPos = 0;
    m_selectionAnchor = 0;
    m_scrollLine = 0;
    return true;
  }
  return false;
}

bool SafeSave(const std::wstring &targetPath, const std::string &content);

bool Buffer::SaveFile(const std::wstring &path) {
  size_t total = m_pieceTable.GetTotalLength();
  std::string content = m_pieceTable.GetText(0, total);

  if (SafeSave(path, content)) {
    if (path == m_filePath) {
      m_isDirty = false;
    }
    return true;
  }
  return false;
}

void Buffer::Insert(size_t pos, const std::string &text) {
  m_pieceTable.Insert(pos, text);
  m_isDirty = true;
}

void Buffer::Delete(size_t pos, size_t length) {
  m_pieceTable.Delete(pos, length);
  m_isDirty = true;
}

std::string Buffer::GetText(size_t pos, size_t length) const {
  return m_pieceTable.GetText(pos, length);
}

size_t Buffer::GetTotalLength() const { return m_pieceTable.GetTotalLength(); }

size_t Buffer::GetTotalLines() const { return m_pieceTable.GetTotalLines(); }

size_t Buffer::GetVisibleLineCount() const {
  return GetTotalLines() - m_foldedLines.size();
}

size_t Buffer::GetLineOffset(size_t lineIndex) const {
  return m_pieceTable.GetLineOffset(lineIndex);
}

size_t Buffer::GetLineAtOffset(size_t offset) const {
  return m_pieceTable.GetLineAtOffset(offset);
}

#include <regex>

size_t Buffer::Find(const std::string &query, size_t startPos, bool forward,
                    bool useRegex, bool matchCase) const {
  std::string text = m_pieceTable.GetText(0, m_pieceTable.GetTotalLength());

  if (useRegex) {
    try {
      std::regex_constants::syntax_option_type flags =
          std::regex_constants::ECMAScript;
      if (!matchCase)
        flags |= std::regex_constants::icase;

      std::regex re(query, flags);
      if (forward) {
        std::smatch match;
        std::string searchPart = text.substr(startPos);
        if (std::regex_search(searchPart, match, re)) {
          return startPos + match.position();
        }
      } else {
        // Backward regex search
        auto words_begin =
            std::sregex_iterator(text.begin(), text.begin() + startPos, re);
        auto words_end = std::sregex_iterator();
        size_t lastPos = std::string::npos;
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
          lastPos = i->position();
        }
        return lastPos;
      }
    } catch (...) {
      return std::string::npos;
    }
  } else {
    // Literal search
    if (!matchCase) {
      // Simple case-insensitive search
      std::string lowerText = text;
      std::string lowerQuery = query;
      std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(),
                     ::tolower);
      std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                     ::tolower);
      if (forward)
        return lowerText.find(lowerQuery, startPos);
      else
        return lowerText.rfind(lowerQuery, startPos);
    } else {
      if (forward)
        return text.find(query, startPos);
      else
        return text.rfind(query, startPos);
    }
  }
  return std::string::npos;
}

void Buffer::Replace(size_t start, size_t end, const std::string &replacement) {
  if (start > end)
    return;
  Delete(start, end - start);
  Insert(start, replacement);
}

std::string Buffer::GetSelectedText() const {
  std::vector<SelectionRange> ranges = GetSelectionRanges();
  if (ranges.empty())
    return "";
  
  if (ranges.size() == 1) {
    return GetText(ranges[0].start, ranges[0].end - ranges[0].start);
  }

  std::string result;
  for (size_t i = 0; i < ranges.size(); ++i) {
    result += GetText(ranges[i].start, ranges[i].end - ranges[i].start);
    if (i < ranges.size() - 1) {
      result += "\n";
    }
  }
  return result;
}

void Buffer::UpdateDesiredColumn() {
  size_t line = GetLineAtOffset(m_caretPos);
  size_t lineStart = GetLineOffset(line);
  m_desiredColumn = m_caretPos - lineStart;
}

void Buffer::MoveCaretUp() {
  size_t line = GetLineAtOffset(m_caretPos);
  if (line > 0) {
    size_t prevLine = line - 1;
    size_t prevStart = GetLineOffset(prevLine);
    size_t nextStart = GetLineOffset(line);
    size_t lineLen = nextStart - prevStart - 1; // -1 for newline

    size_t newCol = (std::min)(m_desiredColumn, lineLen);
    SetCaretPos(prevStart + newCol);
  }
}

void Buffer::MoveCaretDown() {
  size_t line = GetLineAtOffset(m_caretPos);
  size_t totalLines = GetTotalLines();
  if (line < totalLines - 1) {
    size_t nextLine = line + 1;
    size_t lineStart = GetLineOffset(nextLine);
    size_t nextLineStart = (nextLine < totalLines - 1)
                               ? GetLineOffset(nextLine + 1)
                               : GetTotalLength() + 1;
    size_t lineLen = nextLineStart - lineStart - 1;

    size_t newCol = (std::min)(m_desiredColumn, lineLen);
    SetCaretPos(lineStart + newCol);
  }
}

void Buffer::MoveCaretHome() {
  size_t line = GetLineAtOffset(m_caretPos);
  SetCaretPos(GetLineOffset(line));
  UpdateDesiredColumn();
}

void Buffer::MoveCaretEnd() {
  size_t line = GetLineAtOffset(m_caretPos);
  size_t totalLines = GetTotalLines();
  size_t nextStart =
      (line < totalLines - 1) ? GetLineOffset(line + 1) : GetTotalLength() + 1;
  SetCaretPos(nextStart - 1);
  UpdateDesiredColumn();
}

void Buffer::MoveCaretPageUp(size_t linesPerPage) {
  size_t line = GetLineAtOffset(m_caretPos);
  size_t newLine = (line > linesPerPage) ? line - linesPerPage : 0;
  size_t lineStart = GetLineOffset(newLine);
  size_t nextStart = (newLine < GetTotalLines() - 1)
                         ? GetLineOffset(newLine + 1)
                         : GetTotalLength() + 1;
  size_t lineLen = nextStart - lineStart - 1;
  SetCaretPos(lineStart + (std::min)(m_desiredColumn, lineLen));
}

void Buffer::MoveCaretPageDown(size_t linesPerPage) {
  size_t line = GetLineAtOffset(m_caretPos);
  size_t total = GetTotalLines();
  size_t newLine =
      (line + linesPerPage < total) ? line + linesPerPage : total - 1;
  size_t lineStart = GetLineOffset(newLine);
  size_t nextStart =
      (newLine < total - 1) ? GetLineOffset(newLine + 1) : GetTotalLength() + 1;
  size_t lineLen = nextStart - lineStart - 1;
  SetCaretPos(lineStart + (std::min)(m_desiredColumn, lineLen));
}

void Buffer::MoveCaretByChar(int delta) {
  if (delta == 0) return;
  
  std::string text = m_pieceTable.GetText(0, m_pieceTable.GetTotalLength());
  size_t pos = m_caretPos;
  size_t total = text.length();

  if (delta > 0) {
    for (int i = 0; i < delta && pos < total; ++i) {
      unsigned char c = static_cast<unsigned char>(text[pos]);
      if (c < 0x80) pos += 1;
      else if ((c & 0xE0) == 0xC0) pos += 2;
      else if ((c & 0xF0) == 0xE0) pos += 3;
      else if ((c & 0xF8) == 0xF0) pos += 4;
      else pos += 1; // Fallback for invalid sequence
    }
  } else {
    for (int i = 0; i < -delta && pos > 0; ++i) {
      pos--;
      while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xC0) == 0x80) {
        pos--;
      }
    }
  }
  
  SetCaretPos((std::min)(pos, total));
  UpdateDesiredColumn();
}

void Buffer::DeleteSelection() {
  std::vector<SelectionRange> ranges = GetSelectionRanges();
  if (ranges.empty())
    return;

  // Delete ranges backwards to keep offsets valid
  std::sort(ranges.begin(), ranges.end(), [](const SelectionRange& a, const SelectionRange& b) {
      return a.start > b.start;
  });

  for (const auto& r : ranges) {
      Delete(r.start, r.end - r.start);
  }

  m_caretPos = ranges.back().start; // Reset to start of first (now last in sorted) range
  m_selectionAnchor = m_caretPos;
}

void Buffer::Undo() {
  m_pieceTable.Undo();
  m_isDirty = true; // Still dirty if we undo/redo? Technically yes if it
                    // differs from saved state.
}

void Buffer::Redo() {
  m_pieceTable.Redo();
  m_isDirty = true;
}

bool Buffer::CanUndo() const { return m_pieceTable.CanUndo(); }

bool Buffer::CanRedo() const { return m_pieceTable.CanRedo(); }

void Buffer::ToggleFold(size_t line) {
  if (m_foldedLines.count(line)) {
    m_foldedLines.erase(line);
  } else {
    m_foldedLines.insert(line);
  }
}

std::string Buffer::GetVisibleText() const {
  if (m_foldedLines.empty()) {
    return GetText(0, GetTotalLength());
  }

  size_t totalLines = GetTotalLines();
  std::string visibleText;
  for (size_t i = 0; i < totalLines; ++i) {
    if (m_foldedLines.count(i) == 0) {
      size_t start = GetLineOffset(i);
      size_t end =
          (i < totalLines - 1) ? GetLineOffset(i + 1) : GetTotalLength();
      visibleText += GetText(start, end - start);
    }
  }
  return visibleText;
}

size_t Buffer::LogicalToVisualOffset(size_t logicalOffset) const {
  if (m_foldedLines.empty())
    return logicalOffset;

  size_t visualOffset = 0;
  size_t totalLines = GetTotalLines();

  for (size_t i = 0; i < totalLines; ++i) {
    size_t lineStart = GetLineOffset(i);
    size_t lineEnd =
        (i < totalLines - 1) ? GetLineOffset(i + 1) : GetTotalLength();
    size_t lineLen = lineEnd - lineStart;

    if (m_foldedLines.count(i)) {
      if (logicalOffset >= lineStart && logicalOffset < lineEnd) {
        return visualOffset;
      }
    } else {
      if (logicalOffset >= lineStart && logicalOffset < lineEnd) {
        return visualOffset + (logicalOffset - lineStart);
      }
      visualOffset += lineLen;
    }
  }
  return visualOffset;
}

size_t Buffer::VisualToLogicalOffset(size_t visualOffset) const {
  if (m_foldedLines.empty())
    return visualOffset;

  size_t totalLines = GetTotalLines();
  size_t currentVisual = 0;

  for (size_t i = 0; i < totalLines; ++i) {
    if (m_foldedLines.count(i))
      continue;

    size_t lineStart = GetLineOffset(i);
    size_t lineEnd =
        (i < totalLines - 1) ? GetLineOffset(i + 1) : GetTotalLength();
    size_t lineLen = lineEnd - lineStart;

    if (visualOffset >= currentVisual &&
        visualOffset < currentVisual + lineLen) {
      return lineStart + (visualOffset - currentVisual);
    }
    currentVisual += lineLen;
  }
  return GetTotalLength();
}

size_t Buffer::GetPhysicalLine(size_t visualLineIndex) const {
  size_t currentVisual = 0;
  size_t totalLines = GetTotalLines();
  for (size_t i = 0; i < totalLines; ++i) {
    if (m_foldedLines.count(i) == 0) {
      if (currentVisual == visualLineIndex)
        return i;
      currentVisual++;
    }
  }
  return totalLines;
}

void Buffer::SelectLine(size_t lineIndex) {
  if (lineIndex < GetTotalLines()) {
    size_t start = GetLineOffset(lineIndex);
    size_t end = (lineIndex < GetTotalLines() - 1) ? GetLineOffset(lineIndex + 1)
                                                   : GetTotalLength();
    SetSelectionAnchor(start);
    SetCaretPos(end);
  }
}

std::vector<Buffer::SelectionRange> Buffer::GetSelectionRanges() const {
  std::vector<SelectionRange> ranges;
  if (m_selectionMode == SelectionMode::Normal) {
    if (m_caretPos != m_selectionAnchor) {
      ranges.push_back({(std::min)(m_caretPos, m_selectionAnchor),
                        (std::max)(m_caretPos, m_selectionAnchor)});
    }
  } else if (m_selectionMode == SelectionMode::Box) {
    size_t startLine = GetLineAtOffset(m_selectionAnchor);
    size_t endLine = GetLineAtOffset(m_caretPos);
    size_t minLine = (std::min)(startLine, endLine);
    size_t maxLine = (std::max)(startLine, endLine);

    size_t anchorCol = m_selectionAnchor - GetLineOffset(startLine);
    size_t caretCol = m_caretPos - GetLineOffset(endLine);
    size_t minCol = (std::min)(anchorCol, caretCol);
    size_t maxCol = (std::max)(anchorCol, caretCol);

    for (size_t l = minLine; l <= maxLine; ++l) {
      size_t lineStart = GetLineOffset(l);
      size_t lineLength = (l < GetTotalLines() - 1)
                              ? (GetLineOffset(l + 1) - lineStart)
                              : (GetTotalLength() - lineStart);
      
      // Cleanup length (remove newline if needed)
      std::string lineText = m_pieceTable.GetText(lineStart, lineLength);
      while (!lineText.empty() && (lineText.back() == '\r' || lineText.back() == '\n')) {
          lineText.pop_back();
          lineLength--;
      }

      if (minCol <= lineLength) {
        size_t actualStart = lineStart + minCol;
        size_t actualEnd = lineStart + (std::min)(maxCol, lineLength);
        if (actualStart < actualEnd) {
          ranges.push_back({actualStart, actualEnd});
        }
      }
    }
  }
  return ranges;
}
