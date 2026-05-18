# 📝 Ecode - Pending Features & Roadmap

This document tracks the detailed progress of Ecode features based on [REQUIREMENTS.md](REQUIREMENTS.md).

## 🟢 Completed / Substantially Functional
- [x] **FR-1.1.1: Piece Table Implementation**: Core data structure with $O(1)$ edits.
- [x] **FR-1.1.2: Memory-Mapped Files**: Efficient loading of large files.
- [x] **FR-1.2.1: DirectWrite Integration**: High-quality text rendering with D2D.
- [x] **FR-1.3.1: Unrestricted Undo/Redo**: Infinite history via Piece Table snapshots.
- [x] **FR-1.3.4: Clipboard Operations**: Cut, Copy, Paste via Win32 API for stream selection.
- [x] **FR-1.5.2: Buffer Management**: Dynamic 'Buffers' menu for switching files.
- [x] **FR-1.6.1/1.6.2: Encoding Support**: UTF-8 (BOM/No-BOM), UTF-16 LE/BE detection and conversion to UTF-8 internal.
- [x] **FR-1.8.1/1.8.2: Localization**: L10N system for multiple languages with runtime switching (L10N macro).
- [x] **NFR-3.2.1: Safe Save**: Atomic-ish saving via temp files and renaming.

## 🟡 Partially Implemented / In Progress
- [x] **FR-1.2.2: Color Customization**: 
  - [x] Implement theme structure (JSON/JS based).
  - [x] Connect theme colors to `Renderer` (Done).
  - [x] Support syntax highlighting colors (Done).
- [x] **FR-1.2.3: Font Support**:
  - [x] `Renderer::SetFont` implemented.
  - [x] UI/JS API to change font family/size/weight dynamically.
- [x] **FR-1.3.3: Selection Modes**:
  - [x] Stream Selection (Done).
  - [x] Box/Block (Rectangular) Selection logic and rendering.
- [x] **FR-1.7.3: API Exposure**:
  - [x] Basic `Evaluate` and script execution (Done).
  - [x] Implement `moveCursor`, `getCursorPos`, `setCursorPos`, `getSelection`, `setSelection` (Done).
  - [x] Implement File/Buffer management APIs (`open`, `close`, `switchBuffer`).
- [x] **FR-1.1.3: Large File Support**: 
  - [x] Implemented viewport-based rendering (only renders visible lines).
  - [x] Memory usage now O(viewport_size) instead of O(file_size).
  - [x] Rendering performance constant regardless of file size.
  - [x] Works seamlessly with memory-mapped files for true multi-GB file support.
- [x] **FR-1.2.4: Text Wrapping**:
  - [x] Word wrap logic in `Renderer` (Done).
  - [x] Configurable wrap width (Done).
  - [x] Localization-aware wrapping (Kinsho shori, etc.) (Done).
  - [x] Configuration dialogue for wrap size (Done).

## 🔴 Pending Implementation (High Priority)
- [x] **FR-1.3.2: Search and Replace**:
  - [x] Implement literal search.
  - [x] Integrate Win32 Find/Replace standard dialogs.
  - [x] Implement Regex search (using `<regex>`).
- [x] **FR-1.4.1: Emacs-style Key Bindings**: Implement default `Ctrl+F/B/N/P/A/E` mappings in `ecodeinit.js` or `HandleBinding`.
- [x] **FR-1.4.3: Drag and Drop**: Implement `WM_DROPFILES` handler.
- [x] **FR-1.4.4: Context Menu**: Implement `WM_CONTEXTMENU` with Cut/Copy/Paste/Undo/Select All.
- [x] **FR-1.5.1: Tabbed Interface**: Implement a UI container for tabs (using Win32 Tab Control).
- [x] **FR-1.7.2: Initialization Script**: Load and execute `%APPDATA%/Ecode/ecodeinit.js` on startup.
- [x] **FR-1.2.5: Gutter & Line Numbers (Enhancements)**:
  - [x] Basic drawing (Done).
  - [x] Dynamic width based on line count (Done).
  - [x] Physical line numbers (Done).
  - [x] Mouse clicking in gutter for line selection.
- [x] **FR-1.2.6: Caret Blinking**: Use a Win32 timer (`SetTimer`) to toggle caret visibility.
- [x] **FR-1.9.1: Settings Persistence**: Save/Load window position, active language, theme to a config file.
- [x] **Tabbed Settings Dialogue**: Modern UI for General and AI settings.
- [x] **AI Server Configuration**: Configure Gemini, OpenAI, and Local LLMs (Ollama) via UI.
- [x] **FR-1.2.7: Line Folding**: Virtual folding of text lines (logical display suppression).
- [x] **FR-1.2.8: Visual Indicators for Folding**: Icons/buttons in the gutter to toggle folding.

## 🔵 UI & Auxiliary
- [x] **FR-1.2.9: Zoom Logic**: Implement `IDWRITE_TEXT_FORMAT` scaling or D2D Transform for zoom.
- [x] **FR-1.7.4: Macro Gallery UI**: UI for browsing scripts.
- [x] **FR-1.9.2: Jump to Line Dialog**: Small input dialog for offset navigation.
- [x] **FR-1.5.3: Recent Files/Folders**: Track history in the File menu.
- [x] **FR-1.9.3: Test Suite**: Core feature testing (src/test_editor_core.cpp).
- [x] **FR-1.9.4: CLI Support**: Execute JS via command line (-e flag).
- [x] **FR-1.2.10: Scrolling**: 
  - [x] Internal offset logic in `Renderer` (Done).
  - [x] Win32 Vertical (`SB_VERT`) scrollbar integration.
  - [x] Handle `WM_VSCROLL` and `WM_MOUSEWHEEL` messages.
  - [x] Horizontal (`SB_HORZ`) scrollbar.
- [x] **FR-1.7.5: AI Assisted Coding**:
  - [x] Expose shell command API (`Editor.runCommand`).
  - [x] Implement AI completion script using Gemini API (`scripts/ai.js`).
  - [x] Add AI Assistant to Tools menu and map to `Alt+A`.
  - [x] Implement conversational AI Console (`Alt+I`).
  - [x] Secure API Key storage in JSON config.
  - [x] Structured multi-file edit protocol (`@@@REPLACE ... @@@`).
  - [x] Multi-server support (Gemini, OpenAI compatible).
  - [x] Per-buffer conversation context (Session history).
  - [x] Unlimited history storage with sliding window for API prompts.
  - [x] **Gemini Context Caching**: Explicit server-side caching for long histories (cost/token optimization).
  - [x] **Asynchronous Shell API**: `Editor.runAsync` for non-blocking background AI tasks.
  - [x] **@buffer workspace references** in console prompts.

---
*Last Updated: 2026-02-19*
