#include "../include/PieceTable.h"
#include <algorithm>

PieceTable::PieceTable()
    : m_originalData(nullptr), m_originalLength(0), m_totalLength(0) {}

PieceTable::~PieceTable() {}

size_t CountNewlines(const char *data, size_t length) {
  size_t count = 0;
  for (size_t i = 0; i < length; ++i) {
    if (data[i] == '\n')
      count++;
  }
  return count;
}

void PieceTable::LoadOriginal(const char *data, size_t length) {
  m_originalData = data;
  m_originalLength = length;
  m_pieces.clear();
  if (length > 0) {
    m_pieces.emplace_back(BufferType::Original, 0, length,
                          CountNewlines(data, length));
  }
  m_totalLength = length;
}

const char *PieceTable::GetPieceData(const Piece &p) const {
  if (p.bufferType == BufferType::Original) {
    return m_originalData + p.start;
  } else {
    return m_addedBuffer.data() + p.start;
  }
}

PieceTable::PieceInfo PieceTable::FindPiecePosition(size_t pos) const {
  size_t accumulated = 0;
  for (size_t i = 0; i < m_pieces.size(); ++i) {
    if (pos < accumulated + m_pieces[i].length) {
      return {i, pos - accumulated};
    }
    accumulated += m_pieces[i].length;
  }
  // Handle position at the very end
  if (pos == accumulated && !m_pieces.empty()) {
    return {m_pieces.size() - 1, m_pieces.back().length};
  }
  return {0, 0};
}

void PieceTable::Insert(size_t pos, const std::string &text) {
  if (text.empty())
    return;

  SaveState();

  size_t addedStart = m_addedBuffer.length();
  m_addedBuffer += text;

  if (m_pieces.empty()) {
    m_pieces.emplace_back(BufferType::Added, addedStart, text.length(),
                          CountNewlines(text.data(), text.length()));
    m_totalLength = text.length();
    return;
  }

  if (pos >= m_totalLength) {
    // Append to the end
    m_pieces.emplace_back(BufferType::Added, addedStart, text.length(),
                          CountNewlines(text.data(), text.length()));
    m_totalLength += text.length();
    return;
  }

  PieceInfo info = FindPiecePosition(pos);
  Piece target = m_pieces[info.pieceIndex];

  // Split target piece into two if necessary
  if (info.offsetInPiece == 0) {
    // Insert before this piece
    m_pieces.insert(m_pieces.begin() + info.pieceIndex,
                    Piece(BufferType::Added, addedStart, text.length(),
                          CountNewlines(text.data(), text.length())));
  } else if (info.offsetInPiece == target.length) {
    // Insert after this piece
    m_pieces.insert(m_pieces.begin() + info.pieceIndex + 1,
                    Piece(BufferType::Added, addedStart, text.length(),
                          CountNewlines(text.data(), text.length())));
  } else {
    // Split in the middle
    Piece part1(target.bufferType, target.start, info.offsetInPiece,
                CountNewlines(GetPieceData(target), info.offsetInPiece));
    Piece part2(target.bufferType, target.start + info.offsetInPiece,
                target.length - info.offsetInPiece,
                CountNewlines(GetPieceData(target) + info.offsetInPiece,
                              target.length - info.offsetInPiece));
    Piece newPart(BufferType::Added, addedStart, text.length(),
                  CountNewlines(text.data(), text.length()));

    m_pieces.erase(m_pieces.begin() + info.pieceIndex);
    m_pieces.insert(m_pieces.begin() + info.pieceIndex, part2);
    m_pieces.insert(m_pieces.begin() + info.pieceIndex, newPart);
    m_pieces.insert(m_pieces.begin() + info.pieceIndex, part1);
  }

  m_totalLength += text.length();
}

void PieceTable::Delete(size_t pos, size_t length) {
  if (length == 0 || pos >= m_totalLength)
    return;

  SaveState();

  if (pos + length > m_totalLength)
    length = m_totalLength - pos;

  // Implementation can be complex as it might span multiple pieces
  // For now, a simplified version that handles basic cases:
  size_t remainingToDelete = length;
  while (remainingToDelete > 0) {
    PieceInfo info = FindPiecePosition(pos);
    Piece &target = m_pieces[info.pieceIndex];

    size_t deleteInThisPiece =
        std::min(remainingToDelete, target.length - info.offsetInPiece);

    if (info.offsetInPiece == 0 && deleteInThisPiece == target.length) {
      // Remove entire piece
      m_pieces.erase(m_pieces.begin() + info.pieceIndex);
    } else if (info.offsetInPiece == 0) {
      // Shrink from start
      target.start += deleteInThisPiece;
      target.length -= deleteInThisPiece;
      target.lineCount = CountNewlines(GetPieceData(target), target.length);
    } else if (info.offsetInPiece + deleteInThisPiece == target.length) {
      // Shrink from end
      target.length -= deleteInThisPiece;
      target.lineCount = CountNewlines(GetPieceData(target), target.length);
    } else {
      // Split piece and remove middle
      Piece part1(target.bufferType, target.start, info.offsetInPiece,
                  CountNewlines(GetPieceData(target), info.offsetInPiece));
      Piece part2(
          target.bufferType,
          target.start + info.offsetInPiece + deleteInThisPiece,
          target.length - (info.offsetInPiece + deleteInThisPiece),
          CountNewlines(
              GetPieceData(target) + info.offsetInPiece + deleteInThisPiece,
              target.length - (info.offsetInPiece + deleteInThisPiece)));

      m_pieces.erase(m_pieces.begin() + info.pieceIndex);
      m_pieces.insert(m_pieces.begin() + info.pieceIndex, part2);
      m_pieces.insert(m_pieces.begin() + info.pieceIndex, part1);
    }

    remainingToDelete -= deleteInThisPiece;
  }

  m_totalLength -= length;
}

void PieceTable::SaveState() {
  m_undoStack.push_back(m_pieces);
  m_redoStack.clear();
}

void PieceTable::Undo() {
  if (m_undoStack.empty())
    return;

  m_redoStack.push_back(m_pieces);
  m_pieces = m_undoStack.back();
  m_undoStack.pop_back();

  // Recalculate total length
  m_totalLength = 0;
  for (const auto &p : m_pieces) {
    m_totalLength += p.length;
  }
}

void PieceTable::Redo() {
  if (m_redoStack.empty())
    return;

  m_undoStack.push_back(m_pieces);
  m_pieces = m_redoStack.back();
  m_redoStack.pop_back();

  // Recalculate total length
  m_totalLength = 0;
  for (const auto &p : m_pieces) {
    m_totalLength += p.length;
  }
}

std::string PieceTable::GetText(size_t pos, size_t length) const {
  if (length == 0 || pos >= m_totalLength)
    return "";
  if (pos + length > m_totalLength)
    length = m_totalLength - pos;

  std::string result;
  result.reserve(length);

  size_t accumulated = 0;
  size_t remaining = length;
  size_t currentPos = pos;

  for (const auto &piece : m_pieces) {
    size_t pieceEnd = accumulated + piece.length;
    if (currentPos < pieceEnd) {
      size_t offset = currentPos - accumulated;
      size_t count = std::min(remaining, piece.length - offset);

      if (piece.bufferType == BufferType::Original) {
        result.append(m_originalData + piece.start + offset, count);
      } else {
        result += m_addedBuffer.substr(piece.start + offset, count);
      }

      remaining -= count;
      currentPos += count;
      if (remaining == 0)
        break;
    }
    accumulated += piece.length;
  }

  return result;
}

size_t PieceTable::GetTotalLength() const { return m_totalLength; }

size_t PieceTable::GetTotalLines() const {
  size_t count = 1;
  for (const auto &p : m_pieces) {
    count += p.lineCount;
  }
  return count;
}

size_t PieceTable::GetLineOffset(size_t lineIndex) const {
  if (lineIndex == 0)
    return 0;

  size_t currentLine = 0;
  size_t currentOffset = 0;

  for (const auto &p : m_pieces) {
    if (currentLine + p.lineCount >= lineIndex) {
      // Line is in this piece
      const char *data = GetPieceData(p);
      size_t lineInPiece = lineIndex - currentLine;
      size_t foundLines = 0;
      for (size_t i = 0; i < p.length; ++i) {
        if (data[i] == '\n') {
          foundLines++;
          if (foundLines == lineInPiece) {
            return currentOffset + i + 1;
          }
        }
      }
    }
    currentLine += p.lineCount;
    currentOffset += p.length;
  }

  return m_totalLength;
}

size_t PieceTable::GetLineAtOffset(size_t offset) const {
  if (offset == 0)
    return 0;
  if (offset >= m_totalLength)
    return GetTotalLines() - 1;

  size_t currentLine = 0;
  size_t accumulatedOffset = 0;

  for (const auto &p : m_pieces) {
    if (offset < accumulatedOffset + p.length) {
      // Offset is in this piece
      const char *data = GetPieceData(p);
      size_t offsetInPiece = offset - accumulatedOffset;
      currentLine += CountNewlines(data, offsetInPiece);
      return currentLine;
    }
    currentLine += p.lineCount;
    accumulatedOffset += p.length;
  }

  return currentLine;
}
