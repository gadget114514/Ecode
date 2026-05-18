#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include "MemoryMappedFile.h"

struct CsvPiece {
    enum Source { ORIGINAL, ADD_BUFFER };
    Source source;
    uint64_t offset;
    uint64_t length;
};

class CsvPieceTable {
public:
    CsvPieceTable();
    ~CsvPieceTable();

    // Initialize with a file (read-only mode for now)
    bool LoadFromFile(const std::wstring& filePath);
    bool Save(const std::wstring& filePath);

    // Basic operations
    void Insert(uint64_t offset, const uint8_t* data, size_t length);
    void Delete(uint64_t offset, uint64_t length);

    // Data Access
    // Get byte at index (slow, for testing/small access)
    uint8_t GetAt(uint64_t index) const;
    
    // Get total size of the content
    uint64_t GetSize() const;

    // Direct access to pieces for line indexing
    const std::vector<CsvPiece>& GetPieces() const { return m_pieces; }
    void SetPieces(const std::vector<CsvPiece>& pieces);
    const CsvMemoryMappedFile& GetOriginalFile() const { return m_file; }
    const std::vector<uint8_t>& GetAddBuffer() const { return m_addBuffer; }

private:
    CsvMemoryMappedFile m_file;
    std::vector<uint8_t> m_addBuffer;
    std::vector<CsvPiece> m_pieces;
    uint64_t m_totalSize;

    // Helper to find which piece contains the logical offset
    // Returns index in m_pieces and the relative offset within that piece
    bool FindPiece(uint64_t logicalOffset, size_t& outPieceIndex, uint64_t& outRelativeOffset) const;
};
