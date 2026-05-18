// Tests for selection-related Editor APIs

function assertEqual(actual, expected, msg) {
    if (actual !== expected) {
        throw "ASSERT FAILED: " + msg + " (Expected '" + expected + "', Got '" + actual + "')";
    }
}

function assertTrue(condition, msg) {
    if (!condition) {
        throw "ASSERT FAILED: " + msg;
    }
}

function test_selection_apis() {
    Editor.newFile();
    Editor.setStatusText("Running Selection API Tests...");

    // Test 1: setSelectionAnchor and getSelectionAnchor
    Editor.setStatusText("Test 1: Selection anchor...");
    Editor.insert(0, "Hello World");
    Editor.setSelectionAnchor(0);
    assertEqual(Editor.getSelectionAnchor(), 0, "Selection anchor should be 0");
    Editor.setSelectionAnchor(3);
    assertEqual(Editor.getSelectionAnchor(), 3, "Selection anchor should be 3");

    // Test 2: deleteChar deletes selected text when selection exists
    Editor.setStatusText("Test 2: Delete with selection...");
    Editor.newFile();
    Editor.insert(0, "Hello World");
    Editor.setSelectionAnchor(0);
    Editor.setCaretPos(5);
    Editor.deleteChar();
    var text = Editor.getText(0, Editor.getLength());
    assertEqual(text, " World", "After deleteChar with selection, 'Hello' should be gone");

    // Test 3: backspace deletes selected text when selection exists
    Editor.setStatusText("Test 3: Backspace with selection...");
    Editor.newFile();
    Editor.insert(0, "Hello World");
    Editor.setSelectionAnchor(0);
    Editor.setCaretPos(5);
    Editor.backspace();
    text = Editor.getText(0, Editor.getLength());
    assertEqual(text, " World", "After backspace with selection, 'Hello' should be gone");

    // Test 4: setSelectionMode
    Editor.setStatusText("Test 4: Selection mode...");
    Editor.newFile();
    Editor.insert(0, "Line 1\nLine 2\nLine 3");
    Editor.setSelectionMode(1); // Box selection mode
    Editor.setSelectionMode(0); // Back to normal

    // Test 5: Anchor and caret reversed
    Editor.setStatusText("Test 5: Reversed anchor/caret...");
    Editor.newFile();
    Editor.insert(0, "ABCDEFGHIJ");
    Editor.setSelectionAnchor(8);
    Editor.setCaretPos(3);
    Editor.deleteChar();
    text = Editor.getText(0, Editor.getLength());
    assertEqual(text, "HIJ", "After delete with reversed selection, 'ABCDEFG' should be gone");

    Editor.setStatusText("Selection API Tests: PASSED");
    return "SUCCESS";
}

test_selection_apis();
