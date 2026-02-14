function assertEqual(actual, expected, msg) {
    if (actual !== expected) {
        throw "ASSERT FAILED: " + msg + " (Expected '" + expected + "', Got '" + actual + "')";
    }
}

function test_undo_redo() {
    Editor.newFile();
    Editor.setStatusText("Running Undo/Redo API Tests...");

    var states = [];
    states.push(""); // Initial state

    var edits = [
        "Hello",
        " World",
        "!",
        "\nNew Line",
        " and more"
    ];

    // 1. Perform edits and record states
    var currentText = "";
    for (var i = 0; i < edits.length; i++) {
        Editor.insert(Editor.getLength(), edits[i]);
        currentText += edits[i];
        states.push(currentText);
    }

    assertEqual(Editor.getText(0, Editor.getLength()), states[states.length - 1], "Initial state capture failed");

    // 2. Full Undo Loop
    Editor.setStatusText("Testing Full Undo Loop...");
    for (var i = states.length - 2; i >= 0; i--) {
        if (!Editor.canUndo()) throw "Line 33: Expected to be able to undo at step " + i;
        Editor.undo();
        var actual = Editor.getText(0, Editor.getLength());
        assertEqual(actual, states[i], "Line 36: Content mismatch at undo step " + i);
    }
    assertEqual(Editor.canUndo(), false, "Line 38: Should not be able to undo at start");

    // 3. Full Redo Loop
    Editor.setStatusText("Testing Full Redo Loop...");
    for (var i = 1; i < states.length; i++) {
        if (!Editor.canRedo()) throw "Line 43: Expected to be able to redo at step " + i;
        Editor.redo();
        var actual = Editor.getText(0, Editor.getLength());
        assertEqual(actual, states[i], "Line 46: Content mismatch at redo step " + i);
    }
    assertEqual(Editor.canRedo(), false, "Line 48: Should not be able to redo at end");

    // 4. Verification after complete loop
    assertEqual(Editor.getText(0, Editor.getLength()), states[states.length - 1], "Line 51: Final content mismatch after full loops");

    Editor.setStatusText("Undo/Redo Tests: PASSED");
    return "SUCCESS";
}

test_undo_redo();
