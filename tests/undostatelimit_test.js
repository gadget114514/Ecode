function test_undo_limit() {
    Editor.newFile();
    Editor.setStatusText("Running Undo Limit Test...");

    Editor.insert(0, "Initial\n");

    var LIMIT = 500;
    var EXTRA = 100;

    for (var i = 1; i <= LIMIT + EXTRA; i++) {
        Editor.insert(Editor.getLength(), "Edit " + i + "\n");
    }

    // We should be able to undo exactly LIMIT times
    var undoCount = 0;
    while (Editor.canUndo()) {
        Editor.undo();
        undoCount++;
        if (undoCount > LIMIT + EXTRA + 10) break; // Safety
    }

    if (undoCount !== LIMIT) {
        throw "ASSERT FAILED: Line 25: Undo limit mismatch. Expected " + LIMIT + ", Got " + undoCount;
    }

    Editor.setStatusText("Undo Limit Test: PASSED");
    return "SUCCESS";
}

test_undo_limit();
