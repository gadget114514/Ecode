# 📜 Ecode JavaScript API Reference

The `Editor` global object provides access to the Ecode engine. These functions can be used in `ecodeinit.js` or in the Scratch buffer.

## Editor Object

### 🏗 Core Editing
- `Editor.insert(pos: number, text: string)`
    - **Description**: Inserts `text` at byte `pos`.
    - **Return**: `boolean` `true` if successful.
- `Editor.delete(pos: number, len: number)`
    - **Description**: Deletes `len` bytes starting from `pos`.
    - **Return**: `boolean` `true` if successful.
- `Editor.getText(pos: number, len: number)`
    - **Description**: Returns the text from the buffer.
    - **Return**: `string` The requested substring.
- `Editor.getLength()`
    - **Description**: Returns total buffer length in bytes.
    - **Return**: `number` Total bytes.
- `Editor.getTotalLines()`
    - **Description**: Returns total number of lines.
    - **Return**: `number` Total lines.
- `Editor.getLineAtOffset(offset: number)`
    - **Description**: Returns the 0-based line index for a byte offset.
    - **Return**: `number` Line index.
- `Editor.getLineOffset(lineIndex: number)`
    - **Description**: Returns the starting byte offset of the given line.
    - **Return**: `number` Byte offset.

### 📍 Caret & Navigation
- `Editor.getCaretPos()`
    - **Description**: Returns current caret position (byte offset).
    - **Return**: `number` Byte offset.
- `Editor.setCaretPos(pos: number)`
    - **Description**: Sets caret position.
    - **Return**: `boolean` `true` if successful.
- `Editor.moveCaret(delta: number)`
    - **Description**: Moves caret by `delta` bytes.
    - **Return**: `boolean` `true` if successful.
- `Editor.moveCaretByChar(delta: number)`
    - **Description**: Moves caret by `delta` UTF-8 characters.
    - **Return**: `boolean` `true` if successful.
- `Editor.moveCaretUp()`
    - **Description**: Moves caret to the previous line.
    - **Return**: `boolean` `true` if successful.
- `Editor.moveCaretDown()`
    - **Description**: Moves caret to the next line.
    - **Return**: `boolean` `true` if successful.
- `Editor.moveCaretHome()`
    - **Description**: Moves caret to the start of the current line.
    - **Return**: `boolean` `true` if successful.
- `Editor.moveCaretEnd()`
    - **Description**: Moves caret to the end of the current line.
    - **Return**: `boolean` `true` if successful.
- `Editor.setSelectionAnchor(pos: number)`
    - **Description**: Sets the selection anchor (start of selection).
    - **Return**: `boolean` `true` if successful.
- `Editor.jumpToLine(line: number)`
    - **Description**: Navigates to the specified 1-based line number.
    - **Return**: `boolean` `true` if successful.
- `Editor.showJumpToLine()`
    - **Description**: Shows the Jump to Line dialog.
    - **Return**: `boolean` `true` if successful.
- `Editor.setSelectionMode(mode: number)`
    - **Description**: Sets the selection mode (0: Standard, 1: Box).
    - **Return**: `boolean` `true` if successful.

### 📋 Clipboard & History
- `Editor.copy()`
    - **Description**: Copies selected text to system clipboard.
    - **Return**: `boolean` `true` if successful.
- `Editor.cut()`
    - **Description**: Cuts selected text to system clipboard.
    - **Return**: `boolean` `true` if successful.
- `Editor.paste()`
    - **Description**: Pastes text from system clipboard.
    - **Return**: `boolean` `true` if successful.
- `Editor.undo()`
    - **Description**: Undoes the last operation.
    - **Return**: `boolean` `true` if successful.
- `Editor.redo()`
    - **Description**: Redoes the last undone operation.
    - **Return**: `boolean` `true` if successful.

### 📁 File & Buffer Management
- `Editor.open(path: string)`
    - **Description**: Opens the file at the specified path.
    - **Return**: `boolean` `true` if successful.
- `Editor.openDialog()`
    - **Description**: Shows the Win32 Open File dialog.
    - **Return**: `string` The selected path, or empty string if canceled.
- `Editor.saveDialog()`
    - **Description**: Shows the Win32 Save File dialog.
    - **Return**: `string` The selected path, or empty string if canceled.
- `Editor.getBuffers()`
    - **Description**: Returns a list of all open buffers.
    - **Return**: `object[]` Array of buffer objects: `{ index: number, path: string, isDirty: boolean, isScratch: boolean }`.
- `Editor.switchBuffer(index: number)`
    - **Description**: Switches the active view to the buffer at `index`.
    - **Return**: `boolean` `true` if successful.
- `Editor.close()`
    - **Description**: Closes the current active buffer.
    - **Return**: `boolean` `true` if successful.
- `Editor.newFile()`
    - **Description**: Creates a new untitled buffer.
    - **Return**: `boolean` `true` if successful.
- `Editor.execSync(command: string)`
    - **Description**: Executes a shell command synchronously and captures its `stdout` and `stderr` output.
    - **Return**: `string` The text output from the executed command, or an error message if the execution fails.

### 🎨 Visuals & UI
- `Editor.setFont(family: string, size: number, weight?: number)`
    - **Description**: Updates the editor font. `weight` is an optional DWRITE_FONT_WEIGHT value (e.g., 400 for Normal, 700 for Bold).
    - **Return**: `boolean` `true` if successful.
- `Editor.setLigatures(enable: boolean)`
    - **Description**: Enables or disables standard font ligatures.
    - **Return**: `boolean` The previous state.
- `Editor.showLineNumbers(show: boolean)`
    - **Description**: Sets visibility of gutter line numbers.
    - **Return**: `boolean` The previous state.
- `Editor.showPhysicalLineNumbers(show: boolean)`
    - **Description**: Sets visibility of physical (pre-folding) line numbers.
    - **Return**: `boolean` The previous state.
- `Editor.setStatusText(text: string)`
    - **Description**: Sets text in the first part of the status bar.
    - **Return**: `boolean` `true` if successful.
- `Editor.setProgress(value: number)`
    - **Description**: Sets the progress bar (0-100).
    - **Return**: `boolean` `true` if successful.
- `Editor.showAbout()`
    - **Description**: Shows the About dialog.
    - **Return**: `boolean` `true` if successful.
- `Editor.setTheme(theme: object)`
    - **Description**: Sets the editor theme colors.
    - **Return**: `boolean` `true` if successful.
- `Editor.toggleFullScreen()`
	- **Description**: Toggles the application fullscreen mode.
	- **Return**: `boolean` The previous state.
- `Editor.setOpacity(opacity: number)`
	- **Description**: Sets the window transparency (0.0 to 1.0).
	- **Return**: `boolean` `true` if successful.
- `Editor.showTabs(show: boolean)`
    - **Description**: Sets visibility of the tab control.
    - **Return**: `boolean` The previous state.
- `Editor.showStatusBar(show: boolean)`
    - **Description**: Sets visibility of the status bar.
    - **Return**: `boolean` The previous state.
- `Editor.showMenuBar(show: boolean)`
    - **Description**: Sets visibility of the menu bar.
    - **Return**: `boolean` The previous state.
- `Editor.setWordWrap(wrap: boolean)`
    - **Description**: Sets word wrap mode.
    - **Return**: `boolean` The previous state.
- `Editor.setWrapWidth(width: number)`
    - **Description**: Sets the word wrap width (0 for window width).
    - **Return**: `boolean` `true` if successful.
- `Editor.setHighlights(ranges: object[])`
    - **Description**: Applies syntax highlighting. `ranges` is an array of `{ start: number, length: number, type: number }`.
    - **Return**: `boolean` `true` if successful.

### ⌨️ Key Bindings
- `Editor.setKeyBinding(chord: string, funcName: string)`
    - **Description**: Binds a keyboard chord (e.g., `"Ctrl+S"`) to a JavaScript function name.
    - **Return**: `boolean` `true` if successful.
- `Editor.setCaptureKeyboard(capture: boolean)`
    - **Description**: Redirects all keyboard input to the JS key handler.
    - **Return**: `boolean` `true` if successful.
- `Editor.setKeyHandler(funcName: string)`
    - **Description**: Sets the function to handle captured keyboard events.
    - **Return**: `boolean` `true` if successful.
- `Editor.find(query: string, startPos: number, forward: boolean, useRegex: boolean, matchCase: boolean)`
    - **Description**: Searches for text in the buffer.
    - **Return**: `number` Byte offset of the match, or -1 if not found.

---
*Note: APIs are subject to change as the engine evolves.*
