#include "EditorState.h"
#include <algorithm>
#include <map>

CsvEditorState::CsvEditorState()
{
}

float CsvEditorState::GetColumnWidth(size_t col) const {
    auto it = m_colWidths.find(col);
    if (it != m_colWidths.end()) {
        return it->second;
    }
    return m_defaultColWidth;
}

void CsvEditorState::SetColumnWidth(size_t col, float width) {
    if (width < 10.0f) width = 10.0f; // Minimum width
    m_colWidths[col] = width;
}

void CsvEditorState::SelectCell(size_t row, size_t col, bool multiSelect)
{
    if (!multiSelect) m_selections.clear();
    
    CsvSelectionRange range;
    range.start = {row, col};
    range.end = {row, col};
    range.mode = CsvSelectionMode::Cell;
    m_selections.push_back(range);
    
    m_anchor = {row, col};
    m_currentMode = CsvSelectionMode::Cell;
}

void CsvEditorState::SelectRow(size_t row, bool multiSelect)
{
    if (!multiSelect) m_selections.clear();

    CsvSelectionRange range;
    range.start = {row, 0};
    range.end = {row, 0}; // Col 0 ignored for row mode
    range.mode = CsvSelectionMode::Row;
    m_selections.push_back(range);
    
    m_anchor = {row, 0};
    m_currentMode = CsvSelectionMode::Row;
}

void CsvEditorState::SelectColumn(size_t col, bool multiSelect)
{
    if (!multiSelect) m_selections.clear();

    CsvSelectionRange range;
    range.start = {0, col};
    range.end = {0, col}; // Row 0 ignored for col mode
    range.mode = CsvSelectionMode::Column;
    m_selections.push_back(range);
    
    m_anchor = {0, col};
    m_currentMode = CsvSelectionMode::Column;
}

void CsvEditorState::SelectAll()
{
    m_selections.clear();
    
    CsvSelectionRange range;
    range.start = {0, 0};
    range.end = {0, 0}; // Meaningless for All? Or implied max? match Column/Row logic
    range.mode = CsvSelectionMode::All;
    m_selections.push_back(range);
    
    m_currentMode = CsvSelectionMode::All;
}

void CsvEditorState::ClearSelection()
{
    m_selections.clear();
    m_currentMode = CsvSelectionMode::None;
}

void CsvEditorState::DragTo(size_t row, size_t col)
{
    if (m_selections.empty()) return;
    
    // Update last selection end
    auto& range = m_selections.back();
    
    if (m_currentMode == CsvSelectionMode::Row) {
        // Anchor row fixed?
        // Usually anchor is fixed.
        // range.start is implicitly anchor? No. 
        // We should store Anchor properly.
        // m_anchor (from SelectRow)
        
        // Determine start/end from m_anchor and current (row, col)
        // range.start = min(anchor.row, row)
        // range.end = max(anchor.row, row)
        // But we store start/end simply.
        
        range.start.row = (std::min)(m_anchor.row, row);
        range.end.row = (std::max)(m_anchor.row, row);
    } else if (m_currentMode == CsvSelectionMode::Column) {
        range.start.col = (std::min)(m_anchor.col, col);
        range.end.col = (std::max)(m_anchor.col, col);
    } else if (m_currentMode == CsvSelectionMode::Cell) {
        range.start.row = (std::min)(m_anchor.row, row);
        range.start.col = (std::min)(m_anchor.col, col);
        range.end.row = (std::max)(m_anchor.row, row);
        range.end.col = (std::max)(m_anchor.col, col);
    }
}

const std::vector<CsvSelectionRange>& CsvEditorState::GetSelections() const
{
    return m_selections;
}

bool CsvEditorState::IsSelected(size_t row, size_t col) const
{
    for (const auto& sel : m_selections) {
        if (sel.mode == CsvSelectionMode::Cell) {
            if (row >= sel.start.row && row <= sel.end.row &&
                col >= sel.start.col && col <= sel.end.col) return true;
        } else if (sel.mode == CsvSelectionMode::Row) {
            if (row >= sel.start.row && row <= sel.end.row) return true;
        } else if (sel.mode == CsvSelectionMode::Column) {
            if (col >= sel.start.col && col <= sel.end.col) return true;
        } else if (sel.mode == CsvSelectionMode::All) {
            return true;
        }
    }
    return false;
}

bool CsvEditorState::IsRowSelected(size_t row) const
{
    for (const auto& sel : m_selections) {
        if (sel.mode == CsvSelectionMode::Row) {
            if (row >= sel.start.row && row <= sel.end.row) return true;
        }
    }
    return false;
}

bool CsvEditorState::IsColumnSelected(size_t col) const
{
    for (const auto& sel : m_selections) {
        if (sel.mode == CsvSelectionMode::Column) {
            if (col >= sel.start.col && col <= sel.end.col) return true;
        }
    }
    return false;
}
