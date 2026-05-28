// ecodeinit.js
// Initial startup script
Editor.logMessage("Loading ecodeinit.js...");

// Load Notepad keybindings by default.
// To switch profiles, change the first loadScript to one of:
//   scripts/notepad.js   — standard Windows/Notepad bindings (default)
//   scripts/vscode.js    — Visual Studio Code bindings
//   scripts/emacs.js     — Emacs bindings
try {
    Editor.loadScript("scripts/notepad.js");
    Editor.loadScript("scripts/ai.js");
    Editor.loadScript("scripts/setup_wizard.js");
} catch (e) {
    Editor.logMessage("Failed to load scripts: " + e);
}
