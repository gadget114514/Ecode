#include "PieceTable.h"
#include <algorithm>

CsvPieceTable::CsvPieceTable()
    : m_totalSize(0)
{
}

CsvPieceTable::~CsvPieceTable()
{
}

bool CsvPieceTable::LoadFromFile(const std::wstring& filePath)
{
    if (!m_file.Open(filePath)) {
        return false;
    }

    m_addBuffer.clear();
    m_pieces.clear();

    if (m_file.GetSize() > 0) {
        CsvPiece p;
        p.source = CsvPiece::ORIGINAL;
        p.offset = 0;
        p.length = m_file.GetSize();
        m_pieces.push_back(p);
    }

    m_totalSize = m_file.GetSize();
    return true;
}

void CsvPieceTable::SetPieces(const std::vector<CsvPiece>& pieces)
{
    m_pieces = pieces;
    m_totalSize = 0;
    for (const auto& p : m_pieces) {
        m_totalSize += p.length;
    }
}

bool CsvPieceTable::Save(const std::wstring& filePath)
{
    // For large files, we should stream this. 
    // Creating a new file and writing piece by piece.
    
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_WRITE,
        0, NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) return false;

    // TODO: Write buffer in chunks for speed? For now, piece by piece is okay.
    // If piece is huge, nice. If many small pieces, many WriteFile calls.
    // Buffering would be better.

    for (const auto& piece : m_pieces) {
        const uint8_t* dataStart = nullptr;
        if (piece.source == CsvPiece::ORIGINAL) {
            dataStart = m_file.GetData() + piece.offset;
        } else {
            dataStart = m_addBuffer.data() + piece.offset;
        }

        DWORD written = 0;
        // Handle >4GB writing in chunks if piece.length is huge? 
        // WriteFile takes DWORD (32-bit).
        uint64_t remaining = piece.length;
        uint64_t currentOffset = 0;

        while (remaining > 0) {
            DWORD toWrite = (remaining > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD)remaining;
            if (!WriteFile(hFile, dataStart + currentOffset, toWrite, &written, NULL)) {
                CloseHandle(hFile);
                return false;
            }
            remaining -= written;
            currentOffset += written;
        }
    }

    CloseHandle(hFile);
    return true;
}

uint8_t CsvPieceTable::GetAt(uint64_t index) const
{
    size_t pieceIndex;
    uint64_t relativeOffset;
    if (FindPiece(index, pieceIndex, relativeOffset)) {
        const CsvPiece& p = m_pieces[pieceIndex];
        if (p.source == CsvPiece::ORIGINAL) {
            return m_file.GetData()[p.offset + relativeOffset];
        } else {
            return m_addBuffer[p.offset + relativeOffset];
        }
    }
    return 0; // Out of bounds
}

uint64_t CsvPieceTable::GetSize() const
{
    return m_totalSize;
}

bool CsvPieceTable::FindPiece(uint64_t logicalOffset, size_t& outPieceIndex, uint64_t& outRelativeOffset) const
{
    if (logicalOffset >= m_totalSize) return false;

    uint64_t currentPos = 0;
    for (size_t i = 0; i < m_pieces.size(); ++i) {
        const CsvPiece& p = m_pieces[i];
        if (logicalOffset < currentPos + p.length) {
            outPieceIndex = i;
            outRelativeOffset = logicalOffset - currentPos;
            return true;
        }
        currentPos += p.length;
    }
    return false;
}

void CsvPieceTable::Insert(uint64_t offset, const uint8_t* data, size_t length)
{
    if (length == 0) return;

    // Append new data to AddBuffer
    uint64_t addBufferOffset = m_addBuffer.size();
    m_addBuffer.insert(m_addBuffer.end(), data, data + length);

    // Create the new piece
    CsvPiece newPiece;
    newPiece.source = CsvPiece::ADD_BUFFER;
    newPiece.offset = addBufferOffset;
    newPiece.length = length;

    if (offset >= m_totalSize) {
        // Append at end
        m_pieces.push_back(newPiece);
    } else {
        size_t pieceIndex;
        uint64_t relativeOffset;
        FindPiece(offset, pieceIndex, relativeOffset);

        CsvPiece& targetPiece = m_pieces[pieceIndex];

        if (relativeOffset == 0) {
            // Insert before this piece
            m_pieces.insert(m_pieces.begin() + pieceIndex, newPiece);
        } else {
            // Split the piece
            CsvPiece rightPiece = targetPiece;
            rightPiece.offset += relativeOffset;
            rightPiece.length -= relativeOffset;

            targetPiece.length = relativeOffset; // Left part stays in place

            // Insert newPiece and rightPiece
            m_pieces.insert(m_pieces.begin() + pieceIndex + 1, newPiece);
            m_pieces.insert(m_pieces.begin() + pieceIndex + 2, rightPiece);
        }
    }

    m_totalSize += length;
}

void CsvPieceTable::Delete(uint64_t offset, uint64_t length)
{
    if (length == 0 || offset >= m_totalSize) return;

    uint64_t endDelete = offset + length;
    if (endDelete > m_totalSize) endDelete = m_totalSize;
    uint64_t actualDeleteLength = endDelete - offset;

    // 1. Find start piece
    size_t startPieceIndex;
    uint64_t startRelOffset;
    if (!FindPiece(offset, startPieceIndex, startRelOffset)) return;

    // 2. Find end piece
    size_t endPieceIndex;
    uint64_t endRelOffset;
    
    // Special case: if deletion goes to the very end
    if (endDelete == m_totalSize) {
        endPieceIndex = m_pieces.size(); 
        endRelOffset = 0;
    } else {
        FindPiece(endDelete, endPieceIndex, endRelOffset);
    }

    // We will construct a new list of pieces
    // Any piece entirely within [offset, endDelete) is removed.
    // The piece at startPieceIndex might need splitting (keep left).
    // The piece at endPieceIndex might need splitting (keep right).

    // If start and end fall in the SAME piece
    if (startPieceIndex == endPieceIndex) {
        CsvPiece& p = m_pieces[startPieceIndex];
        
        CsvPiece right = p;
        right.offset += endRelOffset;
        right.length -= endRelOffset;

        p.length = startRelOffset; // Truncate left

        if (right.length > 0) {
            m_pieces.insert(m_pieces.begin() + startPieceIndex + 1, right);
        }
    } 
    else {
        // Handle Start Piece
        if (startRelOffset > 0) {
            // Keep left part
            m_pieces[startPieceIndex].length = startRelOffset;
            startPieceIndex++; // The ones to delete start AFTER this
        } else {
            // Remove this piece entirely, so include it in deletion range
        }

        // Handle End Piece
        // If endRelOffset > 0, it means the cut is in the middle of endPiece.
        // We need to keep the RIGHT part of endPiece.
        // But startPieceIndex...endPieceIndex are the pieces touched.
        // Actually, let's look at the range of pieces to REMOVE.
        
        // It's easier to handle split of end piece first?
        // If endPieceIndex < m_pieces.size(), we are entering it at endRelOffset.
        // We need to keep from endRelOffset to end.
        
        if (endPieceIndex < m_pieces.size()) {
            CsvPiece& endP = m_pieces[endPieceIndex];
            if (endRelOffset > 0) {
                 endP.offset += endRelOffset;
                 endP.length -= endRelOffset;
                 // This piece stays!
            } else {
                 // endRelOffset == 0 means we delete up to the START of this piece,
                 // so this piece is NOT touched/removed. (It's outside the range)
            }
        }
        
        // Remove pieces between startPieceIndex and endPieceIndex
        // Wait, indices map to the ORIGINAL array.
        // If we modified startPieceIndex (truncated it), we incremented startPieceIndex.
        // If we modified endPieceIndex (truncated left), effectively we just kept it but modified props.
        
        // Range to erase: [startPieceIndex, endPieceIndex) 
        // Logic check:
        // Case: Delete [10, 20). 
        // Piece 0: [0, 100). 
        // Start: idx 0, rel 10. Split -> P0[0, 10]. New P1[20, 80] inserted? 
        // My same-piece logic above handles this.
        
        // Case: Delete [10, 150).
        // Piece 0: [0, 100). Piece 1: [0, 100).
        // Start: idx 0, rel 10. Keep P0[0,10]. Increment startIdx -> 1.
        // End: idx 1, rel 50. Delete P1[0, 50). Keep P1[50, 50).
        // P1 modified to start+50, len 50.
        // Range to remove: pieces completely inside.
        
        // Implementation:
        // 1. If startRelOffset > 0, we split start piece. The "deletion" starts at startPieceIndex + 1.
        //    Else, deletion starts at startPieceIndex.
        size_t firstToRemove = (startRelOffset > 0) ? startPieceIndex + 1 : startPieceIndex;
        
        // 2. If endRelOffset == 0, we stop BEFORE endPieceIndex.
        //    If endRelOffset > 0, we must adjust endPieceIndex logic.
        //    Actually, if endRelOffset > 0, we are "keeping" the tail of endPieceIndex. 
        //    So we don't remove endPieceIndex, we just modify it.
        //    So we remove up to endPieceIndex.
        size_t lastToRemove = endPieceIndex; // Exclusive?
        
        if (endPieceIndex < m_pieces.size() && endRelOffset > 0) {
            // We need to keep the tail of this piece.
            CsvPiece& p = m_pieces[endPieceIndex];
            p.offset += endRelOffset;
            p.length -= endRelOffset;
            // It stays.
        } else {
            // If endRelOffset == 0, endPieceIndex is the first one *after* deletion. 
            // So we remove up to endPieceIndex. Correct.
        }
        
        if (firstToRemove < lastToRemove) {
             m_pieces.erase(m_pieces.begin() + firstToRemove, m_pieces.begin() + lastToRemove);
        }
    }

    m_totalSize -= actualDeleteLength;
}
