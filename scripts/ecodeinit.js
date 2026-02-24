// ecodeinit.js
// Initial startup script
Editor.logMessage("Loading ecodeinit.js...");

// Load Emacs bindings by default
try {
    Editor.loadScript("scripts/emacs.js");
    Editor.loadScript("scripts/ai.js");
    Editor.loadScript("scripts/setup_wizard.js");
} catch (e) {
    Editor.logMessage("Failed to load scripts: " + e);
}
