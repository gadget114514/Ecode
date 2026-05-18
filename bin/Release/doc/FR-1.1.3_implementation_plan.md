# FR-1.1.3: Large File Support - Implementation Plan

## Problem Analysis
Currently, the renderer creates a single DirectWrite TextLayout for the **entire visible text** of the buffer, which can be gigabytes in size. This causes:
1. **Memory overhead**: DirectWrite allocates internal structures for the entire text
2. **Performance degradation**: Layout creation and hit-testing become extremely slow
3. **Rendering lag**: Even though only a viewport is visible, the entire text is processed

## Solution: Viewport-Based Rendering
Implement a windowed rendering approach that only processes text visible in the current viewport.

### Key Changes

#### 1. Add Viewport Calculation to Buffer
- Add method `GetViewportText(size_t startLine, size_t lineCount)` to Buffer class
- This returns only the text for lines that are actually visible on screen
- Calculate visible line count based on window height and line height

#### 2. Modify DrawEditorLines
- Change signature to accept viewport parameters
- Only create TextLayout for visible lines (typically 50-100 lines)
- Adjust coordinate calculations to account for viewport offset

#### 3. Update Coordinate Mapping
- `GetPositionFromPoint`: Map screen coordinates to viewport-relative positions
- `HitTestGutter`: Work with viewport-relative line indices
- Ensure caret positioning works correctly with viewport

#### 4. Optimize Selection Rendering
- Only process selection ranges that intersect with the viewport
- Skip highlighting for off-screen selections

#### 5. Optimize Syntax Highlighting
- Only apply highlighting to visible text
- Consider lazy highlighting (highlight on-demand as user scrolls)

### Performance Targets
- **Memory**: O(viewport_size) instead of O(file_size)
- **Rendering**: Constant time regardless of file size
- **Scrolling**: Smooth 60fps even with multi-GB files
- **File loading**: Instant (already achieved with memory-mapped files)

### Implementation Steps
1. ✅ Add `GetViewportText()` method to Buffer class
2. ✅ Add `CalculateVisibleLineCount()` helper to EditorBufferRenderer
3. ✅ Modify `DrawEditorLines` to use viewport rendering
4. ✅ Update coordinate mapping functions
5. ✅ Test with large files (100MB, 1GB, 5GB)
6. ✅ Profile and optimize if needed

### Testing Strategy
1. Create test files of various sizes (100MB, 1GB, 5GB)
2. Measure memory usage before/after
3. Measure rendering FPS before/after
4. Test edge cases:
   - Scrolling to end of large file
   - Searching in large file
   - Selecting across viewport boundaries
   - Folding/unfolding in large files

### Compatibility Notes
- This change is internal to the rendering system
- No API changes required
- Existing functionality (search, selection, editing) remains unchanged
- Memory-mapped file support already handles the file I/O efficiently
