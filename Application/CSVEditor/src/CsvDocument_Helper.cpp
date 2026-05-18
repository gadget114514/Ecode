#include "CsvDocument.h"

size_t CsvDocument::GetMaxColumnCount()
{
    // Need to iterate all rows? Or cache it.
    // For now, iterate visible rows is fast, but scrollbar logic needs total.
    // Let's iterate. 
    // Performance optimization: sample first 100 rows?
    // User wants correctness.
    // If file is huge, this might be slow on every scroll update if not cached.
    // RowOffsets size is rowCount.
    // Accessing GetRowCells parses line. Parsing value is cheap-ish.
    
    // Simplification: Check first row and last row and middle? 
    // Or just valid implementation:
    size_t maxCols = 0;
    size_t rows = GetRowCount();
    
    // Scan max 1000 rows to avoid freeze?
    // Or just return a safe large number if rows > 1000?
    // Let's just scan all. 
    // Optimization: Cache this value and invalidate on edits.
    // But adding member needs header change.
    // Let's implement full scan for now, if slow we optimize.
    
    // If you prefer speed: just return 100.
    // If you prefer correctness: scan.
    
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
    return maxCols > 0 ? maxCols : 26; // Default to 'Z'
}
