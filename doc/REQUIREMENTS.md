# Software Requirements Specification: Ecode

## 1. Functional Requirements

### 1.1 Core Engine & Data Management
*   **FR-1.1.1: Piece Table Implementation**: The editor shall use a Piece Table data structure to manage text buffers, ensuring $O(1)$ complexity for inserts and deletes regardless of file size.
*   **FR-1.1.2: Memory-Mapped Files**: Huge files shall be opened using Win32 Memory-Mapped Files (`CreateFileMapping`, `MapViewOfFile`) to allow editing files larger than available RAM without loading them entirely into memory.
    Over size: over 4Gbytes file.
*   **FR-1.1.3: Large File Support**: The system must remain responsive when handling files up to several gigabytes in size.

### 1.2 Text Rendering & Appearance
*   **FR-1.2.1: DirectWrite Integration**: All text rendering shall be performed via DirectWrite and Direct2D for high-quality, hardware-accelerated typography.
*   **FR-1.2.2: Color Customization**: 
    *   The editor shall support a themeable color system.
    *   Configurable elements include: Background, Foreground (default text), Caret, Selection highlight, Line numbers, and Syntax groups (Comments, Keywords, Strings, Numbers, Functions).
    *   Colors shall be specified in HEX or RGBA format via the JavaScript configuration.
*   **FR-1.2.3: Font Support**: 
    *   The editor shall support selection of any installed TrueType or OpenType font via DirectWrite.
    *   Configurable font properties: Font Family, Font Size (in points or pixels), Font Weight (Bold, Light, etc.), and Font Stretch.
    *   Fall-back mechanism: If a requested font is missing, the editor shall revert to a default monospaced system font (e.g., Consolas or Courier New).


### 1.3 Editing Features
*   **FR-1.3.1: Unrestricted Undo/Redo**: The editor shall maintain a complete history of edits, allowing for unlimited undo and redo operations within a session.
*   **FR-1.3.2: Search and Replace**:
    *   Support for literal string searching.
    *   Support for Regular Expression (regex) search and replace.
*   **FR-1.3.3: Selection Modes**:
    *   **Stream Selection**: Standard linear text selection.
    *   **Box/Block Selection**: Rectangular (columnar) selection for editing vertical blocks of text.
*   **FR-1.3.4: Clipboard Operations**: Support for Cut, Copy, and Paste for both stream and box selections.

### 1.4 Input & Interaction
*   **FR-1.4.1: Emacs-style Key Bindings**: Default navigation and editing shortcuts shall follow Emacs conventions (e.g., `Ctrl+F`, `Ctrl+B`, `Ctrl+N`, `Ctrl+P`, `Ctrl+A`, `Ctrl+E`).
*   **FR-1.4.2: Customizable Keymap**: Users shall be able to redefine any key binding via the configuration script.
*   **FR-1.4.3: Drag and Drop**: Dragging a file into the editor window shall open that file in a new tab.
*   **FR-1.4.4: Context Menu**: A right-click context menu shall provide quick access to common actions (Undo, Cut, Copy, Paste, Select All).

### 1.5 Multi-File Support
*   **FR-1.5.1: Tabbed Interface**: Multiple files can be open simultaneously, organized in a tab bar.
*   **FR-1.5.2: Buffer Management**: A "Buffers" menu shall allow users to switch between open files even if tabs are hidden.

### 1.6 File Encodings
*   **FR-1.6.1: UTF-8 Support**: Full support for UTF-8 encoding, with and without Byte Order Mark (BOM).
*   **FR-1.6.2: UCS-2 (UTF-16) Support**: Support for UCS-2 Little Endian and Big Endian encodings.
*   **FR-1.6.3: Line Endings**: Support for Windows (CRLF), Unix (LF), and Classic Mac (CR) line endings.

### 1.7 Scripting & Extensibility
*   **FR-1.7.1: Duktape Integration**: Embed the Duktape JavaScript engine to allow user scripts to interact with the editor.
*   **FR-1.7.2: Initialization Script**: On startup, the editor shall load and execute `%APPDATA%/Ecode/ecodeinit.js` to set user preferences, key bindings, and macros.
*   **FR-1.7.3: API Exposure**: The following categories shall be exposed to the JavaScript environment:
    *   **File Management**: `newFile()`, `open(path)`, `save()`, `saveAs(path)`, `close()`.
    *   **Buffer Management**: `getBuffers()`, `switchBuffer(index)`, `getBufferPath()`.
    *   **Key Bindings**: `setKeyBinding(keyCombo, jsFunctionString)`, `clearKeyBindings()`.
    *   **Text Editing**: `insert(text)`, `delete(count)`, `getText(start, end)`, `replace(start, end, text)`.
    *   **Cursor & Selection**: 
        *   `moveCursor(direction, unit, count, extendSelection)`: 
            *   Direction: `up`, `down`, `left`, `right`.
            *   Unit: `char`, `word`, `line`, `page`, `lineStart`, `lineEnd`, `bufferStart`, `bufferEnd`.
        *   `getCursorPos()`: Returns `{ line, col, index }`.
        *   `setCursorPos(line, col)` or `setCursorPos(index)`.
        *   `getSelection()`: Returns `{ start, end, isRectangular }`.
        *   `setSelection(anchor, focus, isRectangular)`.
        *   `scrollCursorIntoView()`.
    *   **Visibility & Visuals**: `showTabs(bool)`, `showStatusBar(bool)`, `showMenuBar(bool)`, `setOpacity(level)`, `toggleFullScreen()`.
*   **FR-1.7.4: Scratch Buffer**: The editor shall support a special "Scratch Buffer" where users can type JavaScript code and execute it directly within the editor context.
*   **FR-1.7.5: Macro Support**: Provide an API for JavaScript macros to perform complex text manipulations by combining core editing functions.

### 1.8 Localization
*   **FR-1.8.1: Multi-language UI**: The editor shall support UI localization for English, Japanese, Spanish, French, and German.
*   **FR-1.8.2: Dynamic Language Switching**: Users shall be able to switch the editor's language at runtime without restarting the application.
*   **FR-1.8.3: Scriptable Translations**: Translation strings shall be manageable via JavaScript to allow for easy community-driven localization.

## 2. Interface Requirements

### 2.1 Menu Bar Structure
The main menu bar shall contain the following top-level menus:
*   **File**: New, Open, Save, Save As, Close, Exit.
*   **Edit**: Undo, Redo, Cut, Copy, Paste, Search/Replace.
*   **View**: Toggle UI elements, Zoom.
*   **Config**: Settings, Theme selection, Edit ecodeinit.js.
*   **Tools**: Run Macro, Script Console.
*   **Language**: Select UI language (English, Japanese, Spanish, French, German).
*   **Buffers**: List of open files.
*   **Help**: Documentation, About.

## 3. Non-Functional Requirements

### 3.1 Performance
*   **NFR-3.1.1: Latency**: Text entry and cursor movement should have zero perceived lag even in large files.
*   **NFR-3.1.2: Resource Usage**: Memory consumption should be proportional to the visible/edited parts of the file, not the total file size (due to memory mapping).

### 3.2 Reliability
*   **NFR-3.2.1: Safe Save**: Implement a "save-to-temp-and-rename" strategy to ensure file integrity in case of a crash during saving.
*   **NFR-3.2.2: Stability**: The editor must handle invalid file encodings or malformed regex patterns gracefully without crashing.

### 3.3 Platform
*   **NFR-3.3.1**: The application shall be a native 64-bit Win32 executable.


### menu bar

file - new, open, save, save as, close, exit
       open dialogue, save as dialogue
       recent files, recent folders
Edit - undo, redo, cut, copy, paste, select all, find, find next, find previous, replace, replace all
       find dialogue, replace dialogue
View - toggle UI elements, zoom
       zoom in, zoom out, zoom reset
Config - settings, theme selection, edit ecodeinit.js
       settings dialogue, theme selection dialogue
Tools - run macro, script console
       macro gallery
Language - select UI language (English, Japanese, Spanish, French, German)
       language selection dialogue
Buffers - list of open files
       buffer selection dialogue
       switch buffers buffer
Help - documentation, about
       documentation dialogue, about dialogue   


###
