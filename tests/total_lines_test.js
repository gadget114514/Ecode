function assertEqual(actual, expected, msg) {
    if (actual !== expected) {
        // Try to get line number from stack if possible, but for now just unique messages
        throw "ASSERTION FAILED: " + msg + " (Expected " + expected + ", Got " + actual + ")";
    }
}

// Test for total lines and line-related APIs
function test_line_apis() {
    Editor.newFile();
    Editor.setStatusText("Testing Line APIs...");

    // Initial state
    assertEqual(Editor.getTotalLines(), 1, "Line 13: Initial lines mismatch");

    // Simple insertions
    Editor.insert(0, "Line 1\nLine 2\nLine 3");
    assertEqual(Editor.getTotalLines(), 3, "Line 17: After 3 lines insertion mismatch");

    // Insertion with multiple newlines
    Editor.insert(Editor.getLength(), "\n\nLine 6");
    assertEqual(Editor.getTotalLines(), 6, "Line 21: After extra newlines mismatch");

    // Verify offsets
    // "Line 1\nLine 2\nLine 3\n\nLine 6"
    // Line 0: index 0
    assertEqual(Editor.getLineOffset(0), 0, "Line 24: Line 0 offset mismatch");

    // Line 1: starts after "Line 1\n" (7 chars)
    assertEqual(Editor.getLineOffset(1), 7, "Line 27: Line 1 offset mismatch");

    // Test getLineAtOffset
    assertEqual(Editor.getLineAtOffset(0), 0, "Line 30: Offset 0 should be Line 0");
    assertEqual(Editor.getLineAtOffset(7), 1, "Line 31: Offset 7 should be Line 1");
    assertEqual(Editor.getLineAtOffset(6), 0, "Line 32: Offset 6 should be Line 0");

    // Delete text containing newlines
    // Delete "Line 2\nLine 3\n"
    var startOfL2 = Editor.getLineOffset(1);
    var startOfL4 = Editor.getLineOffset(3);
    Editor.delete(startOfL2, startOfL4 - startOfL2);
    // Buffer now: "Line 1\n\nLine 6"
    assertEqual(Editor.getTotalLines(), 4, "Line 38: After delete mismatch");

    // Undo/Redo line consistency
    Editor.undo();
    assertEqual(Editor.getTotalLines(), 6, "Line 42: After undo mismatch");
    Editor.redo();
    assertEqual(Editor.getTotalLines(), 4, "Line 44: After redo mismatch");

    Editor.setStatusText("Line API Tests PASSED");
    return "SUCCESS";
}

test_line_apis();
