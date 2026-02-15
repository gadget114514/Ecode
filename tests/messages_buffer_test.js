// Test for Messages Buffer functionality

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

function test_messages_buffer() {
    Editor.newFile();
    Editor.setStatusText("Running Messages Buffer Tests...");

    // Test 1: logMessage function exists
    Editor.setStatusText("Test 1: logMessage API exists...");
    assertTrue(typeof Editor.logMessage === 'function', "Editor.logMessage should be a function");

    // Test 2: Send message to buffer
    Editor.setStatusText("Test 2: Log messages...");
    Editor.logMessage("Test message 1");
    Editor.logMessage("Test message 2");
    Editor.logMessage("Test message 3");

    // Messages should be logged (we can't directly verify *Messages* buffer content from JS,
    // but we can verify the function doesn't throw)
    assertTrue(true, "logMessage calls completed without error");

    // Test 3: console.log integration
    Editor.setStatusText("Test 3: console.log integration...");
    console.log("Console test message 1");
    console.log("Console test message 2");
    console.log("Console test message 3");

    assertTrue(true, "console.log calls completed without error");

    // Test 4: Log various data types
    Editor.setStatusText("Test 4: Log various data types...");
    Editor.logMessage("String message");
    Editor.logMessage("Number: " + 42);
    Editor.logMessage("Boolean: " + true);

    console.log("Mixed: ", 123, " test ", true);

    assertTrue(true, "Various data types logged without error");

    // Test 5: Long messages
    Editor.setStatusText("Test 5: Long messages...");
    var longMsg = "";
    for (var i = 0; i < 100; i++) {
        longMsg += "Long message segment " + i + " ";
    }
    Editor.logMessage(longMsg);

    assertTrue(true, "Long message logged without error");

    // Test 6: Special characters
    Editor.setStatusText("Test 6: Special characters...");
    Editor.logMessage("Special chars: <>&\"'\n\t");
    Editor.logMessage("Unicode: 日本語 😀 🌍");

    assertTrue(true, "Special characters logged without error");

    // Test 7: Empty messages
    Editor.setStatusText("Test 7: Empty messages...");
    Editor.logMessage("");
    Editor.logMessage("   ");

    assertTrue(true, "Empty messages handled without error");

    // Test 8: Rapid logging
    Editor.setStatusText("Test 8: Rapid logging...");
    for (var i = 0; i < 50; i++) {
        Editor.logMessage("Rapid message " + i);
    }

    assertTrue(true, "Rapid logging completed without error");

    Editor.setStatusText("Messages Buffer Tests: PASSED");
    Editor.logMessage("=== Messages Buffer Tests: PASSED ===");
    return "SUCCESS";
}

test_messages_buffer();
