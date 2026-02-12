# Implementation Plan - Native Win32 Text Editor

This plan outlines the steps to build a high-performance native Win32 text editor with the specified features.

## Phase 1: Setup and Dependencies
- [ ] Generate Duktape source files from the repository using `tools/configure.py`.
- [ ] Set up the Project structure (src, include, lib).
- [ ] Create a basic `main.cpp` to verify the build environment and window creation.

## Phase 2: Core Data Structures (Engine)
- [ ] **Piece Table**: Implement the core data structure to manage large text buffers efficiently.
- [ ] **Memory Mapped File**: Implement a class to handle loading/saving of huge files using memory mapping.
- [ ] **Safe File I/O**: Implement "save-to-temp-and-rename" strategy to prevent data loss.
- [ ] **Buffer Management**: Support for multiple buffers (tabs) and line ending detection (CRLF/LF/CR).


### Phase 3: Rendering System
- [ ] **DirectWrite Integration**: Initialize DirectWrite factory and resources.
- [ ] **Text Layout**: Implement text layout using `IDWriteTextLayout`.
- [ ] **Rendering Loop**: Implement the `WM_PAINT` handler to draw text using Direct2D/DirectWrite.
- [ ] **Font Handling**: 
    - [ ] Create a `FontManager` class to encapsulate DirectWrite font resources.
    - [ ] Implement font selection, sizing, and weight application.
    - [ ] Integration with the JS API to allow `editor.setFont(family, size, weight)`.

## Phase 4: Input and Interaction
- [ ] **Keyboard Handling**: Implement Emacs-like key bindings (Ctrl+F, Ctrl+B, Ctrl+N, Ctrl+P, etc.).
- [ ] **Mouse Handling**:
    - [ ] Caret positioning.
    - [ ] Standard selection (stream selection).
    - [ ] **Block/Box Selection**: Implement rectangular selection (Alt+Drag or similar logic).
    - [ ] Drag and Drop file support.
- [ ] **Clipboard**: Support Cut/Copy/Paste for both stream and block selections.

## Phase 5: Editor Features
- [ ] **Undo/Redo**: Implement an unrestricted undo/redo history stack.
- [ ] **Search and Replace**:
    - [ ] Simple string search.
    - [ ] Regex search (using `<regex>` or Duktape's engine if suitable, or a lightweight C++ lib).
- [ ] **Theming & Colors**: 
    - [ ] Implement a `ColorTheme` structure to store UI and syntax colors.
    - [ ] Expose `editor.setTheme(themeObject)` to JavaScript.
    - [ ] Implement basic syntax highlighting logic for common types.


## Phase 6: Scripting and Extensibility
- [ ] **Duktape Integration**: Embed the Duktape engine.
- [ ] **API Exposure**: Implement JS-to-C++ bindings for:
    - [ ] **File/Buffer**: Open, Save, Close, Switch Tab.
    - [ ] **Editing**: Insert, Delete, Replace, GetText.
    - [ ] **Cursor**: 
        - [ ] Movement logic for units: char, word, line, page, home/end.
        - [ ] Position calculation: Absolute index <-> Line/Col conversion.
        - [ ] Selection handling (Linear and Rectangular/Box).
        - [ ] Caret visibility and scroll-into-view logic.
    - [ ] **Visuals**: Toggle UI visibility (Tabs, Menu, Status), Fullscreen, Opacity controls.
    - [ ] **Keybindings**: Mapping strings like "Ctrl+S" to JS callbacks.
- [ ] **Initialization Script**: Load `ecodeinit.js` from the user's data folder.
- [ ] **Scratch Buffer**: 
    - [ ] Implement a "Scratch" buffer type that isn't bound to a file.
    - [ ] Add a command (`Ctrl+Enter`) to evaluate the current line or selection in the Duktape VM.
- [ ] **Macro Gallery**: Provide a set of built-in macros for common tasks.

## Phase 7: UI Polish
- [ ] **Menus**: File, Edit, View, Config (Settings/Themes/Edit Script), Tools, Language, Buffers, Help.
- [ ] **Context Menu**: Right-click menu.
- [ ] **Tabs**: UI for switching between open files.
- [ ] **Status Bar**: Show cursor position, encoding, etc.

## Phase 8: Localization
- [ ] **Localization infrastructure**: Implement a system to load translated strings from JSON or JS objects.
- [ ] **Language Support**: 
    - [ ] Add English (default).
    - [ ] Add Japanese translation.
    - [ ] Add Spanish translation.
    - [ ] Add French translation.
    - [ ] Add German translation.
- [ ] **UI Integration**: Dynamically update menus and dialogs when language is changed.

## Phase 9: Testing and refinement
- [ ] Test with huge files.
- [ ] Test Unicode support (UTF-8, UCS-2).
- [ ] Verify memory stability.
