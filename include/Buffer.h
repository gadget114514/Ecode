#pragma once

#include "MemoryMappedFile.h"
#include "PieceTable.h"
#include <algorithm>
#include <memory>
#include <set>
#include <string>

enum class Encoding { UTF8, UTF16LE, UTF16BE, ANSI };

enum class SelectionMode {
    Normal,
    Box
};

class Buffer {
public:
  Buffer();
  ~Buffer();

  bool OpenFile(const std::wstring &path);
  bool SaveFile(const std::wstring &path);

  Encoding GetEncoding() const { return m_encoding; }

  void Insert(size_t pos, const std::string &text);
  void Delete(size_t pos, size_t length);

  std::string GetText(size_t pos, size_t length) const;
  size_t GetTotalLength() const;
  size_t GetTotalLines() const;
  size_t GetVisibleLineCount() const;
  size_t GetLineOffset(size_t lineIndex) const;
  size_t GetLineAtOffset(size_t offset) const;
  size_t Find(const std::string &query, size_t startPos, bool forward = true,
              bool useRegex = false, bool matchCase = true) const;
  void Replace(size_t start, size_t end, const std::string &replacement);

  std::string GetSelectedText() const;
  void DeleteSelection();

  void Undo();
  void Redo();
  bool CanUndo() const;
  bool CanRedo() const;

  void SelectLine(size_t lineIndex);

  void SetSelectionMode(SelectionMode mode) { m_selectionMode = mode; }
  SelectionMode GetSelectionMode() const { return m_selectionMode; }
  
  struct SelectionRange {
    size_t start;
    size_t end;
  };
  std::vector<SelectionRange> GetSelectionRanges() const;

  struct HighlightRange {
    size_t start;
    size_t length;
    int type; // 0: normal, 1: keyword, 2: string, 3: number, 4: comment, 5: function
  };
  void SetHighlights(const std::vector<HighlightRange> &highlights) {
    m_highlights = highlights;
  }
  const std::vector<HighlightRange> &GetHighlights() const {
    return m_highlights;
  }

  const std::wstring &GetPath() const { return m_filePath; }
  bool IsDirty() const { return m_isDirty; }

  void SetScratch(bool scratch) { m_isScratch = scratch; }
  bool IsScratch() const { return m_isScratch; }

  size_t GetCaretPos() const { return m_caretPos; }
  void SetCaretPos(size_t pos) {
    size_t total = GetTotalLength();
    m_caretPos = (pos > total) ? total : pos;
  }
  void MoveCaret(int delta) {
    size_t newPos =
        static_cast<size_t>((std::max)(0LL, static_cast<long long>(m_caretPos) +
                                                static_cast<long long>(delta)));
    SetCaretPos(newPos);
    UpdateDesiredColumn();
  }

  void MoveCaretUp();
  void MoveCaretDown();
  void MoveCaretHome();
  void MoveCaretEnd();
  void MoveCaretPageUp(size_t linesPerPage);
  void MoveCaretPageDown(size_t linesPerPage);
  void MoveCaretByChar(int delta);

  void UpdateDesiredColumn();

  size_t GetSelectionAnchor() const { return m_selectionAnchor; }
  void SetSelectionAnchor(size_t pos) {
    size_t total = GetTotalLength();
    m_selectionAnchor = (pos > total) ? total : pos;
  }
  bool HasSelection() const { return m_selectionAnchor != m_caretPos; }
  void GetSelectionRange(size_t &start, size_t &end) const {
    start = (std::min)(m_caretPos, m_selectionAnchor);
    end = (std::max)(m_caretPos, m_selectionAnchor);
  }

  size_t GetScrollLine() const { return m_scrollLine; }
  void SetScrollLine(size_t line) { m_scrollLine = line; }

  float GetScrollX() const { return m_scrollX; }
  void SetScrollX(float scrollX) { m_scrollX = scrollX; }

  // Folding
  void FoldLine(size_t line) { m_foldedLines.insert(line); }
  void UnfoldLine(size_t line) { m_foldedLines.erase(line); }
  void ToggleFold(size_t line);
  bool IsLineFolded(size_t line) const { return m_foldedLines.count(line) > 0; }
  const std::set<size_t> &GetFoldedLines() const { return m_foldedLines; }

  std::string GetVisibleText() const;
  size_t LogicalToVisualOffset(size_t logicalOffset) const;
  size_t VisualToLogicalOffset(size_t visualOffset) const;
  size_t GetPhysicalLine(size_t visualLineIndex) const;

private:
  std::wstring m_filePath;
  std::unique_ptr<MemoryMappedFile> m_mmFile;
  PieceTable m_pieceTable;
  SelectionMode m_selectionMode = SelectionMode::Normal;
  size_t m_caretPos;
  size_t m_selectionAnchor;
  size_t m_scrollLine;
  float m_scrollX;
  size_t m_desiredColumn; // For vertical movement
  Encoding m_encoding;
  std::string m_convertedData; // For files that need conversion (UTF-16)
  std::set<size_t> m_foldedLines;
  std::vector<HighlightRange> m_highlights;
  bool m_isDirty;
  bool m_isScratch;
};
