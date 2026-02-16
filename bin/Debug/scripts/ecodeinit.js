// ecodeinit.js
// Initial startup script
Editor.logMessage("Loading ecodeinit.js...");

// Load Emacs bindings by default
try {
    Editor.loadScript("scripts/emacs.js");
} catch (e) {
    Editor.logMessage("Failed to load emacs.js: " + e);
}
