// Test for multi-byte character support (UTF-8)

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

function test_multibyte_chars() {
    Editor.newFile();
    Editor.setStatusText("Running Multi-byte Character Tests...");

    // Test 1: Japanese characters
    Editor.setStatusText("Test 1: Japanese characters...");
    var japanese = "こんにちは世界"; // Hello World in Japanese
    Editor.insert(0, japanese);
    assertEqual(Editor.getText(0, Editor.getLength()), japanese, "Japanese text insertion");

    // Test 2: Emoji
    Editor.setStatusText("Test 2: Emoji characters...");
    Editor.newFile();
    var emoji = "Hello 😀 World 🌍 Test 🎉";
    Editor.insert(0, emoji);
    assertEqual(Editor.getText(0, Editor.getLength()), emoji, "Emoji text insertion");

    // Test 3: deleteChar on multi-byte character
    Editor.setStatusText("Test 3: deleteChar on emoji...");
    Editor.setCaretPos(6); // Position before first emoji
    Editor.deleteChar();
    var afterDelete = Editor.getText(0, Editor.getLength());
    assertEqual(afterDelete, "Hello  World 🌍 Test 🎉", "Emoji should be deleted completely");

    // Test 4: backspace on multi-byte character
    Editor.setStatusText("Test 4: backspace on emoji...");
    Editor.setCaretPos(13); // After "World "
    Editor.backspace();
    afterDelete = Editor.getText(0, Editor.getLength());
    assertEqual(afterDelete, "Hello  World 🎉", "Second emoji should be deleted with backspace");

    // Test 5: Move caret through multi-byte text
    Editor.setStatusText("Test 5: Caret movement through multi-byte...");
    Editor.newFile();
    Editor.insert(0, "日本語テスト"); // Japanese test

    // Move right character by character
    Editor.setCaretPos(0);
    Editor.moveCaretRight();
    var pos1 = Editor.getCaretPos();
    assertTrue(pos1 > 0, "Caret should move forward");

    Editor.moveCaretRight();
    var pos2 = Editor.getCaretPos();
    assertTrue(pos2 > pos1, "Caret should continue moving forward");

    // Test 6: Mixed ASCII and multi-byte
    Editor.setStatusText("Test 6: Mixed ASCII and multi-byte...");
    Editor.newFile();
    var mixed = "Hello 世界 World 日本";
    Editor.insert(0, mixed);
    assertEqual(Editor.getText(0, Editor.getLength()), mixed, "Mixed text insertion");

    // Delete multi-byte character in middle
    Editor.setCaretPos(6); // Before 世
    Editor.deleteChar();
    var result = Editor.getText(0, Editor.getLength());
    assertEqual(result, "Hello 界 World 日本", "Single multi-byte char deleted from mixed text");

    // Test 7: Caret at end of line with multi-byte
    Editor.setStatusText("Test 7: End of line navigation...");
    Editor.newFile();
    Editor.insert(0, "テスト\nTest");
    Editor.setCaretPos(0);
    Editor.moveCaretEnd();
    var pos = Editor.getCaretPos();
    var char = Editor.getText(pos - 1, 1);
    // The character before caret should be last char of first line
    assertTrue(pos > 0, "Caret should be at end of first line");

    Editor.setStatusText("Multi-byte Character Tests: PASSED");
    return "SUCCESS";
}

test_multibyte_chars();
