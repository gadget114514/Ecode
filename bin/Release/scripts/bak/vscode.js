// Visual Studio Code-style Key Bindings
console.log("Loading VS Code key bindings...");

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+N", "file_new");
Editor.setKeyBinding("Ctrl+O", "file_open");
Editor.setKeyBinding("Ctrl+S", "file_save");
Editor.setKeyBinding("Ctrl+Shift+S", "file_save_as");
Editor.setKeyBinding("Ctrl+W", "file_close");
Editor.setKeyBinding("Ctrl+Tab", "buffer_next");
Editor.setKeyBinding("Ctrl+Shift+Tab", "buffer_prev");
Editor.setKeyBinding("Ctrl+Shift+T", "buffer_reopen_closed");

function file_new() { Editor.newFile(); }
function file_open() { var p = Editor.openDialog(); if (p) Editor.open(p); }
function file_save() { Editor.save(); }
function file_save_as() { var p = Editor.saveDialog(); if (p) Editor.saveAs(p); }
function file_close() { Editor.close(); }
function buffer_next() {
    var bufs = Editor.getBuffers();
    var cur = -1;
    for (var i = 0; i < bufs.length; i++) { if (bufs[i].active) { cur = i; break; } }
    if (cur >= 0) Editor.switchBuffer((cur + 1) % bufs.length);
}
function buffer_prev() {
    var bufs = Editor.getBuffers();
    var cur = -1;
    for (var i = 0; i < bufs.length; i++) { if (bufs[i].active) { cur = i; break; } }
    if (cur >= 0) Editor.switchBuffer((cur - 1 + bufs.length) % bufs.length);
}
function buffer_reopen_closed() { Editor.logMessage("Reopen closed buffer not implemented"); }

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+G", "goto_line");
Editor.setKeyBinding("Ctrl+D", "add_selection_to_next_find_match");
Editor.setKeyBinding("F12", "goto_definition");
Editor.setKeyBinding("Alt+Left", "navigate_back");
Editor.setKeyBinding("Alt+Right", "navigate_forward");
Editor.setKeyBinding("Ctrl+Minus", "navigate_back");
Editor.setKeyBinding("Ctrl+Shift+Minus", "navigate_forward");

function goto_line() { Editor.showJumpToLine(); }
function add_selection_to_next_find_match() {
    var pos = Editor.getCaretPos();
    var selEnd = Editor.getSelectionAnchor();
    if (selEnd == pos) {
        // No selection: select word at cursor
        var lineIdx = Editor.getLineAtOffset(pos);
        var start = Editor.getLineOffset(lineIdx);
        var len = Editor.getLength();
        var text = Editor.getText(0, len);
        // Find word boundaries
        var wordStart = pos;
        while (wordStart > 0 && text.charCodeAt(wordStart - 1) > 32) wordStart--;
        var wordEnd = pos;
        while (wordEnd < text.length && text.charCodeAt(wordEnd) > 32) wordEnd++;
        if (wordEnd > wordStart) {
            Editor.setSelectionAnchor(wordStart);
            Editor.setCaretPos(wordEnd);
        }
    } else {
        // Already have selection: find next occurrence
        var start = Math.min(pos, selEnd);
        var end = Math.max(pos, selEnd);
        var word = Editor.getText(start, end - start);
        if (word && word.length > 0) {
            var found = Editor.find(word, end, true, true, false);
            if (found >= 0) {
                Editor.setSelectionAnchor(found);
                Editor.setCaretPos(found + word.length);
            }
        }
    }
}
function goto_definition() { Editor.logMessage("Go to Definition: F12"); }
function navigate_back() { Editor.logMessage("Navigate back: Alt+Left"); }
function navigate_forward() { Editor.logMessage("Navigate forward: Alt+Right"); }

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+Slash", "toggle_comment");
Editor.setKeyBinding("Ctrl+Shift+K", "delete_line");
Editor.setKeyBinding("Ctrl+Enter", "insert_line_below");
Editor.setKeyBinding("Ctrl+Shift+Enter", "insert_line_above");
Editor.setKeyBinding("Alt+Up", "move_line_up");
Editor.setKeyBinding("Alt+Down", "move_line_down");
Editor.setKeyBinding("Ctrl+Shift+Backslash", "jump_to_bracket");

function toggle_comment() {
    var pos = Editor.getCaretPos();
    var lineIdx = Editor.getLineAtOffset(pos);
    var start = Editor.getLineOffset(lineIdx);
    var end = Editor.getLineOffset(lineIdx + 1);
    if (end == 0) end = Editor.getLength();
    var text = Editor.getText(start, end - start);
    if (text.indexOf("//") === 0) {
        Editor.delete(start, 2);
    } else {
        Editor.insert(start, "//");
    }
}
function delete_line() {
    var pos = Editor.getCaretPos();
    var lineIdx = Editor.getLineAtOffset(pos);
    var start = Editor.getLineOffset(lineIdx);
    var end = Editor.getLineOffset(lineIdx + 1);
    if (end == 0) end = Editor.getLength();
    if (end > start) {
        Editor.delete(start, end - start);
        Editor.setCaretPos(start);
    }
}
function insert_line_below() {
    var pos = Editor.getCaretPos();
    var lineIdx = Editor.getLineAtOffset(pos);
    var end = Editor.getLineOffset(lineIdx + 1);
    if (end == 0) end = Editor.getLength();
    Editor.setCaretPos(end);
    Editor.insert(end, "\n");
}
function insert_line_above() {
    var pos = Editor.getCaretPos();
    var lineIdx = Editor.getLineAtOffset(pos);
    var start = Editor.getLineOffset(lineIdx);
    Editor.setCaretPos(start);
    Editor.insert(start, "\n");
    Editor.setCaretPos(start);
}
function move_line_up() {
    var pos = Editor.getCaretPos();
    var li = Editor.getLineAtOffset(pos);
    if (li <= 0) return;
    var s1 = Editor.getLineOffset(li);
    var e1 = Editor.getLineOffset(li + 1);
    if (e1 == 0) e1 = Editor.getLength();
    var s0 = Editor.getLineOffset(li - 1);
    var e0 = Editor.getLineOffset(li);
    var t1 = Editor.getText(s1, e1 - s1);
    var t0 = Editor.getText(s0, e0 - s0);
    Editor.delete(s0, e1 - s0);
    Editor.insert(s0, t1 + (t1.indexOf("\n") < 0 ? "\n" : "") + t0.replace(/\n$/, ""));
    Editor.setCaretPos(s0 + (pos - s1));
}
function move_line_down() {
    var pos = Editor.getCaretPos();
    var li = Editor.getLineAtOffset(pos);
    var total = Editor.getTotalLines();
    if (li >= total - 1) return;
    var s1 = Editor.getLineOffset(li);
    var e1 = Editor.getLineOffset(li + 1);
    if (e1 == 0) e1 = Editor.getLength();
    var s2 = Editor.getLineOffset(li + 1);
    var e2 = Editor.getLineOffset(li + 2);
    if (e2 == 0) e2 = Editor.getLength();
    var t1 = Editor.getText(s1, e1 - s1);
    var t2 = Editor.getText(s2, e2 - s2);
    Editor.delete(s1, e2 - s1);
    Editor.insert(s1, t2 + (t2.indexOf("\n") < 0 ? "\n" : "") + t1.replace(/\n$/, ""));
    Editor.setCaretPos(s1 + (pos - s1) + (e2 - s2));
}
function jump_to_bracket() { Editor.logMessage("Jump to matching bracket"); }

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+A", "select_all");
Editor.setKeyBinding("Ctrl+Shift+L", "select_all_occurrences");
Editor.setKeyBinding("Ctrl+L", "select_current_line");

function select_all() {
    var len = Editor.getLength();
    Editor.setSelectionAnchor(0);
    Editor.setCaretPos(len);
}
function select_all_occurrences() {
    Editor.logMessage("Select all occurrences: Ctrl+Shift+L");
}
function select_current_line() {
    var pos = Editor.getCaretPos();
    var li = Editor.getLineAtOffset(pos);
    var start = Editor.getLineOffset(li);
    var end = Editor.getLineOffset(li + 1);
    if (end == 0) end = Editor.getLength();
    Editor.setSelectionAnchor(start);
    Editor.setCaretPos(end);
}

// ---------------------------------------------------------------------------
// Find / Search
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+F", "find_open");
Editor.setKeyBinding("Ctrl+H", "find_replace_open");
Editor.setKeyBinding("Ctrl+Shift+F", "find_in_files_open");
Editor.setKeyBinding("F3", "find_next");
Editor.setKeyBinding("Shift+F3", "find_prev");
Editor.setKeyBinding("Ctrl+Shift+H", "find_replace_in_files");

function find_open() { Editor.runCommand("find"); }
function find_replace_open() { Editor.runCommand("replace"); }
function find_in_files_open() { Editor.runCommand("find_in_files"); }
function find_next() { Editor.runCommand("find_next"); }
function find_prev() { Editor.runCommand("find_prev"); }
function find_replace_in_files() { Editor.logMessage("Find and replace in files"); }

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+Z", "undo");
Editor.setKeyBinding("Ctrl+Y", "redo");
Editor.setKeyBinding("Ctrl+Shift+Z", "redo");

function undo() { Editor.undo(); }
function redo() { Editor.redo(); }

// ---------------------------------------------------------------------------
// Copy / Paste / Cut (standard CUA)
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+X", "cut");
Editor.setKeyBinding("Ctrl+C", "copy");
Editor.setKeyBinding("Ctrl+V", "paste");

function cut() { Editor.cut(); }
function copy() { Editor.copy(); }
function paste() { Editor.paste(); }

// ---------------------------------------------------------------------------
// View / Toggle
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+B", "toggle_sidebar");
Editor.setKeyBinding("Ctrl+J", "toggle_panel");
Editor.setKeyBinding("Ctrl+Shift+E", "show_explorer");
Editor.setKeyBinding("Ctrl+Backtick", "toggle_terminal");
Editor.setKeyBinding("Ctrl+K Ctrl+F", "format_selection");
Editor.setKeyBinding("Shift+Alt+F", "format_document");
Editor.setKeyBinding("Ctrl+Equal", "zoom_in");
Editor.setKeyBinding("Ctrl+Shift+Equal", "zoom_in");
Editor.setKeyBinding("Ctrl+Minus", "zoom_out");
Editor.setKeyBinding("Ctrl+Shift+Minus", "zoom_out");

function toggle_sidebar() { Editor.runCommand("toggle_sidebar"); }
function toggle_panel() { Editor.runCommand("toggle_panel"); }
function show_explorer() { Editor.runCommand("show_explorer"); }
function toggle_terminal() { Editor.runCommand("toggle_terminal"); }
function format_selection() { Editor.runCommand("format_selection"); }
function format_document() { Editor.runCommand("format_document"); }
function zoom_in() { Editor.runCommand("zoom_in"); }
function zoom_out() { Editor.runCommand("zoom_out"); }

// ---------------------------------------------------------------------------
// Quick Open / Command Palette
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+P", "quick_open");
Editor.setKeyBinding("Ctrl+Shift+P", "command_palette");

function quick_open() { Editor.showMinibuffer("Go to file: ", "file_search"); }
function command_palette() { Editor.showMinibuffer("> ", "mx"); }

// ---------------------------------------------------------------------------
// Shell / Terminal
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+Shift+C", "open_terminal_cmd");
Editor.setKeyBinding("Ctrl+Shift+V", "open_terminal_powershell");

function open_terminal_cmd() { Editor.openShell("cmd"); }
function open_terminal_powershell() { Editor.openShell("powershell"); }

// ---------------------------------------------------------------------------
// Zoom
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+0", "reset_zoom");
function reset_zoom() { Editor.runCommand("reset_zoom"); }

// ---------------------------------------------------------------------------
// Default overrides for hardcoded Emacs-style C++ bindings
// These override C++ hardcoded keys to match VS Code behavior
// ---------------------------------------------------------------------------

// Ctrl+A should select all (not go to line start)
// Ctrl+F should open find (not forward char)
// Ctrl+B should toggle sidebar (not backward char)
// Ctrl+P should be quick open (not previous line)
// Ctrl+N should be new file (not next line)
// Ctrl+E should be find recent (not line end)
// Note: some of these are handled in C++ before JS bindings
