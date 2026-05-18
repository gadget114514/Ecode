#pragma once

#include "PieceTable.h"
#include <vector>
#include <string>
#include <functional>

enum class CsvFileEncoding {
    UTF8,
    UTF16_LE,
    UTF16_BE,
    ANSI // Fallback
};

enum class CsvLineEnding {
    CRLF, // \r\n
    LF,   // \n
    CR    // \r
};

class CsvDocument {
public:
    CsvDocument();
    ~CsvDocument();

    bool Load(const std::wstring& filePath, std::function<void(float)> progressCallback = nullptr);
    bool Import(const std::wstring& filePath); // Appends to end
    bool Save(const std::wstring& filePath);
    
    enum class ExportFormat {
        HTML,
        Markdown
    };
    bool Export(const std::wstring& filePath, ExportFormat format);
    
    // Row Access
    size_t GetRowCount() const;
    uint64_t GetRowStartOffset(size_t rowIndex) const;
    
    // Returns the raw string content of the row (bytes currently)
    std::vector<uint8_t> GetRowRaw(size_t rowIndex);
    
    // Returns parsed cells as wide strings
    std::vector<std::wstring> GetRowCells(size_t rowIndex);
    
    // Returns max columns across all rows (slow-ish but safe)
    size_t GetMaxColumnCount();

    // Export range to string (e.g. for clipboard), typically TSV or CSV
    std::wstring GetRangeAsText(size_t startRow, size_t startCol, size_t endRow, size_t endCol);


    // Search & Replace Types
    enum class SearchMode { Contains, Exact, Regex };
    struct SearchOptions {
        bool matchCase = false;
        SearchMode mode = SearchMode::Contains;
        bool forward = true;
        bool includeStart = false;
    };

    // Search
    // Updates row/col to match position if found. Returns true if found.
    bool Search(const std::wstring& query, size_t& row, size_t& col, const SearchOptions& options);

    // Replace
    // Replaces match at specific cell if found. Returns true if changed.
    // If 'findNext' is true, updates row/col to the next match after replacement.
    bool Replace(const std::wstring& query, const std::wstring& replacement, size_t& row, size_t& col, const SearchOptions& options);
    
    // Replace All
    // Returns number of replacements.
    int ReplaceAll(const std::wstring& query, const std::wstring& replacement, const SearchOptions& options);
    
    // Configuration
    void SetDelimiter(wchar_t delimiter);
    void SetEncoding(CsvFileEncoding encoding);


    // Parsing (Basic)
    void RebuildRowIndex(std::function<void(float)> progressCallback = nullptr);

    // Column Operations
    void InsertColumn(size_t colIndex, const std::wstring& defaultValue = L"");
    void DeleteColumn(size_t colIndex);

    void DeleteRow(size_t rowIndex);
    void InsertRow(size_t rowIndex, const std::vector<std::wstring>& values);
    void UpdateCell(size_t row, size_t col, const std::wstring& value);
    void PasteCells(size_t startRow, size_t startCol, const std::wstring& text);

    // Sort Operations
    // Sort rows [startRow, endRow] by values in 'sortCol'. ascending=true => A..Z
    void SortRowsByColumn(size_t startRow, size_t endRow, size_t sortCol, bool ascending = true);
    // Sort columns [startCol, endCol] by values in 'sortRow'. ascending=true => A..Z
    void SortColumnsByRow(size_t startCol, size_t endCol, size_t sortRow, bool ascending = true);

    // History
    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;

private:
    void SetRowCells(size_t rowIndex, const std::vector<std::wstring>& cells);

private:
    void Snapshot(); // Save current state to Undo Stack
    
    // Helpers
    void DetectEncoding();
    void DetectLineEnding();
    std::wstring GetLineEndingStr() const;
    std::vector<uint8_t> EncodeString(const std::wstring& str);
    std::wstring DecodeString(const std::vector<uint8_t>& bytes);
    std::vector<std::wstring> ParseRowCells(const std::wstring& rowText);
    std::wstring ConstructRowString(const std::vector<std::wstring>& cells);

    struct HistoryState {
        std::vector<CsvPiece> pieces;
    };
    std::vector<HistoryState> m_undoStack;
    std::vector<HistoryState> m_redoStack;

private:
    CsvPieceTable m_pieceTable;
    std::vector<uint64_t> m_rowOffsets; // Start offset of each row
    
    // Configuration
    CsvLineEnding m_lineEnding = CsvLineEnding::LF;
    CsvFileEncoding m_encoding = CsvFileEncoding::UTF8;
    wchar_t m_delimiter = L',';

    // Helpers
};
