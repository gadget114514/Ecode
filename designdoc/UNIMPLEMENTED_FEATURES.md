# 📝 Ecode - Unimplemented Features & Stubs Status

This document provides a clear, categorized view of features that are currently unimplemented, partially implemented (stubs), or buggy across the **Ecode** codebase and its utility applications.

Last Updated: June 2026 (Local Time: 2026-06-09)

---

## 1. Ecode Core & Editor UI

| Item / Feature ID | Category | Request / Requirement | Current Status | Technical Details / Notes |
| :--- | :--- | :--- | :--- | :--- |
| **LSP Support** | Extensibility | Complete Language Server Protocol client for completions, hover, and diagnostics. | 🔴 **Stub / Unimplemented** | `LspClient` lacks a proper JSON parser (uses raw `std::string::find` checks). It is exposed to JavaScript but is not integrated into any editor-side UI (no completion dropdowns, error gutters, or tooltips). |
| **Grep Non-Recursive Option** | Search / UI | Ability to toggle recursive directory search on/off. | 🔴 **Unimplemented** | `GrepSearchThread` in `src/Editor.cpp` unconditionally uses `fs::recursive_directory_iterator`. The "Grep" UI has no option to query or toggle this, forcing recursion. |
| **Help Menu Clean-up** | UI Layout | Hide all entries in the Help menu except "About". | 🔴 **Unimplemented** | Help menu in `src/UIHelpers.inl` still displays `Documentation`, `Keybindings`, `Copyright`, `Show Messages`, and `Clear Messages`. |
| **Copy Path Shortcut** | UI Shortcut | Add a menu option in the main `Edit` menu to copy the active file's full path. | 🔴 **Unimplemented** | Only available via right-clicking the editor buffer tabs (`IDM_TAB_COPY_PATH`). No entry exists in the main `Edit` menu. |
| **Menu Duplication (New File)** | UI Layout | Fix the duplication where "New File" appears inside a "New" popup menu. | 🟡 **Partially Addressed** | File menu creates a popup named `&New` (`menu_file_new`) containing `New File` (`menu_file_new_file`). This reads as `New -> New File` and looks duplicated. |
| **Config Menu Hierarchy** | UI Layout | The `Config` menu should be a top-level menu rather than nested inside `Edit`. | 🔴 **Bug / Layout issue** | In `src/UIHelpers.inl`, `hConfig` (containing Settings, Themes, etc.) is appended as a popup submenu of `hEdit` (`menu_config`), which goes against standard UX and `REQUIREMENTS.md`. |
| **Tab/Process Mark Update** | UI Layout | Active markers in the `Buffers` menu should update correctly when switching to/from terminal or external app tabs. | 🟡 **Unstable / Buggy** | The `UpdateMenu` function checks `g_activeAppTab` but menu updates are not always triggered reliably upon app/terminal focus switches. |
| **Save As Title Update** | UI / UX | Show full path in window title after saving using "Save As". | 🟢 **Implemented** | Handled inside `UpdateMenu` which is called at the end of `IDM_FILE_SAVE_AS`. |

---

## 2. Dired & FastDired (Filer Utilities)

`FastDired` (located in `Application/FastDired`) is a Direct2D-based multi-threaded filer designed to replace the common-control-based `Dired`. However, most core features are missing:

| Feature | Requirement | Current Status | Details / Technical Backlog |
| :--- | :--- | :--- | :--- |
| **Header Sorting** | Clicking a header column (Name, Size, Type, Date) sorts the folder contents. | 🔴 **Unimplemented** | In `FastDired/src/main.cpp`, `WM_LBUTTONDOWN` is ignored when clicking the headers. No sorting logic exists in the scan thread or memory structures. |
| **Folder Selection Buttons** | Quick buttons on each pane to open a folder selection dialog. | 🔴 **Unimplemented** | Not present in the UI layout. Only keyboard navigation or drag & drop is supported. |
| **Editable Path Fields** | The top path fields should be actual editable text fields. | 🔴 **Unimplemented** | The path is drawn as static text on the header bar using DirectWrite. Users cannot type a path to navigate. |
| **Windows Shell Context Menu** | Right-click context menu containing "Open with Windows shell", etc. | 🔴 **Unimplemented** | Right-click messages (`WM_RBUTTONDOWN` / `WM_CONTEXTMENU`) are completely unhandled. |
| **Text Field Drag & Drop** | Dropping a folder onto the path text field opens it. | 🔴 **Unimplemented** | Drag & drop is implemented at the window level, but not targeted specifically to the path field. |
| **Divider State Persistence** | Save/restore the center splitter position across sessions. | 🔴 **Unimplemented** | The splitter variable `g_dividerPos` is reset on launch; settings manager has no save/load hook for it. |

---

## 3. Terminal Emulator

`Application/Terminal` is a ConPty-backed terminal application embedded into Ecode:

| Feature / Issue | Category | Current Status | Details / Technical Backlog |
| :--- | :--- | :--- | :--- |
| **DECSCA Support** | Terminal Emulation | Character attribute settings. | 🔴 **Unimplemented** | Marked in `TerminalEmulator.cpp` (Selective Erase in Display/Line are treated as normal ED/EL because `DECSCA` is not implemented). |
| **Performance with Large Buffers**| Stability | Terminal becomes slow and unstable under high throughput. | 🟡 **Partially Addressed** | Needs optimization for rendering dirty rects and buffer pruning. Running terminal tabs causes occasional UI stutters. |

---

## 4. Tab Selector (Alt-Tab style Grid View)

Located in `src/TabSwitcherView.inl`:

- **Realtime Snapshots**: Implemented using DWM thumbnails (`DwmRegisterThumbnail`).
- **Issues / Unimplemented**: 
  - Thumbnail mapping fails under certain DPI scale changes or window state changes (reverts to fallback bitmaps).
  - Snapshot updates do not cleanly release resources, occasionally leaking GDI objects or causing slight redraw lag.

---

## How to Check or Run Stub Targets
- To compile all applications, use:
  - `build.sh` (or `build_test.bat` on Windows).
- To inspect configuration schemas, view [SettingsManager.cpp](file:///D:/ws/Ecode/src/SettingsManager.cpp).
