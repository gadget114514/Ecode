# Implementation Plan - Ecode

This document outlines the roadmap and current implementation focus for Ecode.

## Phase-based Roadmap (Original)

### Phase 1: Setup and Dependencies
- [x] Set up the Project structure.
- [x] Create basic `main.cpp`.

[... other phases truncated ...]

## 🛠 Current Focus: Feature Enhancements (2026-02-13)

This section covers the immediate tasks identified in `TODO.md` to bring the editor to a more usable state.

### 1. Scripting & Key Bindings
- **Improve Emacs Bindings**: Update `ScriptEngine.cpp` to support more complex Emacs operations like variable `kill-line` and scrolling.
- **Key Chord Handling**: Ensure chords like `Ctrl+K` correctly interface with the `Buffer` logic.

### 2. Settings & Persistence
- **SettingsManager**: A new component to handle saving/loading window state, language, and editor options to `%APPDATA%/Ecode/settings.ini`.
- **Application Integration**: Hook into `WM_CREATE` and `WM_DESTROY` in `main.cpp` to persist state.

### 3. Gutter & Interaction
- **Gutter Hit-Testing**: Enable selecting lines by clicking the gutter.
- **Folding Toggles**: Implement visual indicators and click handlers in the gutter for the existing folding logic.

### 5. Emacs-like Incremental Search (Isearch)
- **Keyboard Capture**: Implement a mechanism to redirect keyboard input to a JavaScript handler for "modal" operations like Isearch.
- **Search Logic**: Implement `emacs_isearch_forward` and `emacs_isearch_backward` in JavaScript.

### 6. Test Suite Organization [NEW]
- **Directory Structure**: Create a `tests/` directory at the project root.
- **Test Programs**: Migrate and organize all test scripts and programs into the `tests/` folder.
- **Visual Feedback**: Use `setStatusText` to show "I-search: [query]" in the status bar.
- **Match Highlighting**: Proactively move the caret and update selection as the user types.

## Phase 10: Testing and Refinement (Updated)
- [ ] Test with huge files.
- [ ] Test Unicode support (UTF-8, UCS-2).
- [ ] Verify memory stability.
- [ ] **Manual verification of Isearch**: `Ctrl+S` -> type "main" -> caret should move to each letter typed. `Ctrl+S` again -> skip to next match. `Enter` -> finalize. `Esc` -> revert.
