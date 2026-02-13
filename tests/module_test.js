// Ecode Module API Test Suite
// This script tests theme management, buffer switching, and UI toggles

function test_module_apis() {
    Editor.setStatusText("Running Module API Tests...");

    // Test: New Buffer
    Editor.newFile();
    Editor.insert(0, "Testing New File API\n");

    // Test: Theme Application
    Editor.setTheme({
        background: "#2d2d2d",
        foreground: "#cccccc",
        caret: "#ff0000",
        selection: "#444444",
        lineNumbers: "#666666"
    });
    Editor.insert(Editor.getLength(), "Theme applied: Dark Gray\n");

    // Test: UI Visibility (if exposed)
    if (Editor.showTabs) Editor.showTabs(false);
    if (Editor.showTabs) Editor.showTabs(true);

    // Test: Buffer Switch (switching back to previous buffer if possible)
    // Note: Assuming at least 2 buffers exist now
    Editor.switchBuffer(0);
    Editor.insert(Editor.getLength(), "Switched back to Buffer 0\n");

    Editor.setStatusText("Module API Tests Completed.");
}

test_module_apis();
