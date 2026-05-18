#include "MainWindow.h"

float MainWindow::GetColumnWidth(size_t col) const
{
    auto it = m_colWidths.find(col);
    if (it != m_colWidths.end()) return it->second;
    return m_defaultColWidth;
}

float MainWindow::GetColumnX(size_t col) const
{
    // Needs Optimization for many columns, but fine for now
    // Can cache total width?
    float x = 0;
    // Optimization: Check if map is empty
    if (m_colWidths.empty()) {
        return (float)col * m_defaultColWidth;
    }
    
    // Mixed
    for (size_t i = 0; i < col; ++i) {
        x += GetColumnWidth(i);
    }
    return x;
}

size_t MainWindow::GetColumnAtX(float targetX) const
{
    if (targetX < 0) return 0;
    
    // Optimization: Empty map
    if (m_colWidths.empty()) {
         return (size_t)(targetX / m_defaultColWidth);
    }
    
    float x = 0;
    size_t c = 0;
    // Limit?
    size_t maxCols = 10000; // Safety break
    // We should use document col count usually, but hit test might go beyond
    // Let's loop until x > targetX
    while (x <= targetX) {
        float w = GetColumnWidth(c);
        if (x + w > targetX) return c;
        x += w;
        c++;
        if (c > maxCols) return c; // Fallback
    }
    return c;
}
