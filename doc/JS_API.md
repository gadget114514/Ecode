# 📜 Ecode JavaScript API Reference

The `Editor` global object provides access to the Ecode engine. These functions can be used in `ecodeinit.js` or in the Scratch buffer.

## Editor Object

### 🏗 Core Editing
- `Editor.insert(pos, text)`: Inserts `text` at byte `pos`.
- `Editor.delete(pos, len)`: Deletes `len` bytes starting from `pos`.
- `Editor.getText(pos, len)`: Returns the text (string) from the buffer.
- `Editor.getLength()`: Returns total buffer length in bytes.
- `Editor.getTotalLines()`: Returns total number of lines.
- `Editor.getLineAtOffset(offset)`: Returns the 0-based line index for a byte offset.
- `Editor.getLineOffset(lineIndex)`: Returns the starting byte offset of the given line.

### 📍 Caret & Selection
- `Editor.getCaretPos()`: Returns current caret position (byte offset).
- `Editor.setCaretPos(pos)`: Sets caret position.
- `Editor.moveCaret(delta)`: Moves caret by `delta` bytes.
- `Editor.moveCaretUp()`: Moves caret to the previous line.
- `Editor.moveCaretDown()`: Moves caret to the next line.
- `Editor.moveCaretHome()`: Moves caret to the start of the current line.
- `Editor.moveCaretEnd()`: Moves caret to the end of the current line.
- `Editor.setSelectionAnchor(pos)`: Sets the selection anchor (start of selection).

### 📋 Clipboard & History
- `Editor.copy()`: Copies selected text to system clipboard.
- `Editor.cut()`: Cuts selected text to system clipboard.
- `Editor.paste()`: Pastes text from system clipboard.
- `Editor.undo()`: Undoes the last operation.
- `Editor.redo()`: Redoes the last undone operation.

### 📁 File & Buffer Management
- `Editor.open(path)`: Opens the file at the specified path.
- `Editor.openDialog()`: Shows the Win32 Open File dialog and returns the selected path.
- `Editor.saveDialog()`: Shows the Win32 Save File dialog and returns the selected path.

### 🎨 Visuals & UI
- `Editor.setFont(family, size)`: Updates the editor font.
- `Editor.showLineNumbers(bool)`: Toggles gutter line numbers.
- `Editor.showPhysicalLineNumbers(bool)`: Toggles physical (pre-folding) line numbers.
- `Editor.setStatusText(text)`: Sets text in the first part of the status bar.
- `Editor.setProgress(value)`: Sets the progress bar (0-100).
- `Editor.showAbout()`: Shows the About dialog.

### ⌨️ Key Bindings
- `Editor.setKeyBinding(chord, funcName)`: Binds a keyboard chord (e.g., `"Ctrl+S"`) to a JavaScript function name.
- `Editor.setCaptureKeyboard(bool)`: Redirects all keyboard input to the JS key handler.
- `Editor.setKeyHandler(funcName)`: Sets the function to handle captured keyboard events.
- `Editor.find(query, startPos, forward, useRegex, matchCase)`: Searches for text in the buffer. Returns byte offset or -1.
- `Editor.switchBuffer(index)`: Switches to the buffer at the given index.
- `Editor.close()`: Closes the current active buffer.

---
*Note: Some APIs listed in `ecodeinit.js.example` (like `setTheme`, `toggleFullScreen`) are currently in development and may not be fully functional.*
