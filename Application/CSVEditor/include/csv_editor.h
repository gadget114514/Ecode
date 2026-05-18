#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <functional>

// ── Enums ─────────────────────────────────────────────────────────────────

enum class CsvFileEncoding {
    UTF8,
    UTF16_LE,
    UTF16_BE,
    ANSI
};

enum class CsvLineEnding {
    CRLF,
    LF,
    CR
};

struct CsvCellPos {
    size_t row;
    size_t col;
    bool operator==(const CsvCellPos& other) const {
        return row == other.row && col == other.col;
    }
};

// ── Core Document API ─────────────────────────────────────────────────────

class CsvDocument {
public:
    CsvDocument();
    ~CsvDocument();

    // ── File I/O ──
    bool Load(const std::wstring& filePath,
              std::function<void(float)> progressCallback = nullptr);
    bool Import(const std::wstring& filePath);
    bool Save(const std::wstring& filePath);

    enum class CsvExportFormat { HTML, Markdown };
    bool Export(const std::wstring& filePath, CsvExportFormat format);

    // ── Row Access ──
    size_t GetRowCount() const;
    uint64_t GetRowStartOffset(size_t rowIndex) const;
    std::vector<uint8_t> GetRowRaw(size_t rowIndex);
    std::vector<std::wstring> GetRowCells(size_t rowIndex);
    size_t GetMaxColumnCount();
    std::wstring GetRangeAsText(size_t startRow, size_t startCol,
                                size_t endRow, size_t endCol);

    // ── Search & Replace ──
    enum class CsvSearchMode { Contains, Exact, Regex };
    struct CsvSearchOptions {
        bool matchCase = false;
        CsvSearchMode mode = CsvSearchMode::Contains;
        bool forward = true;
        bool includeStart = false;
    };

    bool Search(const std::wstring& query, size_t& row, size_t& col,
                const CsvSearchOptions& options);
    bool Replace(const std::wstring& query, const std::wstring& replacement,
                 size_t& row, size_t& col, const CsvSearchOptions& options);
    int ReplaceAll(const std::wstring& query, const std::wstring& replacement,
                   const CsvSearchOptions& options);

    // ── Configuration ──
    void SetDelimiter(wchar_t delimiter);
    void SetEncoding(CsvFileEncoding encoding);

    // ── Editing ──
    void InsertColumn(size_t colIndex,
                      const std::wstring& defaultValue = L"");
    void DeleteColumn(size_t colIndex);
    void DeleteRow(size_t rowIndex);
    void InsertRow(size_t rowIndex,
                   const std::vector<std::wstring>& values);
    void UpdateCell(size_t row, size_t col, const std::wstring& value);
    void PasteCells(size_t startRow, size_t startCol,
                    const std::wstring& text);

    // ── Sort ──
    void SortRowsByColumn(size_t startRow, size_t endRow, size_t sortCol,
                          bool ascending = true);
    void SortColumnsByRow(size_t startCol, size_t endCol, size_t sortRow,
                          bool ascending = true);

    // ── History ──
    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;
};
