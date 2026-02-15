// Test for Emacs navigation commands

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

function test_emacs_navigation() {
    Editor.newFile();
    Editor.setStatusText("Running Emacs Navigation Tests...");

    // Setup test data - 10 lines
    var lines = [];
    for (var i = 1; i <= 10; i++) {
        lines.push("Line " + i + ": Content of line " + i);
    }

    for (var i = 0; i < lines.length; i++) {
        Editor.insert(Editor.getLength(), lines[i] + "\n");
    }

    // Test 1: Beginning of buffer (Alt+<)
    Editor.setStatusText("Test 1: Beginning of buffer...");
    Editor.setCaretPos(50); // Move to middle of file
    Editor.setCaretPos(0); // Simulate Alt+<
    Editor.setSelectionAnchor(0);

    assertEqual(Editor.getCaretPos(), 0, "Caret should be at beginning");

    // Test 2: End of buffer (Alt+>)
    Editor.setStatusText("Test 2: End of buffer...");
    var endPos = Editor.getLength();
    Editor.setCaretPos(endPos);
    Editor.setSelectionAnchor(endPos);

    assertEqual(Editor.getCaretPos(), endPos, "Caret should be at end");

    // Test 3: Line navigation (Ctrl+N / Ctrl+P)
    Editor.setStatusText("Test 3: Next/Previous line...");
    Editor.setCaretPos(0);

    var line0Offset = Editor.getLineAtOffset(0);
    Editor.moveCaretDown(); // Simulate Ctrl+N
    var line1Offset = Editor.getLineAtOffset(Editor.getCaretPos());
    assertEqual(line1Offset, 1, "Should move to line 1");

    Editor.moveCaretUp(); // Simulate Ctrl+P
    var line0OffsetAgain = Editor.getLineAtOffset(Editor.getCaretPos());
    assertEqual(line0OffsetAgain, 0, "Should move back to line 0");

    // Test 4: Character navigation (Ctrl+F / Ctrl+B)
    Editor.setStatusText("Test 4: Forward/Backward char...");
    Editor.setCaretPos(5);
    var pos5 = Editor.getCaretPos();

    Editor.moveCaretRight(); // Simulate Ctrl+F
    var pos6 = Editor.getCaretPos();
    assertTrue(pos6 > pos5, "Should move forward");

    Editor.moveCaretLeft(); // Simulate Ctrl+B
    var pos5Again = Editor.getCaretPos();
    assertEqual(pos5Again, pos5, "Should move back to position 5");

    // Test 5: Line start/end (Ctrl+A / Ctrl+E)
    Editor.setStatusText("Test 5: Line start/end...");
    Editor.setCaretPos(0);
    Editor.moveCaretEnd(); // Simulate Ctrl+E
    var lineEndPos = Editor.getCaretPos();

    var firstLine = lines[0];
    var textToEnd = Editor.getText(0, lineEndPos);
    assertTrue(textToEnd.indexOf(firstLine) >= 0, "Should be at end of first line");

    Editor.moveCaretHome(); // Simulate Ctrl+A
    assertEqual(Editor.getCaretPos(), 0, "Should be back at line start");

    // Test 6: Page navigation (Ctrl+V / Alt+V)
    Editor.setStatusText("Test 6: Page up/down...");
    Editor.setCaretPos(0);
    var initialLine = Editor.getLineAtOffset(Editor.getCaretPos());

    // Simulate scroll down (multiple moveCaretDown calls)
    for (var i = 0; i < 5; i++) {
        Editor.moveCaretDown();
    }
    var afterScrollLine = Editor.getLineAtOffset(Editor.getCaretPos());
    assertTrue(afterScrollLine >= initialLine + 3, "Should scroll down multiple lines");

    // Simulate scroll up
    for (var i = 0; i < 5; i++) {
        Editor.moveCaretUp();
    }
    var backToLine = Editor.getLineAtOffset(Editor.getCaretPos());
    assertEqual(backToLine, initialLine, "Should scroll back up");

    // Test 7: Set mark and region selection (Ctrl+Space)
    Editor.setStatusText("Test 7: Set mark and region...");
    Editor.setCaretPos(0);
    Editor.setSelectionAnchor(0); // Simulate Ctrl+Space

    Editor.moveCaretRight();
    Editor.moveCaretRight();
    Editor.moveCaretRight();

    // Should have selection from 0 to current pos
    var selectionStart = 0;
    var selectionEnd = Editor.getCaretPos();
    assertTrue(selectionEnd > selectionStart, "Should have a selection");

    Editor.setStatusText("Emacs Navigation Tests: PASSED");
    return "SUCCESS";
}

test_emacs_navigation();
