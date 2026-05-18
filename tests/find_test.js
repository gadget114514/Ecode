// Tests for Editor find/query APIs

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

function test_find_apis() {
    Editor.newFile();
    Editor.setStatusText("Running Find API Tests...");

    // Test 1: Basic insert/getLength/getText
    Editor.setStatusText("Test 1: Basic text operations...");
    Editor.insert(0, "The quick brown fox");
    assertEqual(Editor.getLength(), 19, "Length of 'The quick brown fox'");
    assertEqual(Editor.getText(0, 3), "The", "First 3 chars should be 'The'");
    assertEqual(Editor.getText(16, 3), "fox", "Last 3 chars should be 'fox'");

    // Test 2: Get line information
    Editor.setStatusText("Test 2: Line information...");
    Editor.newFile();
    Editor.insert(0, "Line 1\nLine 2\nLine 3");
    assertEqual(Editor.getTotalLines(), 3, "Should have 3 lines");
    var line2Offset = Editor.getLineOffset(1);
    assertTrue(line2Offset > 0, "Line 2 offset should be > 0");
    var lineAtOffset = Editor.getLineAtOffset(7);
    assertEqual(lineAtOffset, 1, "Offset 7 should be on line 1");

    // Test 3: getLineAtOffset boundaries
    Editor.setStatusText("Test 3: Offset boundaries...");
    assertEqual(Editor.getLineAtOffset(0), 0, "Offset 0 should be line 0");

    // Test 4: Get line offset positions
    Editor.setStatusText("Test 4: Line offsets...");
    var off0 = Editor.getLineOffset(0);
    var off1 = Editor.getLineOffset(1);
    var off2 = Editor.getLineOffset(2);
    assertEqual(off0, 0, "First line offset should be 0");
    assertTrue(off1 > off0, "Line 1 offset should be after line 0");
    assertTrue(off2 > off1, "Line 2 offset should be after line 1");

    // Test 5: find API
    Editor.setStatusText("Test 5: Find API...");
    Editor.newFile();
    Editor.insert(0, "The quick brown fox jumps over the lazy dog");
    var pos = Editor.find("fox", 0, true, false, true);
    assertEqual(pos, 16, "Find 'fox' should return 16");
    pos = Editor.find("dog", 0, true, false, true);
    assertEqual(pos, 40, "Find 'dog' should return 40");
    pos = Editor.find("nonexistent", 0, true, false, true);
    assertEqual(pos, -1, "Find nonexistent should return -1");

    Editor.setStatusText("Find API Tests: PASSED");
    return "SUCCESS";
}

test_find_apis();
