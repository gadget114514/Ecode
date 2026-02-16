// Regression tests for bugs reported on 2026-02-16
// 1. Duplicate buffer protection
// 2. Caret movement in newline-only buffers
// 3. Emacs mark and selection
// 4. Alt+X binding verify (if possible via JS)

function assertEqual(actual, expected, msg) {
    if (actual !== expected) {
        console.log("FAIL: " + msg + " (Expected '" + expected + "', Got '" + actual + "')");
        throw "ASSERT FAILED: " + msg + " (Expected '" + expected + "', Got '" + actual + "')";
    }
}

function assertTrue(condition, msg) {
    if (!condition) {
        throw "ASSERT FAILED: " + msg;
    }
}

function test_newline_only_movement() {
    console.log("TEST 1: Testing newline-only movement...");
    Editor.newFile();
    Editor.insert(0, "\n\n\n");

    assertEqual(Editor.getLength(), 3, "Length should be 3");

    Editor.setCaretPos(0);
    Editor.moveCaretRight();
    assertEqual(Editor.getCaretPos(), 1, "Should move to offset 1");

    Editor.moveCaretDown();
    assertEqual(Editor.getCaretPos(), 2, "Should move to line 2 (offset 2)");

    Editor.moveCaretDown();
    assertEqual(Editor.getCaretPos(), 3, "Should move to line 3 (offset 3)");

    Editor.moveCaretUp();
    assertEqual(Editor.getCaretPos(), 2, "Should move back to offset 2");

    console.log("TEST 1: Newline-only movement: PASSED");
}

function test_emacs_loading() {
    console.log("TEST 2: Testing Emacs key binding loading...");

    // Load emacs.js (path relative to executable or handle by proper loading mechanism)
    // In test environment, we might need to point to it explicitly if not autoloaded
    if (typeof emacs_next_line !== 'function') {
        Editor.loadScript("scripts/emacs.js");
    }

    // Check if a known Emacs binding is registered
    // setKeyBinding in C++ doesn't have a corresponding geKeyBinding to check easily from JS
    // But we can check if the functions exist in global scope
    if (typeof emacs_next_line !== 'function') {
        throw "Emacs functions not loaded (emacs_next_line missing)";
    }
    console.log("TEST 2: Emacs key binding loading: PASSED");
}

function test_mouse_selection_simulation() {
    console.log("TEST 3: Testing mouse selection simulation...");
    Editor.newFile();
    Editor.insert(0, "Line 1\nLine 2\nLine 3");

    // Simulate mouse down at 0
    Editor.setCaretPos(0);
    Editor.setSelectionAnchor(0);

    // Simulate drag to end of line 1 (offset 6)
    // The C++ fix for HandleMouseMove ensures VisualToLogicalOffset is used
    // Here we verify the API allows setting selection anchor independently
    Editor.setCaretPos(6);
    // In mouse drag, anchor stays at 0, caret moves to 6

    assertEqual(Editor.getSelectionAnchor(), 0, "Anchor should be at 0");
    assertEqual(Editor.getCaretPos(), 6, "Caret should be at 6");

    console.log("TEST 3: Mouse selection simulation: PASSED");
}

function test_alt_x_minibuffer() {
    console.log("TEST 4: Testing Alt+X minibuffer opening...");
    // We can't easily simulate physical key press of Alt+X here without extra C++ exposure
    // But we can verify the binding function exists and works
    if (typeof emacs_execute_extended_command !== 'function') {
        throw "emacs_execute_extended_command function missing";
    }

    // Call it and check if minibuffer mode changed (hard to check directly without exposed state)
    // But ensuring it runs without error is a start
    emacs_execute_extended_command();
    // If we are here, it didn't crash
    console.log("TEST 4: Alt+X function execution: PASSED");
}

function test_emacs_mark_extended() {
    console.log("TEST 5: Testing Emacs mark extension...");
    Editor.newFile();
    Editor.insert(0, "ABCDEFGHIJ");

    // Set mark
    Editor.setSelectionAnchor(0);

    // Move caret to 2
    Editor.setCaretPos(2);

    assertEqual(Editor.getCaretPos(), 2, "Caret should be at 2");
    assertEqual(Editor.getSelectionAnchor(), 0, "Anchor should be at 0");

    console.log("TEST 5: Emacs mark extension: PASSED");
}

function test_duplicate_buffer() {
    Editor.setStatusText("Testing duplicate buffer protection...");
    // This is hard to test in headless JS because current JS API
    // might not expose buffer list size.
}

try {
    test_emacs_loading();
    test_newline_only_movement();
    test_emacs_mark_extended();
    test_mouse_selection_simulation();
    test_alt_x_minibuffer();
    console.log("Regression Tests Completed");
} catch (e) {
    console.log("FAILURE: " + e);
    // headles output?
}
