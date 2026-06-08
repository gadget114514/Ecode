// ecodeinit.js
// Initial startup script
Editor.logMessage("Loading ecodeinit.js...");

// Load VS Code bindings by default
try {
    Editor.loadScript("scripts/vscode.js");
    Editor.loadScript("scripts/ai.js");
    Editor.loadScript("scripts/setup_wizard.js");
    Editor.loadScript("scripts/wizard_engine.js");
} catch (e) {
    Editor.logMessage("Failed to load scripts: " + e);
}
