// Notepad-style Key Bindings
console.log("Loading Notepad key bindings...");

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+N", "np_file_new");
Editor.setKeyBinding("Ctrl+O", "np_file_open");
Editor.setKeyBinding("Ctrl+S", "np_file_save");
Editor.setKeyBinding("Ctrl+Shift+S", "np_file_save_as");
Editor.setKeyBinding("Ctrl+W", "np_file_close");

function np_file_new() { Editor.newFile(); }
function np_file_open() { var p = Editor.openDialog(); if (p) Editor.open(p); }
function np_file_save() { Editor.save(); }
function np_file_save_as() { var p = Editor.saveDialog(); if (p) Editor.saveAs(p); }
function np_file_close() { Editor.close(); }

// ---------------------------------------------------------------------------
// Edit — Undo / Redo
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+Z", "np_undo");
Editor.setKeyBinding("Ctrl+Y", "np_redo");

function np_undo() { Editor.undo(); }
function np_redo() { Editor.redo(); }

// ---------------------------------------------------------------------------
// Edit — Clipboard
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+X", "np_cut");
Editor.setKeyBinding("Ctrl+C", "np_copy");
Editor.setKeyBinding("Ctrl+V", "np_paste");

function np_cut()   { Editor.cut(); }
function np_copy()  { Editor.copy(); }
function np_paste() { Editor.paste(); }

// ---------------------------------------------------------------------------
// Edit — Selection
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+A", "np_select_all");

function np_select_all() {
    var len = Editor.getLength();
    Editor.setSelectionAnchor(0);
    Editor.setCaretPos(len);
}

// ---------------------------------------------------------------------------
// Edit — Find / Replace
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+F", "np_find");
Editor.setKeyBinding("Ctrl+H", "np_replace");
Editor.setKeyBinding("F3",     "np_find_next");
Editor.setKeyBinding("Shift+F3", "np_find_prev");
Editor.setKeyBinding("Ctrl+Shift+F", "np_find_in_files");

function np_find()         { Editor.command("find"); }
function np_replace()      { Editor.command("replace"); }
function np_find_next()    { Editor.command("find_next"); }
function np_find_prev()    { Editor.command("find_prev"); }
function np_find_in_files() { Editor.command("find_in_files"); }

// ---------------------------------------------------------------------------
// Navigation — Go To Line (Ctrl+G in Windows 11 Notepad)
// Note: Ctrl+G cancels special modes first; reaches here in normal editing mode.
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+G", "np_goto_line");

function np_goto_line() { Editor.showJumpToLine(); }

// ---------------------------------------------------------------------------
// Navigation — Document start / end (Ctrl+Home / Ctrl+End)
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+Home", "np_doc_home");
Editor.setKeyBinding("Ctrl+End",  "np_doc_end");
Editor.setKeyBinding("Ctrl+Shift+Home", "np_doc_home_extend");
Editor.setKeyBinding("Ctrl+Shift+End",  "np_doc_end_extend");

function np_doc_home() {
    Editor.setSelectionAnchor(0);
    Editor.setCaretPos(0);
}
function np_doc_end() {
    var len = Editor.getLength();
    Editor.setSelectionAnchor(len);
    Editor.setCaretPos(len);
}
function np_doc_home_extend() {
    Editor.setCaretPos(0);
}
function np_doc_end_extend() {
    Editor.setCaretPos(Editor.getLength());
}

// ---------------------------------------------------------------------------
// View — Zoom
// ---------------------------------------------------------------------------
Editor.setKeyBinding("Ctrl+=", "np_zoom_in");
Editor.setKeyBinding("Ctrl+-", "np_zoom_out");
Editor.setKeyBinding("Ctrl+0", "np_zoom_reset");

function np_zoom_in()    { Editor.command("zoom_in"); }
function np_zoom_out()   { Editor.command("zoom_out"); }
function np_zoom_reset() { Editor.command("zoom_reset"); }

// ---------------------------------------------------------------------------
// View — Fullscreen (F11)
// Already handled in C++; also bind here for JS completeness.
// ---------------------------------------------------------------------------
Editor.setKeyBinding("F11", "np_toggle_fullscreen");
function np_toggle_fullscreen() { Editor.toggleFullscreen(); }

// ---------------------------------------------------------------------------
// Insert — Date / Time (F5, classic Notepad feature)
// ---------------------------------------------------------------------------
Editor.setKeyBinding("F5", "np_insert_datetime");

function np_insert_datetime() {
    var now = new Date();
    var s = now.toLocaleString();
    var pos = Editor.getCaretPos();
    Editor.insert(pos, s);
    Editor.setCaretPos(pos + s.length);
}
