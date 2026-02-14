#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

enum class BufferType { Original, Added };

struct Piece {
  BufferType bufferType;
  size_t start;
  size_t length;
  size_t lineCount;

  Piece(BufferType type, size_t s, size_t l, size_t lc = 0)
      : bufferType(type), start(s), length(l), lineCount(lc) {}
};

class PieceTable {
public:
  PieceTable();
  ~PieceTable();

  // Load original content (e.g., from memory-mapped file)
  // Note: We assume the caller keeps the 'data' pointer valid for the lifetime
  // of the PieceTable
  void LoadOriginal(const char *data, size_t length);

  // Core editing operations
  void Insert(size_t pos, const std::string &text);
  void Delete(size_t pos, size_t length);

  // Undo/Redo
  void Undo();
  void Redo();
  bool CanUndo() const { return !m_undoStack.empty(); }
  bool CanRedo() const { return !m_redoStack.empty(); }

  // Retrieval
  std::string GetText(size_t pos, size_t length) const;
  void WriteTo(std::function<void(const char *, size_t)> writer) const;
  size_t GetTotalLength() const;
  size_t GetTotalLines() const;
  size_t GetLineOffset(size_t lineIndex) const;
  size_t GetLineAtOffset(size_t offset) const;

  // OPTIMIZATION: Piece table compaction
  void CompactPieces();
  size_t GetPieceCount() const { return m_pieces.size(); }
  void InvalidateLineCache() { m_lineCacheValid = false; }

private:
  const char *m_originalData;
  size_t m_originalLength;
  std::string m_addedBuffer;
  std::vector<Piece> m_pieces;

  size_t m_totalLength;
  size_t m_totalLines;

  const char *GetPieceData(const Piece &p) const;

  // History for Undo/Redo
  std::vector<std::vector<Piece>> m_undoStack;
  std::vector<std::vector<Piece>> m_redoStack;

  void SaveState();

  // Internal helper to find which piece contains the position
  struct PieceInfo {
    size_t pieceIndex;
    size_t offsetInPiece;
  };
  PieceInfo FindPiecePosition(size_t pos) const;

  // OPTIMIZATION: Line offset cache for O(1) line lookups
  mutable std::vector<size_t> m_lineOffsetCache;
  mutable bool m_lineCacheValid = false;

  void RebuildLineCache() const;

  // OPTIMIZATION: Piece table compaction tracking
  size_t m_editsSinceCompaction = 0;
};
