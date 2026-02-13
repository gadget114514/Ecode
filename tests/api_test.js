// Ecode JavaScript API Test Suite
// This script tests basic buffer manipulation and caret movement APIs

function test_js_apis() {
    Editor.setStatusText("Running JS API Tests...");

    // Test: Insertion
    var startPos = Editor.getCaretPos();
    Editor.insert(startPos, "API TEST START\n");
    
    // Test: Length and Position
    var len = Editor.getLength();
    var pos = Editor.getCaretPos();
    
    // Test: Selection
    Editor.insert(pos, "Selecting this text.");
    Editor.setSelectionAnchor(pos);
    Editor.setCaretPos(pos + 19); // "Selecting this text" is 19 chars
    
    // Test: Caret Movement
    Editor.setCaretPos(Editor.getLength());
    Editor.insert(Editor.getLength(), "\nCaret moved to end.\n");

    // Test: Search
    var findPos = Editor.find("API TEST", 0, true, false, true);
    if (findPos !== -1) {
        Editor.insert(Editor.getLength(), "Find API TEST: SUCCESS at " + findPos + "\n");
    } else {
        Editor.insert(Editor.getLength(), "Find API TEST: FAILED\n");
    }

    Editor.setStatusText("JS API Tests Completed.");
}

test_js_apis();
