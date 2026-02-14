function assertEqual(actual, expected, msg) {
    if (actual !== expected) {
        throw "ASSERTION FAILED: " + msg + " (Expected " + expected + ", Got " + actual + ")";
    }
}

// Test for multiple sequential operations in JS API
function test_multiple_ops() {
    Editor.newFile();
    Editor.setStatusText("Testing Multiple Operations...");

    // 1. Initial Insert
    Editor.insert(0, "Line A\nLine B\nLine C");

    // 2. Multiple inserts at different positions
    Editor.insert(7, "[B-Prefix] "); // Line 2 start
    Editor.insert(Editor.getLength(), "\n[End]");
    Editor.insert(0, "[Start]\n");

    // Current expected state:
    // "[Start]\nLine A\n[B-Prefix] Line B\nLine C\n[End]"
    assertEqual(Editor.getText(0, Editor.getLength()), "[Start]\nLine A\n[B-Prefix] Line B\nLine C\n[End]", "Line 20: Content mismatch after multiple inserts");

    // 3. Sequential deletions
    // Delete "[Start]\n" (8 chars)
    Editor.delete(0, 8);
    // Buffer: "Line A\n[B-Prefix] Line B\nLine C\n[End]"

    // Delete "[B-Prefix] " (11 chars) which is at index 7 (after "Line A\n")
    Editor.delete(7, 11);
    // Buffer: "Line A\nLine B\nLine C\n[End]"

    assertEqual(Editor.getText(0, Editor.getLength()), "Line A\nLine B\nLine C\n[End]", "Line 26: Content mismatch after sequential deletes");

    // 4. Batch check line counts
    assertEqual(Editor.getTotalLines(), 4, "Line 29: Line count mismatch");

    // 5. Compound Find and Replace Simulation
    var p = Editor.find("Line B", 0, true, false, true);
    if (p !== -1) {
        Editor.delete(p, 6);
        Editor.insert(p, "Modified Line");
    }

    assertEqual(Editor.getText(0, Editor.getLength()), "Line A\nModified Line\nLine C\n[End]", "Line 38: Content mismatch after find/replace");

    Editor.setStatusText("Multiple Operations JS Tests PASSED");
    return "SUCCESS";
}

test_multiple_ops();
