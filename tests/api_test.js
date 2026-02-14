function assertEqual(actual, expected, msg) {
    if (actual !== expected) {
        throw "ASSERTION FAILED: " + msg + " (Expected '" + expected + "', Got '" + actual + "')";
    }
}

function test_js_apis() {
    Editor.newFile();
    Editor.setStatusText("Running JS API Tests...");

    // Test: Insertion
    Editor.insert(0, "API TEST START\n");
    assertEqual(Editor.getLength(), 15, "Line 11: Length after insert mismatch");

    // Test: Selection
    var pos = Editor.getLength();
    Editor.insert(pos, "Selecting this text.");
    Editor.setSelectionAnchor(pos);
    Editor.setCaretPos(pos + 20);
    assertEqual(Editor.getSelectionText(), "Selecting this text.", "Line 18: Selection text mismatch");

    // Test: Caret Movement
    Editor.setCaretPos(Editor.getLength());
    assertEqual(Editor.getCaretPos(), Editor.getLength(), "Line 22: Caret pos mismatch");

    // Test: Search
    var findPos = Editor.find("API TEST", 0, true, false, true);
    assertEqual(findPos, 0, "Line 26: Find 'API TEST' failed");

    Editor.setStatusText("JS API Tests Completed.");
    return "SUCCESS";
}

test_js_apis();
