#pragma once

#include <vector>
#include <map>
#include <cstdint>

struct CsvCellPos {
    size_t row;
    size_t col;

    bool operator==(const CsvCellPos& other) const {
        return row == other.row && col == other.col;
    }
};

enum class CsvSelectionMode {
    None,
    Cell,
    Row,
    Column,
    All
};

struct CsvSelectionRange {
    CsvCellPos start;
    CsvCellPos end; // Inclusive
    CsvSelectionMode mode;
};

class CsvEditorState {
public:
    CsvEditorState();

    // Selection
    void SelectCell(size_t row, size_t col, bool multiSelect = false);
    void SelectRow(size_t row, bool multiSelect = false);
    void SelectColumn(size_t col, bool multiSelect = false);
    void SelectAll();
    void ClearSelection();
    void DragTo(size_t row, size_t col);

    const std::vector<CsvSelectionRange>& GetSelections() const;
    bool IsSelected(size_t row, size_t col) const;
    bool IsRowSelected(size_t row) const;
    bool IsColumnSelected(size_t col) const;

    // Scroll Position
    size_t GetScrollRow() const { return m_scrollRow; }
    void SetScrollRow(size_t row) { m_scrollRow = row; }
    
    float GetScrollX() const { return m_scrollX; }
    void SetScrollX(float x) { m_scrollX = x; }

    // Column Widths
    float GetColumnWidth(size_t col) const;
    void SetColumnWidth(size_t col, float width);
    float GetDefaultColumnWidth() const { return m_defaultColWidth; }

private:
    std::vector<CsvSelectionRange> m_selections;
    CsvSelectionMode m_currentMode = CsvSelectionMode::None;
    CsvCellPos m_anchor; // Start of drag

    size_t m_scrollRow = 0;
    float m_scrollX = 0.0f;

    // Layout
    std::map<size_t, float> m_colWidths;
    float m_defaultColWidth = 100.0f;
};
