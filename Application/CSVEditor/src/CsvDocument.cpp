#include "CsvDocument.h"
#include <iostream>
#include <regex>

CsvDocument::CsvDocument()
{
}

CsvDocument::~CsvDocument()
{
}

// Column Operations
// Column Operations
bool CsvDocument::Load(const std::wstring& filePath, std::function<void(float)> progressCallback)
{
    if (!m_pieceTable.LoadFromFile(filePath)) return false;
    
    m_rowOffsets.clear();
    DetectEncoding();
    DetectLineEnding();
    
    RebuildRowIndex(progressCallback);
    
    return true;
}

bool CsvDocument::Import(const std::wstring& filePath)
{
    // Load as a separate doc to parse
    CsvDocument importDoc;
    if (!importDoc.Load(filePath)) return false;
    
    // Copy rows
    // Snapshot first for Undo
    Snapshot();
    
    size_t rowCount = importDoc.GetRowCount();
    size_t insertAt = GetRowCount();
    
    for (size_t i = 0; i < rowCount; ++i) {
        std::vector<std::wstring> cells = importDoc.GetRowCells(i);
        InsertRow(insertAt + i, cells);
    }
    
    return true;
}

void CsvDocument::DetectEncoding()
{
    size_t size = m_pieceTable.GetSize();
    if (size >= 3) {
        // UTF-8 BOM: EF BB BF
        if (m_pieceTable.GetAt(0) == 0xEF && m_pieceTable.GetAt(1) == 0xBB && m_pieceTable.GetAt(2) == 0xBF) {
            m_encoding = CsvFileEncoding::UTF8;
            // Should we skip BOM in indexing? Usually yes.
            // But CsvPieceTable is raw. We handle BOM skipping in iterators or just ignore it.
            // For now, let's keep it simple: Encoding detected, rest is handled.
            return;
        }
    }
    if (size >= 2) {
        // UTF-16 LE BOM: FF FE
        if (m_pieceTable.GetAt(0) == 0xFF && m_pieceTable.GetAt(1) == 0xFE) {
            m_encoding = CsvFileEncoding::UTF16_LE;
            return;
        }
        // UTF-16 BE BOM: FE FF
        if (m_pieceTable.GetAt(0) == 0xFE && m_pieceTable.GetAt(1) == 0xFF) {
            m_encoding = CsvFileEncoding::UTF16_BE;
            return;
        }
    }
    
    // Default to UTF-8 (or check logic for validity)
    m_encoding = CsvFileEncoding::UTF8;
}

void CsvDocument::DetectLineEnding()
{
    // Scan up to 4KB or so
    size_t scanSize = (std::min)(m_pieceTable.GetSize(), (size_t)4096);
    if (scanSize == 0) return;
    
    int crlfCount = 0;
    int lfCount = 0;
    
    // Naive scan usually works. 
    // For UTF-16, \n is 0A 00 (LE) or 00 0A (BE).
    
    for (size_t i = 0; i < scanSize; ++i) {
        uint8_t c = m_pieceTable.GetAt(i);
        
        if (m_encoding == CsvFileEncoding::UTF16_LE) {
            // Check for 0x0A at even or odd? 
            // LE: 0A 00. i should be even.
            // Check pair
            if (i + 1 < scanSize) {
                 uint16_t val = c | (m_pieceTable.GetAt(i+1) << 8);
                 if (val == 0x000A) { // LF
                     lfCount++;
                     // Check previous for CR (000D)
                     if (i >= 2) {
                         uint16_t prev = m_pieceTable.GetAt(i-2) | (m_pieceTable.GetAt(i-1) << 8);
                         if (prev == 0x000D) crlfCount++;
                     }
                 }
                 i++; // Skip byte
            }
        } 
        else if (m_encoding == CsvFileEncoding::UTF16_BE) {
             if (i + 1 < scanSize) {
                 uint16_t val = (c << 8) | m_pieceTable.GetAt(i+1);
                 if (val == 0x000A) {
                     lfCount++;
                     if (i >= 2) {
                         uint16_t prev = (m_pieceTable.GetAt(i-2) << 8) | m_pieceTable.GetAt(i-1);
                         if (prev == 0x000D) crlfCount++;
                     }
                 }
                 i++;
            }
        }
        else {
             // UTF8 / ANSI
             if (c == '\n') {
                 lfCount++;
                 if (i > 0 && m_pieceTable.GetAt(i-1) == '\r') {
                     crlfCount++;
                 }
             }
        }
    }
    
    if (lfCount > 0 && crlfCount == lfCount) {
        m_lineEnding = CsvLineEnding::CRLF;
    } else {
        m_lineEnding = CsvLineEnding::LF; // Default to LF if mixed or pure LF
    }
}

std::wstring CsvDocument::GetLineEndingStr() const
{
    if (m_lineEnding == CsvLineEnding::CRLF) return L"\r\n";
    return L"\n";
}

std::vector<uint8_t> CsvDocument::EncodeString(const std::wstring& str)
{
    std::vector<uint8_t> result;
    if (m_encoding == CsvFileEncoding::UTF8) {
        int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0, NULL, NULL);
        if (len > 0) {
            result.resize(len);
            WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), (char*)result.data(), len, NULL, NULL);
        }
    } else if (m_encoding == CsvFileEncoding::UTF16_LE) {
        result.resize(str.length() * 2);
        memcpy(result.data(), str.c_str(), str.length() * 2);
    } else if (m_encoding == CsvFileEncoding::UTF16_BE) {
        result.resize(str.length() * 2);
        for (size_t i = 0; i < str.length(); ++i) {
            uint16_t ch = static_cast<uint16_t>(str[i]);
            result[i*2] = (ch >> 8) & 0xFF;
            result[i*2+1] = ch & 0xFF;
        }
    } else {
        // ANSI
        int len = WideCharToMultiByte(CP_ACP, 0, str.c_str(), (int)str.length(), NULL, 0, NULL, NULL);
        if (len > 0) {
            result.resize(len);
            WideCharToMultiByte(CP_ACP, 0, str.c_str(), (int)str.length(), (char*)result.data(), len, NULL, NULL);
        }
    }
    return result;
}

void CsvDocument::InsertColumn(size_t colIndex, const std::wstring& defaultValue)
{
    Snapshot(); // Save state

    int64_t shift = 0;
    size_t rows = GetRowCount();
    
    // We iterate all rows and modify them one by one.
    // This shifts offsets for subsequent rows.
    
    for (size_t r = 0; r < rows; ++r) {
        uint64_t start = m_rowOffsets[r] + shift;
        
        uint64_t oldLen = 0;
        if (r + 1 < rows) oldLen = m_rowOffsets[r+1] - m_rowOffsets[r];
        else oldLen = m_pieceTable.GetSize() - start;
        
        // Read raw row
        std::vector<uint8_t> rawPos(oldLen);
        if (oldLen > 0) {
            for(size_t i=0; i<oldLen; ++i) {
                rawPos[i] = m_pieceTable.GetAt(start + i);
            }
        }
        
        std::wstring rowText = DecodeString(rawPos);
        auto cells = ParseRowCells(rowText);
        
        // Modify
        if (colIndex <= cells.size()) {
             cells.insert(cells.begin() + colIndex, defaultValue);
        } else {
             while(cells.size() < colIndex) cells.push_back(L"");
             cells.push_back(defaultValue);
        }
        
        std::wstring newRowStr = ConstructRowString(cells);
        newRowStr += GetLineEndingStr(); 
        
        // Encode
        std::vector<uint8_t> bytes = EncodeString(newRowStr);
        
        // Apply Edit
        m_pieceTable.Delete(start, oldLen);
        m_pieceTable.Insert(start, (const uint8_t*)bytes.data(), bytes.size());
        
        shift += (int64_t)bytes.size() - (int64_t)oldLen;
    }
    
    RebuildRowIndex();
}

void CsvDocument::DeleteColumn(size_t colIndex)
{
    Snapshot();
    int64_t shift = 0;
    size_t rows = GetRowCount();
    
    for (size_t r = 0; r < rows; ++r) {
        uint64_t start = m_rowOffsets[r] + shift;
        
        uint64_t oldLen = 0;
        if (r + 1 < rows) oldLen = m_rowOffsets[r+1] - m_rowOffsets[r];
        else oldLen = m_pieceTable.GetSize() - start;
        
        std::vector<uint8_t> rawPos(oldLen);
        if (oldLen > 0) {
            for(size_t i=0; i<oldLen; ++i) {
                rawPos[i] = m_pieceTable.GetAt(start + i);
            }
        }
        
        std::wstring rowText = DecodeString(rawPos);
        auto cells = ParseRowCells(rowText);
        
        if (colIndex < cells.size()) {
             cells.erase(cells.begin() + colIndex);
             
            std::wstring newRowStr = ConstructRowString(cells);
            newRowStr += GetLineEndingStr(); 
            
            // Encode
            std::vector<uint8_t> bytes = EncodeString(newRowStr);
            
            m_pieceTable.Delete(start, oldLen);
            m_pieceTable.Insert(start, (const uint8_t*)bytes.data(), bytes.size());
            
            shift += (int64_t)bytes.size() - (int64_t)oldLen;
        }
    }
    RebuildRowIndex();
}

std::vector<std::wstring> CsvDocument::ParseRowCells(const std::wstring& rowText)
{
    std::vector<std::wstring> cells;
    bool inQuotes = false;
    std::wstring currentCell;
    
    for (size_t i = 0; i < rowText.size(); ++i) {
        wchar_t c = rowText[i];
        if (inQuotes) {
            if (c == L'\"') {
                if (i + 1 < rowText.size() && rowText[i + 1] == L'\"') {
                    currentCell += L'\"'; i++;
                } else { inQuotes = false; }
            } else { currentCell += c; }
        } else {
            if (c == L'\"') { inQuotes = true; }
            else if (c == m_delimiter) { cells.push_back(currentCell); currentCell.clear(); }
            else if (c == L'\r' || c == L'\n') { /* ignore */ } 
            else { currentCell += c; }
        }
    }
    cells.push_back(currentCell);
    return cells;
}

std::wstring CsvDocument::ConstructRowString(const std::vector<std::wstring>& cells)
{
    std::wstring newRowStr;
    for (size_t i = 0; i < cells.size(); ++i) {
        bool needsQuotes = false;
        if (cells[i].find(m_delimiter) != std::wstring::npos || 
            cells[i].find(L'\"') != std::wstring::npos ||
            cells[i].find(L'\n') != std::wstring::npos) {
            needsQuotes = true;
        }
        
        if (needsQuotes) {
            newRowStr += L'\"';
            for (wchar_t c : cells[i]) {
                if (c == L'\"') newRowStr += L"\"\"";
                else newRowStr += c;
            }
            newRowStr += L'\"';
        } else {
            newRowStr += cells[i];
        }
        
        if (i < cells.size() - 1) newRowStr += m_delimiter;
    }
    return newRowStr;
}

bool CsvDocument::Save(const std::wstring& filePath)
{
    // In future: might need to handle encoding conversion here if we supported converting ON SAVE.
    // Use current encoding -> implies CsvPieceTable stores raw bytes of that encoding.
    return m_pieceTable.Save(filePath);
}

size_t CsvDocument::GetRowCount() const
{
    // The last virtual row might be empty if file ends with newline, 
    // but usually we count it.
    return m_rowOffsets.size();
}

uint64_t CsvDocument::GetRowStartOffset(size_t rowIndex) const
{
    if (rowIndex >= m_rowOffsets.size()) return m_pieceTable.GetSize();
    return m_rowOffsets[rowIndex];
}

void CsvDocument::RebuildRowIndex(std::function<void(float)> progressCallback)
{
    m_rowOffsets.clear();
    
    if (m_pieceTable.GetSize() == 0) {
        if (progressCallback) progressCallback(1.0f);
        return; 
    }

    m_rowOffsets.push_back(0); // First row always starts at 0

    // Iterate through all pieces
    const auto& pieces = m_pieceTable.GetPieces();
    const auto& file = m_pieceTable.GetOriginalFile();
    const auto& addBuf = m_pieceTable.GetAddBuffer();
    
    uint64_t currentLogicalOffset = 0;
    bool inQuotes = false;
    
    // Quick helpers
    auto is_newline = [&](uint16_t ch) { return ch == L'\n'; };
    auto is_quote = [&](uint16_t ch) { return ch == L'\"'; };

    uint64_t totalBytes = m_pieceTable.GetSize(); // Approx
    uint64_t processedBytes = 0;
    const uint64_t reportInterval = 1024 * 1024; // Report every 1MB
    uint64_t nextReport = reportInterval;

    for (const auto& piece : pieces) {
        const uint8_t* data = (piece.source == CsvPiece::ORIGINAL) 
            ? file.GetData() + piece.offset
            : addBuf.data() + piece.offset;
        
        uint64_t len = piece.length;

        // Progress check helper
        auto check_progress = [&](uint64_t currentPieceIdx) {
            processedBytes += currentPieceIdx; 
            if (processedBytes >= nextReport) {
                if (progressCallback) progressCallback((float)processedBytes / totalBytes);
                nextReport += reportInterval;
            }
            processedBytes -= currentPieceIdx; // Reset for loop
        };
        // Simplified progress: just report inside loop logic or after piece?
        // Pieces can be large (whole file). Need inside loop.

        if (m_encoding == CsvFileEncoding::UTF16_LE) {
            for (uint64_t i = 0; i + 1 < len; i += 2) {
                // Progress
                if (progressCallback && (processedBytes + i) >= nextReport) {
                    progressCallback((float)(processedBytes + i) / totalBytes);
                    nextReport += reportInterval;
                }
                
                uint16_t ch = data[i] | (data[i+1] << 8);
                if (is_quote(ch)) {
                    inQuotes = !inQuotes;
                } else if (is_newline(ch)) {
                    if (!inQuotes) {
                        m_rowOffsets.push_back(currentLogicalOffset + i + 2);
                    }
                }
            }
        } else if (m_encoding == CsvFileEncoding::UTF16_BE) {
            for (uint64_t i = 0; i + 1 < len; i += 2) {
                 if (progressCallback && (processedBytes + i) >= nextReport) {
                    progressCallback((float)(processedBytes + i) / totalBytes);
                    nextReport += reportInterval;
                }

                uint16_t ch = (data[i] << 8) | data[i+1];
                if (is_quote(ch)) {
                    inQuotes = !inQuotes;
                } else if (is_newline(ch)) {
                    if (!inQuotes) {
                        m_rowOffsets.push_back(currentLogicalOffset + i + 2);
                    }
                }
            }
        } else {
            // UTF8 / ANSI
            for (uint64_t i = 0; i < len; ++i) {
                 if (progressCallback && (processedBytes + i) >= nextReport) {
                    progressCallback((float)(processedBytes + i) / totalBytes);
                    nextReport += reportInterval;
                }

                uint8_t b = data[i];
                if (b == '\"') {
                    inQuotes = !inQuotes;
                }
                else if (b == '\n') {
                    if (!inQuotes) {
                        m_rowOffsets.push_back(currentLogicalOffset + i + 1);
                    }
                }
            }
        }
        currentLogicalOffset += len;
        processedBytes += len;
    }
    
    // Handle trailing newline/empty row logic
    if (m_rowOffsets.size() > 1 && m_rowOffsets.back() == m_pieceTable.GetSize()) {
        m_rowOffsets.pop_back();
    }
    
    if (progressCallback) progressCallback(1.0f);
}

std::vector<uint8_t> CsvDocument::GetRowRaw(size_t rowIndex)
{
    std::vector<uint8_t> result;
    if (rowIndex >= m_rowOffsets.size()) return result;

    uint64_t start = m_rowOffsets[rowIndex];
    uint64_t end = (rowIndex + 1 < m_rowOffsets.size()) ? m_rowOffsets[rowIndex + 1] : m_pieceTable.GetSize();
    
    // Exclude the newline char(s) from the row data? Usually yes.
    // Check if previous char was \r if we are at \n (handled in parsing logic usually)
    
    if (end > start) {
        uint64_t dataLen = end - start;
        // Fetch byte by byte is slow, optimization needed: FetchChunk
        // For now, simple loop
        for (uint64_t i = 0; i < dataLen; ++i) {
            result.push_back(m_pieceTable.GetAt(start + i));
        }

        // Check for newline removal based on encoding
        if (m_encoding == CsvFileEncoding::UTF8 || m_encoding == CsvFileEncoding::ANSI) {
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
                if (!result.empty() && result.back() == '\r') result.pop_back();
            }
        } else if (m_encoding == CsvFileEncoding::UTF16_LE) {
            // Newline is 0A 00
            if (result.size() >= 2 && result[result.size()-2] == 0x0A && result[result.size()-1] == 0x00) {
                result.pop_back(); result.pop_back();
                // Check CR: 0D 00
                if (result.size() >= 2 && result[result.size()-2] == 0x0D && result[result.size()-1] == 0x00) {
                     result.pop_back(); result.pop_back();
                }
            }
        } else if (m_encoding == CsvFileEncoding::UTF16_BE) {
            // Newline is 00 0A
            if (result.size() >= 2 && result[result.size()-2] == 0x00 && result[result.size()-1] == 0x0A) {
                result.pop_back(); result.pop_back();
                // Check CR: 00 0D
                if (result.size() >= 2 && result[result.size()-2] == 0x00 && result[result.size()-1] == 0x0D) {
                     result.pop_back(); result.pop_back();
                }
            }
        }
    }
    
    return result;
}

void CsvDocument::SetDelimiter(wchar_t delimiter)
{
    m_delimiter = delimiter;
}

void CsvDocument::SetEncoding(CsvFileEncoding encoding)
{
    m_encoding = encoding;
}

std::vector<std::wstring> CsvDocument::GetRowCells(size_t rowIndex)
{
    std::vector<std::wstring> cells;
    std::vector<uint8_t> rawRow = GetRowRaw(rowIndex);
    std::wstring rowText = DecodeString(rawRow);

    // Basic CSV parsing state machine
    bool inQuotes = false;
    std::wstring currentCell;
    
    for (size_t i = 0; i < rowText.size(); ++i) {
        wchar_t c = rowText[i];
        
        if (inQuotes) {
            if (c == L'\"') {
                if (i + 1 < rowText.size() && rowText[i + 1] == L'\"') {
                    // Escaped quote
                    currentCell += L'\"';
                    i++;
                } else {
                    inQuotes = false;
                }
            } else {
                currentCell += c;
            }
        } else {
            if (c == L'\"') {
                inQuotes = true;
            } else if (c == m_delimiter) {
                cells.push_back(currentCell);
                currentCell.clear();
            } else {
                // If we hit \r or \n here, it's end of row (but usually GetRowRaw strips them?)
                // Actually GetRowRaw might strip the *row delimiter*, but not in-cell newlines if logic was better.
                // Current `GetRowRaw` relies on `RebuildRowIndex` which splits on ANY \n.
                // This is a flaw: `RebuildRowIndex` needs to respect quotes to support newlines in cells.
                // For this step, I will stick to simple splitting. 
                // Advanced parsing requires `RebuildRowIndex` to change.
                currentCell += c;
            }
        }
    }
    cells.push_back(currentCell);
    
    return cells;
}

void CsvDocument::DeleteRow(size_t rowIndex)
{
    if (rowIndex >= m_rowOffsets.size()) return;

    uint64_t startOffset = m_rowOffsets[rowIndex];
    uint64_t endOffset = 0;
    
    if (rowIndex + 1 < m_rowOffsets.size()) {
        endOffset = m_rowOffsets[rowIndex + 1];
    } else {
        // Last row, delete until end
        endOffset = m_pieceTable.GetSize();
    }
    
    uint64_t length = endOffset - startOffset;
    if (length > 0) {
        m_pieceTable.Delete(startOffset, length);
        RebuildRowIndex(); // Re-index after structural change
    }
}

void CsvDocument::UpdateCell(size_t row, size_t col, const std::wstring& value)
{
    // 1. Get current row content
    // This is expensive: we need to parse the row again to find the cell's byte range
    // Then delete that range, insert new value (encoded).
    
    // Simplification for prototype:
    // Reconstruct the ENTIRE row with the new cell value and replace the whole row.
    
    auto cells = GetRowCells(row);
    if (col >= cells.size()) {
        // Pad with empty cells?
        while (cells.size() <= col) cells.push_back(L"");
    }
    
    cells[col] = value;
    
    // Reconstruct row string
    std::wstring newRowStr;
    for (size_t i = 0; i < cells.size(); ++i) {
        bool needsQuotes = false;
        // Check if needs quotes (contains delimiter, quote, or newline)
        if (cells[i].find(m_delimiter) != std::wstring::npos || 
            cells[i].find(L'"') != std::wstring::npos ||
            cells[i].find(L'\n') != std::wstring::npos) {
            needsQuotes = true;
        }
        
        if (needsQuotes) {
            newRowStr += L'"';
            // Escape quotes
            std::wstring& cell = cells[i];
            for (wchar_t c : cell) {
                if (c == L'"') newRowStr += L"\"\"";
                else newRowStr += c;
            }
            newRowStr += L'"';
        } else {
            newRowStr += cells[i];
        }
        
        if (i < cells.size() - 1) newRowStr += m_delimiter;
    }
    
    // Add newline
    newRowStr += GetLineEndingStr(); 

    // Encode to bytes
    // Simple UTF8 for now
    std::string bytes;
    if (!newRowStr.empty()) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, newRowStr.c_str(), (int)newRowStr.length(), NULL, 0, NULL, NULL);
        bytes.resize(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, newRowStr.c_str(), (int)newRowStr.length(), &bytes[0], size_needed, NULL, NULL);
    }
    
    // Replace old row
    uint64_t startOffset = m_rowOffsets[row];
    uint64_t endOffset = (row + 1 < m_rowOffsets.size()) ? m_rowOffsets[row + 1] : m_pieceTable.GetSize();
    
    m_pieceTable.Delete(startOffset, endOffset - startOffset);
    m_pieceTable.Insert(startOffset, (const uint8_t*)bytes.data(), bytes.size());
    
    RebuildRowIndex();
}

void CsvDocument::InsertRow(size_t rowIndex, const std::vector<std::wstring>& values)
{
    // Calculate Offset first to check if we need to prepend newline
    uint64_t insertOffset = 0;
    bool needsPrependNewline = false;
    
    if (rowIndex < m_rowOffsets.size()) {
        insertOffset = m_rowOffsets[rowIndex];
    } else {
        insertOffset = m_pieceTable.GetSize();
        if (insertOffset > 0) {
            uint8_t lastChar = m_pieceTable.GetAt(insertOffset - 1);
            if (lastChar != '\n') {
                needsPrependNewline = true;
            }
        }
    }

    // Construct row string
    std::wstring newRowStr;
    
    if (needsPrependNewline) {
        newRowStr += GetLineEndingStr();
    }

    for (size_t i = 0; i < values.size(); ++i) {
        bool needsQuotes = false;
        if (values[i].find(m_delimiter) != std::wstring::npos || 
            values[i].find(L'"') != std::wstring::npos ||
            values[i].find(L'\n') != std::wstring::npos) {
            needsQuotes = true;
        }
        
        if (needsQuotes) {
            newRowStr += L'"';
            std::wstring val = values[i];
            for (wchar_t c : val) {
                if (c == L'"') newRowStr += L"\"\"";
                else newRowStr += c;
            }
            newRowStr += L'"';
        } else {
            newRowStr += values[i];
        }
        
        if (i < values.size() - 1) newRowStr += m_delimiter;
    }
    
    // Always append newline at end of inserted row (standard CSV row behavior)
    // If we inserted IN BETWEEN, the previous row ends with \n. We insert "RowContent\n".
    // Next row starts after our \n. Correct.
    newRowStr += GetLineEndingStr(); 
    
    // Encode
    std::string bytes;
    if (!newRowStr.empty()) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, newRowStr.c_str(), (int)newRowStr.length(), NULL, 0, NULL, NULL);
        bytes.resize(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, newRowStr.c_str(), (int)newRowStr.length(), &bytes[0], size_needed, NULL, NULL);
    }
    
    Snapshot(); // Save before modification

    m_pieceTable.Insert(insertOffset, (const uint8_t*)bytes.data(), bytes.size());
    RebuildRowIndex();
}

void CsvDocument::Snapshot()
{
    HistoryState state;
    state.pieces = m_pieceTable.GetPieces();
    m_undoStack.push_back(state);
    
    // Clear redo stack on new action
    m_redoStack.clear();
    
    // Cap history size? For now unlimited or 100
    if (m_undoStack.size() > 100) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

void CsvDocument::Undo()
{
    if (m_undoStack.empty()) return;
    
    // Save current to Redo
    HistoryState current;
    current.pieces = m_pieceTable.GetPieces();
    m_redoStack.push_back(current);
    
    // Restore
    HistoryState match = m_undoStack.back();
    m_undoStack.pop_back();
    
    m_pieceTable.SetPieces(match.pieces);
    RebuildRowIndex();
}

void CsvDocument::Redo()
{
    if (m_redoStack.empty()) return;
    
    // Save current to Undo
    HistoryState current;
    current.pieces = m_pieceTable.GetPieces();
    m_undoStack.push_back(current);
    
    // Restore
    HistoryState match = m_redoStack.back();
    m_redoStack.pop_back();
    
    m_pieceTable.SetPieces(match.pieces);
    RebuildRowIndex();
}

bool CsvDocument::CanUndo() const { return !m_undoStack.empty(); }
bool CsvDocument::CanRedo() const { return !m_redoStack.empty(); }


// Wait, that's literally what I had. If it failed, maybe I need to be more explicit.
// Let's use std::tolower from <locale> or just simple loop.
// Win32 approach:
// Helper for case-insensitive find (Manual loop to verify correct behavior)
static bool CaseInsensitiveFindWin32(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
           if (towlower(haystack[i+j]) != towlower(needle[j])) {
               match = false;
               break;
           }
        }
        if (match) return true;
    }
    return false;
}

static bool CaseInsensitiveExact(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for(size_t i=0; i<a.size(); ++i) {
        if(towlower(a[i]) != towlower(b[i])) return false;
    }
    return true;
}

bool CsvDocument::Search(const std::wstring& query, size_t& row, size_t& col, const SearchOptions& options)
{
    if (query.empty()) return false;
    
    size_t startRow = row;
    size_t startCol = col;
    size_t numRows = GetRowCount();
    
    // Prepare Regex if needed
    std::wregex regexPattern;
    if (options.mode == SearchMode::Regex) {
        try {
            std::regex_constants::syntax_option_type flags = std::regex_constants::icase; // Default icase?
            if (options.matchCase) flags = std::regex_constants::ECMAScript;
            else flags = std::regex_constants::ECMAScript | std::regex_constants::icase;
            
            regexPattern.assign(query, flags);
        } catch(...) {
            return false; // Invalid regex
        }
    }

    if (options.forward) {
        for (size_t r = startRow; r < numRows; ++r) {
            auto cells = GetRowCells(r);
            size_t c = (r == startRow) ? (options.includeStart ? startCol : startCol + 1) : 0;
            
            for (; c < cells.size(); ++c) {
                 bool found = false;
                 
                 if (options.mode == SearchMode::Contains) {
                     if (options.matchCase) {
                         if (cells[c].find(query) != std::wstring::npos) found = true;
                     } else {
                         if (CaseInsensitiveFindWin32(cells[c], query)) found = true;
                     }
                 } else if (options.mode == SearchMode::Exact) {
                     if (options.matchCase) found = (cells[c] == query);
                     else found = CaseInsensitiveExact(cells[c], query);
                 } else if (options.mode == SearchMode::Regex) {
                     if (std::regex_search(cells[c], regexPattern)) found = true;
                 }
                 
                 if (found) {
                     row = r;
                     col = c;
                     return true;
                 }
            }
        }
    } else {
        // Backward
        for (int r = (int)startRow; r >= 0; --r) {
            auto cells = GetRowCells(r);
            int startC = (int)startCol;
            int c = (r == (int)startRow) ? (options.includeStart ? startC : startC - 1) : (int)cells.size() - 1;
            
            for (; c >= 0; --c) {
                 bool found = false;
                 if (options.mode == SearchMode::Contains) {
                     if (options.matchCase) {
                         if (cells[c].find(query) != std::wstring::npos) found = true;
                     } else {
                         if (CaseInsensitiveFindWin32(cells[c], query)) found = true;
                     }
                 } else if (options.mode == SearchMode::Exact) {
                     if (options.matchCase) found = (cells[c] == query);
                     else found = CaseInsensitiveExact(cells[c], query);
                 } else if (options.mode == SearchMode::Regex) {
                     if (std::regex_search(cells[c], regexPattern)) found = true;
                 }
                 
                 if (found) {
                     row = r;
                     col = c;
                     return true;
                 }
            }
        }
    }
    
    return false;
}

bool CsvDocument::Replace(const std::wstring& query, const std::wstring& replacement, size_t& row, size_t& col, const SearchOptions& options)
{
    // 1. Verify match at row/col
    if (row >= GetRowCount()) return false;
    auto cells = GetRowCells(row);
    if (col >= cells.size()) return false;
    
    std::wstring& cellText = cells[col];
    bool found = false;
    
    // Prepare Regex
    std::wregex regexPattern;
    if (options.mode == SearchMode::Regex) {
        try {
            auto flags = options.matchCase ? std::regex_constants::ECMAScript : (std::regex_constants::ECMAScript | std::regex_constants::icase);
            regexPattern.assign(query, flags);
        } catch(...) { return false; }
    }

    // Check Match
    if (options.mode == SearchMode::Exact) {
         if (options.matchCase) found = (cellText == query);
         else found = CaseInsensitiveExact(cellText, query);
    } else if (options.mode == SearchMode::Contains) {
         if (options.matchCase) found = (cellText.find(query) != std::wstring::npos);
         else found = CaseInsensitiveFindWin32(cellText, query);
    } else if (options.mode == SearchMode::Regex) {
         found = std::regex_search(cellText, regexPattern);
    }
    
    if (!found) return false;
    
    // Perform Replacement
    std::wstring newText = cellText;
    if (options.mode == SearchMode::Exact) {
        newText = replacement;
    } else if (options.mode == SearchMode::Regex) {
        newText = std::regex_replace(cellText, regexPattern, replacement);
    } else { // Contains
        // Replace current occurrence? Or all in cell?
        // Usually "Replace" does "Replace All in Cell" or "Replace First"?
        // Standard is Replace All occurrences within the target string for that "Replace" action?
        // Let's do Replace All Occurrences WITHIN cell for 'Contains' mode.
        // Actually simpler: just find first and replace? 
        // Let's do ALL in cell logic for simplicity unless user highlights specific part.
        
        // Manual string replace
        // Note: Caseless replace is tricky. std::regex is easier.
        // If not using regex, implementing caseless replace is pain.
        // Let's use simple logic: if !CaseSensitive, use regex anyway? 
        // Or specific loop.
        
        if (options.matchCase) {
             size_t pos = 0;
             while ((pos = newText.find(query, pos)) != std::wstring::npos) {
                 newText.replace(pos, query.length(), replacement);
                 pos += replacement.length();
             }
        } else {
             // Use Regex for case-insensitive non-regex replace
             // Escape query for regex?
             // Too complex.
             // Let's assume naive single replace for now or use regex hack.
             // Actually, the user asked for "partly match".
             // Let's iterate.
             std::wstring lowerQuery = query;
             for(auto& c : lowerQuery) c = towlower(c);
             
             size_t pos = 0;
             // Naive iteration
             while (true) {
                 if (newText.size() < lowerQuery.size()) break;
                 size_t limit = newText.size() - lowerQuery.size();
                 if (pos > limit) break;

                 // Scan
                 size_t foundPos = std::wstring::npos;
                 for (size_t i = pos; i <= limit; ++i) {
                     bool m = true;
                     for(size_t j=0; j<lowerQuery.size(); ++j) {
                         if(towlower(newText[i+j]) != lowerQuery[j]) { m = false; break; }
                     }
                     if (m) { foundPos = i; break; }
                 }
                 
                 if (foundPos == std::wstring::npos) break;
                 newText.replace(foundPos, query.length(), replacement);
                 pos = foundPos + replacement.length();
                 // if (pos > newText.size()) break; // Handled by limit check next iter
             }
        }
    }
    
    if (newText != cellText) {
        UpdateCell(row, col, newText);
        return true;
    }
    return false;
}

int CsvDocument::ReplaceAll(const std::wstring& query, const std::wstring& replacement, const SearchOptions& options)
{
    int count = 0;
    size_t rows = GetRowCount();
    
    // Iterate ALL cells.
    for (size_t r = 0; r < rows; ++r) {
        auto cells = GetRowCells(r);
        bool rowMod = false;
        
        for (size_t c = 0; c < cells.size(); ++c) {
             size_t tempR = r, tempC = c;
             // Temporarily use Replace logic per cell
             // Optimize: Check match first
             if (Replace(query, replacement, tempR, tempC, options)) {
                 count++;
                 // Update local cells copy if we want to continue in same row?
                 // Replace() calls UpdateCell() which Snapshot()s and modifies CsvPieceTable.
                 // This is VERY SLOW for ReplaceAll (one snapshot per cell).
                 // Ideally: Batch edits.
                 // Current limitation: One undo per ReplaceAll call?
                 // Or just let it be slow for now. ReplaceAll implementation in CsvPieceTable is better.
                 // For now, naive loop.
             }
        }
    }
    return count;
}

std::wstring CsvDocument::GetRangeAsText(size_t startRow, size_t startCol, size_t endRow, size_t endCol)
{
    std::wstring result;
    
    for (size_t r = startRow; r <= endRow && r < m_rowOffsets.size(); ++r) {
        auto cells = GetRowCells(r);
        
        for (size_t c = startCol; c <= endCol; ++c) {
            if (c < cells.size()) {
                // Determine if we need quoting for clipboard?
                // Excel puts TSV on clipboard usually.
                // Let's use Tab (\t) delimiter for clipboard by default as it works best with Excel.
                
                std::wstring cell = cells[c];
                bool needsQuotes = false;
                if (cell.find(L'\t') != std::wstring::npos || cell.find(L'\n') != std::wstring::npos || cell.find(L'"') != std::wstring::npos) {
                    needsQuotes = true;
                }
                
                if (needsQuotes) {
                    result += L'"';
                    for (wchar_t ch : cell) {
                        if (ch == L'"') result += L"\"\"";
                        else result += ch;
                    }
                    result += L'"';
                } else {
                    result += cell;
                }
            }
            
            if (c < endCol) result += L'\t'; // TSV
        }
        
        if (r < endRow) result += L"\r\n";
    }
    
    return result;
}

std::wstring CsvDocument::DecodeString(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty()) return L"";

    if (m_encoding == CsvFileEncoding::UTF8) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(bytes.data()), (int)bytes.size(), NULL, 0);
        if (wlen == 0) return L"";
        
        std::wstring result(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(bytes.data()), (int)bytes.size(), &result[0], wlen);
        return result;
    } 
    else if (m_encoding == CsvFileEncoding::UTF16_LE) {
        if (bytes.size() % 2 != 0) {
            // Invalid stream size, maybe pad or ignore last byte?
        }
        size_t wlen = bytes.size() / 2;
        std::wstring result(wlen, 0);
        memcpy(&result[0], bytes.data(), wlen * sizeof(wchar_t));
        return result;
    }
    else if (m_encoding == CsvFileEncoding::UTF16_BE) {
        size_t wlen = bytes.size() / 2;
        std::wstring result(wlen, 0);
        for(size_t i=0; i<wlen; ++i) {
            uint16_t b1 = bytes[i*2];
            uint16_t b2 = bytes[i*2+1];
            // BE: b1 is high byte
            wchar_t ch = (b1 << 8) | b2;
            result[i] = ch;
        }
        return result;
    }

    // Fallback to ANSI
    int wlen = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(bytes.data()), (int)bytes.size(), NULL, 0);
    if (wlen == 0) return L"";
    std::wstring result(wlen, 0);
    MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(bytes.data()), (int)bytes.size(), &result[0], wlen);
    return result;
}

void CsvDocument::PasteCells(size_t startRow, size_t startCol, const std::wstring& text)
{
    Snapshot(); // Save state

    if (text.empty()) return;

    // Detect format and parse
    // Simple heuristic: count tabs. If tabs > 0, assume TSV. Else CSV.
    // Excel puts TSV on clipboard.
    wchar_t delimiter = L'\t';
    if (text.find(L'\t') == std::wstring::npos) {
        delimiter = m_delimiter; // Fallback to current delimiter
        if (text.find(L',') != std::wstring::npos && m_delimiter != L',') {
            delimiter = L','; 
        }
    }

    std::vector<std::vector<std::wstring>> grid;
    
    // Parse Logic
    bool inQuotes = false;
    std::wstring currentCell;
    std::vector<std::wstring> currentRow;
    
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        
        if (inQuotes) {
            if (c == L'\"') {
                if (i + 1 < text.size() && text[i+1] == L'\"') {
                    currentCell += L'\"';
                    i++;
                } else {
                    inQuotes = false;
                }
            } else {
                currentCell += c;
            }
        } else {
            if (c == L'\"') {
                inQuotes = true;
            } else if (c == delimiter) {
                currentRow.push_back(currentCell);
                currentCell.clear();
            } else if (c == L'\n') {
                currentRow.push_back(currentCell);
                currentCell.clear();
                grid.push_back(currentRow);
                currentRow.clear();
            } else if (c == L'\r') {
                // Ignore \r
            } else {
                currentCell += c;
            }
        }
    }
    if (!currentCell.empty() || !currentRow.empty()) {
        currentRow.push_back(currentCell);
        grid.push_back(currentRow);
    }
    
    // Apply to Document
    for(size_t r = 0; r < grid.size(); ++r) {
        size_t targetRow = startRow + r;
        
        std::vector<std::wstring> cells;
        if (targetRow < GetRowCount()) {
             cells = GetRowCells(targetRow);
        }
        
        // Resize if needed
        size_t neededSize = startCol + grid[r].size();
        if (cells.size() < neededSize) {
            cells.resize(neededSize, L"");
        }
        
        for(size_t c = 0; c < grid[r].size(); ++c) {
            cells[startCol + c] = grid[r][c];
        }
        
        if (targetRow < GetRowCount()) {
             SetRowCells(targetRow, cells);
        } else {
             InsertRow(targetRow, cells);
        }
    }
}

void CsvDocument::SetRowCells(size_t rowIndex, const std::vector<std::wstring>& cells)
{
    // Reconstruct row string and replace in CsvPieceTable
    std::wstring newRowStr = ConstructRowString(cells);
    newRowStr += GetLineEndingStr(); 
    
    std::string bytes;
    if (!newRowStr.empty()) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, newRowStr.c_str(), (int)newRowStr.length(), NULL, 0, NULL, NULL);
        bytes.resize(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, newRowStr.c_str(), (int)newRowStr.length(), &bytes[0], size_needed, NULL, NULL);
    }
    
    uint64_t startOffset = m_rowOffsets[rowIndex];
    uint64_t endOffset = (rowIndex + 1 < m_rowOffsets.size()) ? m_rowOffsets[rowIndex + 1] : m_pieceTable.GetSize();
    
    m_pieceTable.Delete(startOffset, endOffset - startOffset);
    m_pieceTable.Insert(startOffset, (const uint8_t*)bytes.data(), bytes.size());
    
    RebuildRowIndex();
}


bool CsvDocument::Export(const std::wstring& filePath, ExportFormat format)
{
    FILE* f = _wfopen(filePath.c_str(), L"wb");
    if (!f) return false;

    std::string content;
    
    if (format == ExportFormat::HTML) {
        content += "<html>\n<body>\n<table border=\"1\">\n";
        size_t rows = GetRowCount();
        for (size_t r = 0; r < rows; ++r) {
            content += "  <tr>\n";
            auto cells = GetRowCells(r);
            for (const auto& cell : cells) {
                // Convert wstring to UTF-8
                std::string cellUtf8;
                int len = WideCharToMultiByte(CP_UTF8, 0, cell.c_str(), -1, NULL, 0, NULL, NULL);
                if (len > 0) {
                    cellUtf8.resize(len-1);
                    WideCharToMultiByte(CP_UTF8, 0, cell.c_str(), -1, &cellUtf8[0], len, NULL, NULL);
                }
                
                // Simple escaping
                std::string escaped;
                for(char c : cellUtf8) {
                    if (c == '<') escaped += "&lt;";
                    else if (c == '>') escaped += "&gt;";
                    else if (c == '&') escaped += "&amp;";
                    else escaped += c;
                }

                content += "    <td>" + escaped + "</td>\n";
            }
            content += "  </tr>\n";
        }
        content += "</table>\n</body>\n</html>";
    }
    else if (format == ExportFormat::Markdown) {
        // Find max cols
        size_t rows = GetRowCount();
        if (rows == 0) {
             fclose(f); return true;
        }

        auto headerCells = GetRowCells(0);
        size_t cols = headerCells.size();
        
        // Header
        content += "|";
        for (const auto& cell : headerCells) {
             std::string cellUtf8;
             int len = WideCharToMultiByte(CP_UTF8, 0, cell.c_str(), -1, NULL, 0, NULL, NULL);
             if (len > 0) { cellUtf8.resize(len-1); WideCharToMultiByte(CP_UTF8, 0, cell.c_str(), -1, &cellUtf8[0], len, NULL, NULL); }
             content += " " + cellUtf8 + " |";
        }
        content += "\n|";
        for (size_t c = 0; c < cols; ++c) content += " --- |";
        content += "\n";
        
        // Rows
        for (size_t r = 1; r < rows; ++r) {
            content += "|";
            auto cells = GetRowCells(r);
            size_t currentCols = cells.size();
            for (size_t c=0; c<cols; ++c) { // Normalize to header col count
                std::string cellUtf8;
                if (c < currentCols) {
                    const auto& cell = cells[c];
                    int len = WideCharToMultiByte(CP_UTF8, 0, cell.c_str(), -1, NULL, 0, NULL, NULL);
                    if (len > 0) { cellUtf8.resize(len-1); WideCharToMultiByte(CP_UTF8, 0, cell.c_str(), -1, &cellUtf8[0], len, NULL, NULL); }
                }
                content += " " + cellUtf8 + " |";
            }
            content += "\n";
        }
    }

    fwrite(content.c_str(), 1, content.size(), f);
    fclose(f);
    return true;
}
size_t CsvDocument::GetMaxColumnCount()
{
    size_t maxCols = 0;
    size_t rows = GetRowCount();
    
    if (rows > 1000) {
        // Sample
        for(size_t i=0; i<100; ++i) {
             size_t c = GetRowCells(i).size();
             if(c > maxCols) maxCols = c;
        }
        return maxCols > 0 ? maxCols : 26;
    }
    
    for (size_t i = 0; i < rows; ++i) {
        size_t c = GetRowCells(i).size();
        if (c > maxCols) maxCols = c;
    }
    return maxCols > 0 ? maxCols : 26;
}

// Helper: compare two cell strings. Numeric strings compare by value.
static int CompareCellValues(const std::wstring& a, const std::wstring& b)
{
    // Try numeric compare
    wchar_t* endA = nullptr;
    wchar_t* endB = nullptr;
    double da = wcstod(a.c_str(), &endA);
    double db = wcstod(b.c_str(), &endB);

    bool aIsNum = (endA && *endA == L'\0' && !a.empty());
    bool bIsNum = (endB && *endB == L'\0' && !b.empty());

    if (aIsNum && bIsNum) {
        if (da < db) return -1;
        if (da > db) return  1;
        return 0;
    }
    // Fallback: case-insensitive lexicographic
    return _wcsicmp(a.c_str(), b.c_str());
}

void CsvDocument::SortRowsByColumn(size_t startRow, size_t endRow, size_t sortCol, bool ascending)
{
    size_t rowCount = GetRowCount();
    if (startRow >= rowCount) return;
    if (endRow >= rowCount) endRow = rowCount - 1;
    if (startRow >= endRow) return;

    Snapshot();

    // Read all rows in the range into memory
    size_t rangeSize = endRow - startRow + 1;
    std::vector<std::vector<std::wstring>> rows(rangeSize);
    for (size_t i = 0; i < rangeSize; ++i) {
        rows[i] = GetRowCells(startRow + i);
    }

    // Sort the in-memory rows
    std::stable_sort(rows.begin(), rows.end(),
        [&](const std::vector<std::wstring>& ra, const std::vector<std::wstring>& rb) {
            const std::wstring& va = (sortCol < ra.size()) ? ra[sortCol] : L"";
            const std::wstring& vb = (sortCol < rb.size()) ? rb[sortCol] : L"";
            int cmp = CompareCellValues(va, vb);
            return ascending ? (cmp < 0) : (cmp > 0);
        });

    // Write sorted rows back (from bottom to top to preserve offsets)
    // We update each row individually – SetRowCells does full row replacement
    for (size_t i = 0; i < rangeSize; ++i) {
        SetRowCells(startRow + i, rows[i]);
    }

    RebuildRowIndex();
}

void CsvDocument::SortColumnsByRow(size_t startCol, size_t endCol, size_t sortRow, bool ascending)
{
    size_t rowCount = GetRowCount();
    if (sortRow >= rowCount) return;
    if (startCol > endCol) return;

    Snapshot();

    // Determine actual column count (max across all rows)
    size_t colCount = GetMaxColumnCount();
    if (startCol >= colCount) return;
    if (endCol >= colCount) endCol = colCount - 1;

    // Read ALL rows completely into memory (we need to rearrange columns)
    std::vector<std::vector<std::wstring>> allRows(rowCount);
    for (size_t r = 0; r < rowCount; ++r) {
        allRows[r] = GetRowCells(r);
    }

    // Collect column indices in the sort range
    size_t rangeSize = endCol - startCol + 1;
    std::vector<size_t> colOrder(rangeSize);
    for (size_t i = 0; i < rangeSize; ++i) colOrder[i] = startCol + i;

    // Sort column order by the values in sortRow
    std::stable_sort(colOrder.begin(), colOrder.end(),
        [&](size_t ca, size_t cb) {
            const auto& keyRow = allRows[sortRow];
            const std::wstring& va = (ca < keyRow.size()) ? keyRow[ca] : L"";
            const std::wstring& vb = (cb < keyRow.size()) ? keyRow[cb] : L"";
            int cmp = CompareCellValues(va, vb);
            return ascending ? (cmp < 0) : (cmp > 0);
        });

    // Rearrange columns in all rows according to colOrder
    std::vector<std::vector<std::wstring>> newRows(rowCount);
    for (size_t r = 0; r < rowCount; ++r) {
        newRows[r] = allRows[r]; // copy base
        for (size_t i = 0; i < rangeSize; ++i) {
            size_t srcCol = colOrder[i];
            size_t dstCol = startCol + i;
            // Ensure capacity
            while (newRows[r].size() <= dstCol) newRows[r].push_back(L"");
            while (allRows[r].size() <= srcCol) allRows[r].push_back(L"");
            newRows[r][dstCol] = allRows[r][srcCol];
        }
    }

    // Write back
    for (size_t r = 0; r < rowCount; ++r) {
        SetRowCells(r, newRows[r]);
    }

    RebuildRowIndex();
}