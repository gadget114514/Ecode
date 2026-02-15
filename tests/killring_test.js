// Test for Kill Ring functionality (Emacs-style kill/yank)

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

function test_kill_ring() {
    Editor.newFile();
    Editor.setStatusText("Running Kill Ring Tests...");

    // Setup test data
    var lines = [
        "Line 1: First line",
        "Line 2: Second line",
        "Line 3: Third line",
        "Line 4: Fourth line"
    ];

    for (var i = 0; i < lines.length; i++) {
        Editor.insert(Editor.getLength(), lines[i] + "\n");
    }

    // Test 1: Kill line (Ctrl+K)
    Editor.setStatusText("Test 1: Kill line functionality...");
    Editor.setCaretPos(0); // Beginning of file

    // Simulate emacs_kill_line
    var pos = Editor.getCaretPos();
    var lineIdx = Editor.getLineAtOffset(pos);
    var lineEnd = Editor.getLineOffset(lineIdx + 1);
    if (lineEnd == 0) lineEnd = Editor.getLength();
    var killText = Editor.getText(pos, lineEnd - pos - 1); // -1 for newline
    Editor.delete(pos, lineEnd - pos - 1);

    assertEqual(killText, "Line 1: First line", "Killed text should match line 1");

    // Test 2: Multiple kills
    Editor.setStatusText("Test 2: Multiple kills...");
    pos = Editor.getCaretPos();
    lineIdx = Editor.getLineAtOffset(pos);
    lineEnd = Editor.getLineOffset(lineIdx + 1);
    if (lineEnd == 0) lineEnd = Editor.getLength();
    var killText2 = Editor.getText(pos, lineEnd - pos - 1);
    Editor.delete(pos, lineEnd - pos - 1);

    assertEqual(killText2, "Line 2: Second line", "Second killed text should match line 2");

    // Test 3: Yank (paste)
    Editor.setStatusText("Test 3: Yank (paste)...");
    Editor.setCaretPos(Editor.getLength()); // End of file
    Editor.insert(Editor.getCaretPos(), "\n" + killText2);

    var finalText = Editor.getText(0, Editor.getLength());
    assertTrue(finalText.indexOf("Line 2: Second line") > 0, "Yanked text should be in buffer");

    // Test 4: Kill region (Ctrl+W)
    Editor.setStatusText("Test 4: Kill region...");
    Editor.newFile();
    Editor.insert(0, "Select this text");
    Editor.setSelectionAnchor(0);
    Editor.setCaretPos(10); // Select "Select thi"
    var selectedText = Editor.getText(0, 10);
    Editor.cut();

    assertEqual(Editor.getText(0, Editor.getLength()), "s text", "Remaining text after cut");

    // Test 5: Copy region (Alt+W)
    Editor.setStatusText("Test 5: Copy region...");
    Editor.newFile();
    Editor.insert(0, "Copy this text");
    Editor.setSelectionAnchor(0);
    Editor.setCaretPos(9); // Select "Copy this"
    Editor.copy();

    assertEqual(Editor.getText(0, Editor.getLength()), "Copy this text", "Text should remain after copy");

    // Paste to verify clipboard
    Editor.setCaretPos(Editor.getLength());
    Editor.insert(Editor.getCaretPos(), " - ");
    Editor.paste();
    assertTrue(Editor.getText(0, Editor.getLength()).indexOf("Copy this text - Copy this") >= 0,
        "Pasted text should match copied text");

    Editor.setStatusText("Kill Ring Tests: PASSED");
    return "SUCCESS";
}

test_kill_ring();
