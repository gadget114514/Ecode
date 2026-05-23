# Session Management Feature

## Overview

Save/restore the complete editor state: all open buffers (file-backed, scratch, shell, output buffers) with their content, cursor positions, scroll positions, encoding, and folded lines. Provide auto-save on exit, manual save/load via menu, and a session list dialog.

## 1. Data Storage

**Location**: `%APPDATA%\Ecode\sessions\`
**Master index**: `sessions.ini` — lists all saved sessions
**Per session**: A subdirectory `session_N/` containing:
- `session.ini` — metadata + buffer state (paths, cursors, scroll, encoding, folded lines)
- `buffer_0.txt`, `buffer_1.txt`, ... — content for non-file-backed buffers (scratch, shell, output buffers like `*Messages*`, `*Find Results*`, AI sessions)

### session.ini format

```ini
[Session]
Name=My Project
Time=2026-05-23 01:30:00
ActiveBuffer=2
BufferCount=5

[Buffer_0]
Path=C:\code\main.cpp
CaretPos=1024
SelectionAnchor=1024
ScrollLine=50
ScrollX=0.0
DesiredColumn=10
Encoding=0
IsDirty=0
IsScratch=0
IsShell=0
FoldedLines=

[Buffer_3]
Path=*Messages*
CaretPos=0
SelectionAnchor=0
ScrollLine=0
ScrollX=0.0
DesiredColumn=0
Encoding=0
IsDirty=0
IsScratch=0
IsShell=0
FoldedLines=
ContentFile=buffer_3.txt
```

### sessions.ini format

```ini
[Index]
Count=3
Session_0_Name=My Project
Session_0_Time=2026-05-23 01:30:00
Session_1_Name=Debug Session
Session_1_Time=2026-05-23 02:15:00
Session_2_Name=*AutoSave*
Session_2_Time=2026-05-23 03:00:00
```

## 2. Files to Modify

| File | Changes |
|------|---------|
| `include/SettingsManager.h` | Add `SessionInfo` struct, `SessionBufferState` struct, and methods: `GetSessionsDir()`, `SaveSession(name)`, `LoadSession(index)`, `DeleteSession(index)`, `GetSessionList()`, `GetAutoSaveSessionIndex()`, `SetAutoSaveSessionIndex()`, `ClearAutoSaveSession()` |
| `src/SettingsManager.cpp` | Implement all session methods using `WritePrivateProfileStringW`/`GetPrivateProfileStringW` for INI files and file I/O for content files |
| `include/Editor.h` | Add `GetBufferState(index)` / `RestoreBufferState(index, state)` helper methods |
| `src/Editor.cpp` | Implement the above |
| `src/Globals.inl` | Add `IDM_FILE_SAVE_SESSION` (108), `IDM_FILE_OPEN_SESSION` (109), `IDM_FILE_MANAGE_SESSIONS` (110). Add `ShowSessionManagerDialog()` forward declaration. |
| `src/UIHelpers.inl` | Add session menu items to the File menu |
| `src/WindowHandlers_Command.inl` | Add command handlers for session commands |
| `src/WindowHandlers_Core.inl` | In `HandleDestroy()`: auto-save session before exit. In `HandleCreate()`: check for auto-saved session and prompt to restore. |
| `include/Dialogs.h` | Add `ShowSessionManagerDialog(HWND hwnd)` and `ShowSaveSessionDialog(HWND hwnd)` |
| `src/Dialogs.cpp` | Implement the two session dialogs |
| `include/resource.h` | Add new resource IDs for dialog controls |
| `src/Ecode.rc` | Add dialog templates |

## 3. Data Structures

```cpp
struct SessionBufferState {
    std::wstring path;
    size_t caretPos;
    size_t selectionAnchor;
    size_t scrollLine;
    float scrollX;
    size_t desiredColumn;
    int encoding;           // 0=UTF8, 1=UTF16LE, 2=UTF16BE, 3=ANSI
    bool isDirty;
    bool isScratch;
    bool isShell;
    std::vector<size_t> foldedLines;
    std::wstring contentFile; // relative path in session dir (for non-file buffers)
};

struct SessionInfo {
    int index;              // N from session_N
    std::wstring name;
    std::wstring time;
    int bufferCount;
};
```

## 4. Implementation Steps

### Step 1: Data structures and SettingsManager methods

Add `SessionInfo` and `SessionBufferState` structs to `SettingsManager.h`. Add methods:
- `GetSessionsDirectory()` — returns `%APPDATA%\Ecode\sessions\`, creates if not exists
- `GetSessionIndexPath()` — returns `sessions.ini` path
- `GetSessionDir(int index)` — returns `session_N` subdirectory path
- `SaveSession(const std::wstring& name)` — iterates all buffers, extracts state, writes session.ini + content files, updates sessions.ini
- `LoadSession(int index)` — reads session.ini, prompts for unsaved buffers, closes all, opens files, restores state
- `DeleteSession(int index)` — removes session directory and updates master index
- `GetSessionList()` — returns `std::vector<SessionInfo>`
- `GetAutoSaveSessionIndex()` / `SetAutoSaveSessionIndex(int)` / `ClearAutoSaveSession()`

### Step 2: Editor helper methods

- `GetBufferState(size_t index)` — returns a `SessionBufferState` for the buffer at given index
- `RestoreBufferState(size_t index, const SessionBufferState& state)` — restores cursor, scroll, etc. to the buffer at given index

### Step 3: Command IDs and menu items

Add to `Globals.inl`:
```cpp
#define IDM_FILE_SAVE_SESSION 108
#define IDM_FILE_OPEN_SESSION 109
#define IDM_FILE_MANAGE_SESSIONS 110
```

In `UIHelpers.inl`, add to the File menu (before Exit):
```cpp
AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
AppendMenu(hFile, MF_STRING, IDM_FILE_SAVE_SESSION, L"Save Session...");
AppendMenu(hFile, MF_STRING, IDM_FILE_OPEN_SESSION, L"Open Session...");
AppendMenu(hFile, MF_STRING, IDM_FILE_MANAGE_SESSIONS, L"Manage Sessions...");
```

### Step 4: Command handlers

In `WindowHandlers_Command.inl`:
- `case IDM_FILE_SAVE_SESSION`: call `Dialogs::ShowSaveSessionDialog(hwnd)`
- `case IDM_FILE_OPEN_SESSION`: call `Dialogs::ShowSessionManagerDialog(hwnd)` (user picks session)
- `case IDM_FILE_MANAGE_SESSIONS`: call `Dialogs::ShowSessionManagerDialog(hwnd)` for management

### Step 5: Session dialogs

**Save Session dialog** (`ShowSaveSessionDialog`):
- Dialog resource with edit field for name, OK/Cancel
- Default name = `L"Session "` + current datetime string
- On OK: `SettingsManager::Instance().SaveSession(name)`

**Session Manager dialog** (`ShowSessionManagerDialog`):
- Dialog resource with listbox, Load, Delete, Close buttons
- Populate listbox from `GetSessionList()` — format: `"Name  [Date]  (N buffers)"`
- Load: `SettingsManager::Instance().LoadSession(selIndex)`, close dialog
- Delete: `SettingsManager::Instance().DeleteSession(selIndex)`, refresh list
- Follows existing dialog pattern (see `ThemeManagerDlgProc`, `CliSettingsDlgProc`)

### Step 6: Auto-save on exit

In `HandleDestroy()` in `WindowHandlers_Core.inl`:
- Before existing cleanup, check if there are multiple buffers or if the single buffer has content
- Save session with name `L"*AutoSave*"` under a fixed auto-save index
- Call `SettingsManager::Instance().SaveSession(L"*AutoSave*")`

### Step 7: Auto-restore on launch

In `HandleCreate()` in `WindowHandlers_Core.inl`:
- After loading settings and creating the initial file, check `GetAutoSaveSessionIndex()`
- If an auto-save session exists, show `MessageBoxW` asking "An auto-saved session was found. Restore?"
- On Yes: close the initial Untitled buffer, call `LoadSession(autoSaveIndex)`
- On No: delete the auto-save session via `ClearAutoSaveSession()`

### Step 8: Resource IDs and dialog templates

Add to `resource.h`:
```cpp
#define IDD_SAVE_SESSION 2500
#define IDD_SESSION_MANAGER 2510
#define IDC_SESSION_NAME 2501
#define IDC_SESSION_LIST 2511
#define IDC_SESSION_LOAD 2512
#define IDC_SESSION_DELETE 2513
```

Add dialog templates to `Ecode.rc` for `IDD_SAVE_SESSION` and `IDD_SESSION_MANAGER`.

## 5. Edge Cases

- **Buffer content extraction**: Use `buf->GetText(0, buf->GetTotalLength())` for non-file-backed buffers. Large shell/output buffers could be slow — consider a progress indicator if needed.
- **File-backed with unsaved changes**: Prompt user to save before session save, or auto-save. Recommendation: auto-save dirty file-backed buffers to disk as part of Save Session.
- **App tabs (Terminal, Dired, etc.)**: These are separate processes and cannot be serialized. Session saves only regular buffers. Restore reopens file buffers only.
- **Auto-save on exit**: Skip auto-save if only one empty Untitled buffer exists (nothing meaningful to restore).
- **Single instance**: Sessions are tied to one editor instance. No multi-instance session merging.
- **Large sessions**: If many buffers, session file writing happens in `HandleDestroy` which should be fast. Content file writing for large shell buffers could be deferred with a progress bar.

## 6. Future Extensions (v2+)

- Command-palette integration (`:save-session`, `:load-session`) via JS API
- Session export/import to share sessions
- Per-session project directory auto-restore
- Restore app tabs (Terminal/Dired) by re-launching them with original working directories
- Session auto-save on timer (crash recovery)
