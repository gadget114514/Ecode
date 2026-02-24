var vendor = Editor.getAIVendor ? Editor.getAIVendor() : "Gemini";
var model = Editor.getAIModel ? Editor.getAIModel() : "gemini-1.5-pro";

Editor.newFile();
Editor.insert(0, "Test AI session Binding!\n");

Editor.logMessage("Keybinding typeof: " + typeof Editor.setKeyBinding);

if (Editor.setKeyBinding) {
    Editor.setKeyBinding("Ctrl+Enter", "ai_provider_ask");
    Editor.setKeyBinding("Ctrl+Return", "ai_provider_ask");
    Editor.logMessage("Bound local keys.");
} else {
    Editor.logMessage("FAILED: setKeyBinding is missing from Editor object!");
}
