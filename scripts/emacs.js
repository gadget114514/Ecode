// Emacs-style Key Bindings
console.log("Loading Emacs key bindings...");
Editor.setGlobalKeyBinding("Ctrl+N", "emacs_next_line");
Editor.setGlobalKeyBinding("Ctrl+P", "emacs_prev_line");
Editor.setGlobalKeyBinding("Ctrl+F", "emacs_forward_char");
Editor.setGlobalKeyBinding("Ctrl+B", "emacs_backward_char");
Editor.setGlobalKeyBinding("Ctrl+A", "emacs_line_start");
Editor.setGlobalKeyBinding("Ctrl+E", "emacs_line_end");
Editor.setGlobalKeyBinding("Ctrl+D", "emacs_delete_char");
Editor.setGlobalKeyBinding("Ctrl+H", "emacs_backspace");
Editor.setGlobalKeyBinding("Ctrl+K", "emacs_kill_line");
Editor.setGlobalKeyBinding("Ctrl+Y", "emacs_yank");
Editor.setGlobalKeyBinding("Ctrl+Space", "emacs_set_mark");
Editor.setGlobalKeyBinding("Ctrl+V", "emacs_scroll_down");
Editor.setGlobalKeyBinding("Alt+V", "emacs_scroll_up");
Editor.setGlobalKeyBinding("Ctrl+T", "emacs_transpose_chars");
Editor.setGlobalKeyBinding("Ctrl+S", "emacs_isearch_forward");
Editor.setGlobalKeyBinding("Ctrl+R", "emacs_isearch_backward");
Editor.setGlobalKeyBinding("Ctrl+G", "emacs_quit");
Editor.setGlobalKeyBinding("Ctrl+W", "emacs_kill_region");
Editor.setGlobalKeyBinding("Alt+<", "emacs_beginning_of_buffer");
Editor.setGlobalKeyBinding("Alt+>", "emacs_end_of_buffer");
Editor.setGlobalKeyBinding("Alt+Y", "emacs_yank_pop");
Editor.setGlobalKeyBinding("Alt+W", "emacs_copy_region");
Editor.setGlobalKeyBinding("Alt+X", "emacs_execute_extended_command");
Editor.setGlobalKeyBinding("Alt+Z", "emacs_redo");
Editor.setGlobalKeyBinding("F12", "tag_jump");

// Kill ring implementation (Emacs-style kill/yank system)
var killRing = [];
var killRingMax = 60;
var killRingYankPointer = 0;
var lastYankStart = -1;
var lastYankEnd = -1;
var lastCommandWasYank = false;
var isMarkActive = false;

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

function emacs_next_line() { Editor.moveCaretDown(isMarkActive); }
function emacs_prev_line() { Editor.moveCaretUp(isMarkActive); }
function emacs_forward_char() { Editor.moveCaretByChar(1, isMarkActive); }
function emacs_backward_char() { Editor.moveCaretByChar(-1, isMarkActive); }
function emacs_line_start() { Editor.moveCaretHome(isMarkActive); }
function emacs_line_end() { Editor.moveCaretEnd(isMarkActive); }
function emacs_delete_char() { Editor.deleteChar(); }
function emacs_backspace() { Editor.backspace(); }
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
            pushToKillRing("\n");
            Editor.delete(pos, 1);
        } else if (newlinePos > 0) {
            var killText = text.substring(0, newlinePos);
            pushToKillRing(killText);
            Editor.delete(pos, newlinePos);
        } else {
            pushToKillRing(text);
            Editor.delete(pos, len);
        }
    }
    lastCommandWasYank = false;
}

function pushToKillRing(text) {
    if (text && text.length > 0) {
        // Remove duplicates of the same text at front
        if (killRing.length > 0 && killRing[0] === text) {
            return; // Don't add duplicate
        }
        // Add to front of ring
        killRing.unshift(text);
        // Limit ring size
        if (killRing.length > killRingMax) {
            killRing.pop();
        }
        killRingYankPointer = 0;
    }
}

function emacs_yank() {
    if (killRing.length > 0) {
        var text = killRing[0];
        var startPos = Editor.getCaretPos();
        Editor.insert(startPos, text);
        lastYankStart = startPos;
        lastYankEnd = startPos + text.length;
        Editor.setCaretPos(lastYankEnd);
        killRingYankPointer = 0;
        lastCommandWasYank = true;
    }
}

function emacs_yank_pop() {
    if (!lastCommandWasYank || killRing.length < 2) {
        return; // yank-pop only works after yank
    }
    // Remove the previously yanked text
    if (lastYankStart >= 0 && lastYankEnd >= lastYankStart) {
        Editor.delete(lastYankStart, lastYankEnd - lastYankStart);
        Editor.setCaretPos(lastYankStart);
    }
    // Cycle to next item in kill ring
    killRingYankPointer = (killRingYankPointer + 1) % killRing.length;
    var text = killRing[killRingYankPointer];
    var startPos = Editor.getCaretPos();
    Editor.insert(startPos, text);
    lastYankStart = startPos;
    lastYankEnd = startPos + text.length;
    Editor.setCaretPos(lastYankEnd);
    lastCommandWasYank = true;
}

function emacs_set_mark() {
    Editor.setSelectionAnchor(Editor.getCaretPos());
    isMarkActive = true;
    Editor.setStatusText("Mark set");
}

function emacs_scroll_up() {
    for (var i = 0; i < 20; i++) Editor.moveCaretUp(isMarkActive);
}
function emacs_scroll_down() {
    for (var i = 0; i < 20; i++) Editor.moveCaretDown(isMarkActive);
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


function emacs_kill_region() {
    if (Editor.hasSelection && Editor.hasSelection()) {
        var startPos = Editor.getSelectionStart ? Editor.getSelectionStart() : Editor.getCaretPos();
        var endPos = Editor.getSelectionEnd ? Editor.getSelectionEnd() : Editor.getSelectionAnchor ? Editor.getSelectionAnchor() : Editor.getCaretPos();
        if (startPos > endPos) {
            var tmp = startPos;
            startPos = endPos;
            endPos = tmp;
        }
        var text = Editor.getText(startPos, endPos - startPos);
        pushToKillRing(text);
    }
    Editor.cut();
    lastCommandWasYank = false;
}

function emacs_copy_region() {
    if (Editor.hasSelection && Editor.hasSelection()) {
        var startPos = Editor.getSelectionStart ? Editor.getSelectionStart() : Editor.getCaretPos();
        var endPos = Editor.getSelectionEnd ? Editor.getSelectionEnd() : Editor.getSelectionAnchor ? Editor.getSelectionAnchor() : Editor.getCaretPos();
        if (startPos > endPos) {
            var tmp = startPos;
            startPos = endPos;
            endPos = tmp;
        }
        var text = Editor.getText(startPos, endPos - startPos);
        pushToKillRing(text);
    }
    Editor.copy();
    lastCommandWasYank = false;
}

function emacs_beginning_of_buffer() {
    Editor.setCaretPos(0);
    Editor.setSelectionAnchor(0);
}

function emacs_end_of_buffer() {
    var endPos = Editor.getLength();
    Editor.setCaretPos(endPos);
    if (!isMarkActive) Editor.setSelectionAnchor(endPos);
}

function emacs_quit() {
    if (isMarkActive) {
        isMarkActive = false;
        Editor.setSelectionAnchor(Editor.getCaretPos());
        Editor.setStatusText("Quit (Mark deactivated)");
    } else {
        Editor.setStatusText("Quit");
    }
    Editor.setCaptureKeyboard(false);
}

function emacs_jump_to_line() { Editor.showJumpToLine(); }

function emacs_execute_extended_command() {
    Editor.showMinibuffer("M-x ", "mx");
}

// Standard Windows CUA (Common User Access) keybindings
// Ctrl+V and Ctrl+Y are preserved as Emacs bindings (scroll down and yank)
console.log("Adding Windows CUA keybindings...");
Editor.setGlobalKeyBinding("Ctrl+X", "cua_cut");
Editor.setGlobalKeyBinding("Ctrl+C", "cua_copy");
Editor.setGlobalKeyBinding("Ctrl+Z", "cua_undo");

function cua_cut() { Editor.cut(); }
function cua_copy() { Editor.copy(); }
function cua_undo() { Editor.undo(); }
function emacs_redo() { Editor.redo(); }

function emacs_eval_line_or_selection() {
    var code = "";
    var isSelection = false;
    if (Editor.hasSelection && Editor.hasSelection()) {
        isSelection = true;
        var start = Editor.getSelectionStart ? Editor.getSelectionStart() : Editor.getCaretPos();
        var end = Editor.getSelectionEnd ? Editor.getSelectionEnd() : Editor.getSelectionAnchor ? Editor.getSelectionAnchor() : Editor.getCaretPos();
        if (start > end) { var t = start; start = end; end = t; }
        code = Editor.getText(start, end - start);
    } else {
        var pos = Editor.getCaretPos();
        var lineIdx = Editor.getLineAtOffset(pos);
        var start = Editor.getLineOffset(lineIdx);
        var end = Editor.getLineOffset(lineIdx + 1);
        if (end == 0) end = Editor.getLength();
        code = Editor.getText(start, end - start);
    }

    if (code) {
        var result = "Error";
        try {
            // Use eval in the global scope?
            // ScriptEngine doesn't expose 'eval' directly to JS context the way we might want,
            // but we can use Function constructor or just assume the engine handles it?
            // Wait, ScriptEngine.cpp exposes 'Evaluate' method to C++, but not to JS directly except via eval().
            // But we are IN JS. So just eval().
            result = String(eval(code));
        } catch (e) {
            result = "Error: " + e.toString();
        }

        Editor.logMessage("Eval: " + code + " -> " + result);

        var parsed = result;
        // Append result to next line
        var endPos = Editor.getLength();
        Editor.setCaretPos(endPos);
        Editor.insert(endPos, "\n// > " + parsed + "\n");
        Editor.setCaretPos(Editor.getLength());
    }
}

Editor.setGlobalKeyBinding("Ctrl+Enter", "emacs_eval_line_or_selection");
Editor.setGlobalKeyBinding("Ctrl+/", "emacs_redo");

// Shell mode
var emacs_shell_cmd = "cmd.exe";
function emacs_shell() {
    Editor.openShell(emacs_shell_cmd);
    lastCommandWasYank = false;
}

Editor.setGlobalKeyBinding("Alt+X", "emacs_execute_command");

// M-x command registry
var emacs_mx_commands = {
    "shell": emacs_shell,
    "eval": function () { Editor.showMinibuffer("Eval: ", "eval"); },
    "switch-to-buffer": emacs_switch_to_buffer,
    "find-file": emacs_find_file,
    "kill-buffer": emacs_kill_buffer
};

// M-x: show minibuffer with prompt "M-x " in MX command mode
function emacs_execute_command() {
    Editor.showMinibuffer("M-x ", "mx");
}

// C-x prefix implementation
Editor.setGlobalKeyBinding("Ctrl+X", "emacs_ctrl_x_prefix");

function emacs_ctrl_x_prefix() {
    Editor.setCaptureKeyboard(true);
    Editor.setStatusText("C-x-");
    Editor.setKeyHandler(function (key, isChar) {
        Editor.setCaptureKeyboard(false);
        Editor.setKeyHandler(null);
        Editor.setStatusText("");

        if (key === "f" || key === "Ctrl+F") {
            emacs_find_file();
            return true;
        } else if (key === "b" || key === "Ctrl+B") {
            emacs_switch_to_buffer();
            return true;
        } else if (key === "k" || key === "Ctrl+K") {
            emacs_kill_buffer();
            return true;
        } else if (key === "Ctrl+S") {
            Editor.save();
            return true;
        } else if (key === "Ctrl+C") {
            // Emacs exit? For now just exit... 
            // In a real editor this might prompt.
            return true;
        } else if (key === "Ctrl+G") {
            Editor.setStatusText("Quit");
            return true;
        }
        return false;
    });
}

function emacs_switch_to_buffer() {
    Editor.showMinibuffer("Switch to buffer: ", "callback", "on_switch_buffer_input");
}

function on_switch_buffer_input(input) {
    if (!input) return;
    var buffers = Editor.getBuffers();
    var target = input.trim();

    // Search by name or index
    for (var i = 0; i < buffers.length; i++) {
        var b = buffers[i];
        if (b.path.endsWith(target) || b.path === target) {
            Editor.switchBuffer(b.index);
            return;
        }
    }

    // If not found, maybe it's an index
    var idx = parseInt(target);
    if (!isNaN(idx) && idx >= 0 && idx < buffers.length) {
        Editor.switchBuffer(idx);
    } else {
        Editor.setStatusText("No buffer matching: " + target);
    }
}

// Completion for switch-to-buffer
function on_switch_buffer_input_complete(input) {
    var buffers = Editor.getBuffers();
    var matches = [];
    for (var i = 0; i < buffers.length; i++) {
        var name = buffers[i].path;
        if (!name) name = buffers[i].isScratch ? "*scratch*" : "Untitled";

        // Strip path for easier matching
        var lastSlash = Math.max(name.lastIndexOf('/'), name.lastIndexOf('\\'));
        var shortName = (lastSlash != -1) ? name.substring(lastSlash + 1) : name;

        if (shortName.startsWith(input)) matches.push(shortName);
        else if (name.startsWith(input)) matches.push(name);
    }

    if (matches.length == 0) return "";
    if (matches.length == 1) return matches[0];

    // Find common prefix
    var common = matches[0];
    for (var i = 1; i < matches.length; i++) {
        while (matches[i].indexOf(common) !== 0) common = common.substring(0, common.length - 1);
    }
    return common;
}

function emacs_find_file() {
    var path = Editor.openDialog();
    if (path) {
        Editor.open(path);
    }
}

function emacs_kill_buffer() {
    Editor.close();
}

// Goto line M-g g
Editor.setGlobalKeyBinding("Alt+G", "emacs_goto_line_prefix");
function emacs_goto_line_prefix() {
    Editor.setCaptureKeyboard(true);
    Editor.setStatusText("M-g-");
    Editor.setKeyHandler(function (key, isChar) {
        Editor.setCaptureKeyboard(false);
        Editor.setKeyHandler(null);
        Editor.setStatusText("");
        if (key === "g" || key === "G") {
            Editor.showMinibuffer("Goto line: ", "callback", "on_goto_line_input");
            return true;
        }
        return false;
    });
}

function on_goto_line_input(input) {
    var line = parseInt(input);
    if (!isNaN(line)) {
        Editor.jumpToLine(line);
    }
}

// M-! shell-command
Editor.setGlobalKeyBinding("Alt+Shift+1", "emacs_shell_command"); // Alt+!
function emacs_shell_command() {
    Editor.showMinibuffer("Shell command: ", "callback", "on_shell_command_input");
}

function on_shell_command_input(input) {
    if (!input) return;
    Editor.openShell("cmd.exe /c " + input);
}
