function override_test_1() {
    Editor.logMessage("override_test_1 triggered");
}
function override_test_2() {
    Editor.logMessage("override_test_2 triggered");
}

Editor.newFile();
Editor.setBufferName("Buffer A");
Editor.setGlobalKeyBinding("Ctrl+M", "override_test_1");

Editor.newFile();
Editor.setBufferName("Buffer B");
Editor.setKeyBinding("Ctrl+M", "override_test_2");
