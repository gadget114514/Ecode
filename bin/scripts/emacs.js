// Emacs-style Key Bindings
Editor.setKeyBinding("Ctrl+N", "emacs_next_line");
Editor.setKeyBinding("Ctrl+P", "emacs_prev_line");
Editor.setKeyBinding("Ctrl+F", "emacs_forward_char");
Editor.setKeyBinding("Ctrl+B", "emacs_backward_char");
Editor.setKeyBinding("Ctrl+A", "emacs_line_start");
Editor.setKeyBinding("Ctrl+E", "emacs_line_end");
Editor.setKeyBinding("Ctrl+D", "emacs_delete_char");
Editor.setKeyBinding("Ctrl+H", "emacs_backspace");
Editor.setKeyBinding("Ctrl+K", "emacs_kill_line");
Editor.setKeyBinding("Ctrl+Y", "emacs_yank");
Editor.setKeyBinding("Ctrl+Space", "emacs_set_mark");
Editor.setKeyBinding("Ctrl+V", "emacs_scroll_down");
Editor.setKeyBinding("Alt+V", "emacs_scroll_up");
Editor.setKeyBinding("Ctrl+T", "emacs_transpose_chars");
Editor.setKeyBinding("Ctrl+S", "emacs_isearch_forward");
Editor.setKeyBinding("Ctrl+R", "emacs_isearch_backward");
Editor.setKeyBinding("Ctrl+G", "emacs_jump_to_line");
Editor.setKeyBinding("F12", "tag_jump");

var isearchQuery = "";
var isearchStartPos = 0;
var isearchForward = true;

function emacs_isearch_forward() {
    isearchQuery = "";
    isearchStartPos = Editor.getCaretPos();
    isearchForward = true;
    Editor.setCaptureKeyboard(true);
    Editor.setKeyHandler("emacs_isearch_handler");
    Editor.setStatusText("I-search: ");
}

function emacs_isearch_backward() {
    isearchQuery = "";
    isearchStartPos = Editor.getCaretPos();
    isearchForward = false;
    Editor.setCaptureKeyboard(true);
    Editor.setKeyHandler("emacs_isearch_handler");
    Editor.setStatusText("I-search backward: ");
}

function emacs_isearch_handler(key, isChar) {
    if (!isChar) {
        if (key == "Enter") {
            Editor.setCaptureKeyboard(false);
            Editor.setStatusText("Ready");
            return true;
        }
        if (key == "Esc") {
            Editor.setCaretPos(isearchStartPos);
            Editor.setSelectionAnchor(isearchStartPos);
            Editor.setCaptureKeyboard(false);
            Editor.setStatusText("Canceled");
            return true;
        }
        if (key == "Backspace") {
            if (isearchQuery.length > 0) {
                isearchQuery = isearchQuery.substring(0, isearchQuery.length - 1);
                emacs_isearch_update();
            }
            return true;
        }
        if (key == "Ctrl+S") {
            isearchForward = true;
            emacs_isearch_next();
            return true;
        }
        if (key == "Ctrl+R") {
            isearchForward = false;
            emacs_isearch_next();
            return true;
        }
        return false;
    }

    isearchQuery += key;
    emacs_isearch_update();
    return true;
}

function emacs_isearch_update() {
    var prefix = isearchForward ? "I-search: " : "I-search backward: ";
    Editor.setStatusText(prefix + isearchQuery);

    var pos = Editor.find(isearchQuery, isearchStartPos, isearchForward, false, false);
    if (pos != -1) {
        Editor.setSelectionAnchor(pos);
        Editor.setCaretPos(pos + isearchQuery.length);
    }
}

function emacs_isearch_next() {
    var currentPos = Editor.getCaretPos();
    var searchStart = isearchForward ? currentPos : (currentPos - isearchQuery.length - 1);
    if (searchStart < 0) searchStart = 0;

    var pos = Editor.find(isearchQuery, searchStart, isearchForward, false, false);
    if (pos != -1) {
        Editor.setSelectionAnchor(pos);
        Editor.setCaretPos(pos + isearchQuery.length);
    } else {
        Editor.setStatusText("Failing " + (isearchForward ? "I-search: " : "I-search backward: ") + isearchQuery);
    }
}

function emacs_next_line() { Editor.moveCaretDown(); }
function emacs_prev_line() { Editor.moveCaretUp(); }
function emacs_forward_char() { Editor.moveCaretByChar(1); }
function emacs_backward_char() { Editor.moveCaretByChar(-1); }
function emacs_line_start() { Editor.moveCaretHome(); }
function emacs_line_end() { Editor.moveCaretEnd(); }
function emacs_delete_char() { Editor.delete(Editor.getCaretPos(), 1); }
function emacs_backspace() {
    var pos = Editor.getCaretPos();
    if (pos > 0) {
        Editor.setCaretPos(pos - 1);
        Editor.delete(pos - 1, 1);
    }
}
function emacs_kill_line() {
    var pos = Editor.getCaretPos();
    var lineIdx = Editor.getLineAtOffset(pos);
    var lineEnd = Editor.getLineOffset(lineIdx + 1);
    if (lineEnd == 0) lineEnd = Editor.getLength();

    var len = lineEnd - pos;
    if (len > 0) {
        var text = Editor.getText(pos, len);
        var newlinePos = text.indexOf('\n');
        if (newlinePos == 0) {
            Editor.delete(pos, 1);
        } else if (newlinePos > 0) {
            Editor.setSelectionAnchor(pos);
            Editor.setCaretPos(pos + newlinePos);
            Editor.cut();
        } else {
            Editor.setSelectionAnchor(pos);
            Editor.setCaretPos(pos + len);
            Editor.cut();
        }
    }
}
function emacs_yank() { Editor.paste(); }
function emacs_set_mark() { Editor.setSelectionAnchor(Editor.getCaretPos()); }

function emacs_scroll_up() {
    for (var i = 0; i < 20; i++) Editor.moveCaretUp();
}
function emacs_scroll_down() {
    for (var i = 0; i < 20; i++) Editor.moveCaretDown();
}
function emacs_transpose_chars() {
    var pos = Editor.getCaretPos();
    if (pos > 0 && pos < Editor.getLength()) {
        var c1 = Editor.getText(pos - 1, 1);
        var c2 = Editor.getText(pos, 1);
        Editor.delete(pos - 1, 2);
        Editor.insert(pos - 1, c2 + c1);
        Editor.setCaretPos(pos + 1);
    }
}

function tag_jump() {
    var pos = Editor.getCaretPos();
    var lineIdx = Editor.getLineAtOffset(pos);
    var start = Editor.getLineOffset(lineIdx);
    var end = Editor.getLineOffset(lineIdx + 1);
    if (end == 0) end = Editor.getLength();

    var text = Editor.getText(start, end - start);
    var match = text.match(/([a-zA-Z0-9_\-\.\/\\]+)[:\(](\d+)[:\)]?/);
    if (match) {
        var file = match[1];
        var line = parseInt(match[2]);
        Editor.open(file);
        var targetOffset = Editor.getLineOffset(line - 1);
        Editor.setCaretPos(targetOffset);
        Editor.setSelectionAnchor(targetOffset);
    }
}

function emacs_jump_to_line() { Editor.showJumpToLine(); }
