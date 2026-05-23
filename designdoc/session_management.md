# Ecode Terminal — Session Management Design

## Overview

A **session** is the full lifespan of a single ConPTY shell or CLI process,
from the moment `Terminal.exe` calls `ConPtySession::Start()` until the process
exits (or the plugin window is closed).

Currently Ecode lacks:

- Detection and graceful handling of shell-process death
- A way to restart a dead session without closing and reopening the tab
- Named session profiles for frequently-used shells or AI tools
- Scrollback export (save the session transcript to a file)
- Automatic API-key injection for AI CLI tools (`claude`, `opencode`, `aider`)

This document specifies how to add all of the above.

---

## Architecture

```
[Ecode Tab Bar]
  └── [Terminal tab N]  ──  Terminal.exe plugin process
                                │
                        ┌───────┴────────┐
                  TerminalView        ConPtySession
                  (D2D render)        (ConPTY + reader thread)
                        │                   │
                  TerminalEmulator    WriteFile / ReadFile
                  (VT parser)         (inputWriteSide_ / outputReadSide_)
                        │
                  TerminalBuffer
                  (cell grid + history)
```

The Ecode host launches `Terminal.exe` via `LaunchApp()` and embeds its HWND
as a child window.  Each terminal tab is an independent OS process — multiple
tabs run independently without shared state.

---

## Session Lifecycle

```
  ┌──────────┐   Start()      ┌─────────┐
  │ Created  │ ─────────────► │ Active  │
  └──────────┘                └────┬────┘
                                   │  shell process exits
                                   ▼
                           ┌──────────────┐
                           │ ShellExited  │
                           └──────┬───────┘
                    Enter / API  │          close tab
                    ┌────────────┘            │
                    ▼                         ▼
              ┌──────────┐             ┌──────────┐
              │ Active   │             │  Closed  │
              │(restarted│             └──────────┘
              └──────────┘
```

| State | Description |
|-------|-------------|
| **Created** | `Terminal.exe` process running; `ConPtySession::Start()` not yet called |
| **Active** | PTY process running; I/O flowing normally |
| **ShellExited** | Reader thread ended; process handle signaled; scrollback intact |
| **Closed** | Plugin window destroyed; all handles released |

### Transition: Active → ShellExited

`ConPtySession::ReaderLoop()` calls `ReadFile(outputReadSide_)`.  When the
shell process exits the PTY closes, `ReadFile` returns with 0 bytes and error
`ERROR_BROKEN_PIPE`.  At this point the reader thread should:

1. Record `exitCode_` via `GetExitCodeProcess(hProcess_)`.
2. Set `running_ = false`.
3. Post `WM_SESSION_ENDED` to `TerminalView`'s HWND with `wParam = exitCode`.

```cpp
// ConPtySession.cpp — end of ReaderLoop()
DWORD code = 0;
GetExitCodeProcess(hProcess_, &code);
exitCode_ = code;
running_  = false;
if (notifyHwnd_)
    PostMessage(notifyHwnd_, WM_SESSION_ENDED, (WPARAM)code, 0);
```

Add `WM_SESSION_ENDED` and `notifyHwnd_` to `ConPtySession`:

```cpp
// ConPtySession.h additions
static constexpr UINT WM_SESSION_ENDED =
    WM_APP + 0x10;            // wParam = exit code

void SetNotifyHwnd(HWND hwnd) { notifyHwnd_ = hwnd; }
DWORD ExitCode() const        { return exitCode_; }

private:
HWND  notifyHwnd_ = nullptr;
DWORD exitCode_   = 0;
```

### Transition: ShellExited → Active (restart)

`TerminalView` handles `WM_SESSION_ENDED`:

```cpp
case WM_SESSION_ENDED: {
    DWORD code = (DWORD)wp;
    wchar_t buf[64];
    swprintf(buf, 64, L"\r\n[Process exited with code %lu]\r\n"
                      L"Press Enter to restart.\r\n", code);
    session_.Write(std::wstring());  // no-op; session is dead
    emulator_.process(buf);          // render notice into buffer
    InvalidateRect(hwnd_, nullptr, FALSE);
    state_ = SessionState::ShellExited;
    return 0;
}
```

On the next `WM_CHAR` / `Enter` key while `state_ == ShellExited`:

```cpp
if (state_ == SessionState::ShellExited && ch == L'\r') {
    RestartSession();
    return 0;
}
```

```cpp
void TerminalView::RestartSession() {
    session_.Close();
    buffer_.clear();
    emulator_.reset(&buffer_);
    session_.SetNotifyHwnd(hwnd_);
    if (!session_.Start(shell_, buffer_.cols(), buffer_.rows(), extraEnv_))
        /* show error */;
    state_ = SessionState::Active;
    InvalidateRect(hwnd_, nullptr, FALSE);
}
```

---

## Session State Model

```cpp
// Application/Terminal/src/SessionState.h
#pragma once
#include <windows.h>
#include <chrono>
#include <string>
#include <vector>
#include <utility>

enum class SessionState { Created, Active, ShellExited, Closed };

struct SessionInfo {
    DWORD    id;                         // process ID of the shell
    std::wstring shell;                  // command line passed to ConPtySession
    std::wstring initialCwd;             // CWD at session start
    std::wstring lastCwd;                // updated via OSC 7
    FILETIME startTime;
    FILETIME exitTime;
    DWORD    exitCode = 0;
    SessionState state = SessionState::Created;
    std::vector<std::pair<std::wstring,std::wstring>> extraEnv;
};
```

`TerminalView` owns one `SessionInfo` and updates it on transitions.

### CWD Tracking via OSC 7

Many shells emit `\e]7;file:///C:/path\a` after each prompt.  `TerminalEmulator`
already routes OSC sequences via `setTitleCallback`; add a parallel
`setCwdCallback`:

```cpp
emulator_.setCwdCallback([this](const std::wstring& cwd){
    info_.lastCwd = cwd;
});
```

This allows the parent editor (or a future "Open in Explorer" action) to know
the current directory without shelling out.

---

## Session Identity & Naming

The tab label is currently derived from the shell binary name (e.g.,
`"powershell"`).  Additions:

- **Dead-session indicator**: append `" [exited]"` to the tab label when
  `state_ == ShellExited`.  Send `WM_SETTEXT` to the plugin HWND; the host tab
  bar reads it on the next `UpdateTabs()` call.
- **Rename**: double-clicking the tab label opens a small `Edit` control
  inline (same pattern as browser tab rename).  Implementation is in
  `TabDragHandler.inl` or a new `TabRenameHandler.inl`.

---

## Session Storage Directory

### Default Location

Session files are stored under the user's home folder in a `.ecode` directory:

```
%USERPROFILE%\.ecode\sessions\
```

For example, for user `bluen` this expands to:

```
C:\Users\bluen\.ecode\sessions\
```

`%USERPROFILE%` is the user's home folder (`C:\Users\<username>`), easy to
find and always writable.  The dotfolder `.ecode` keeps all Ecode data in one
place alongside other config files the user may already have there.

> **Note on `%APPDATA%`:** The Windows roaming profile folder
> (`C:\Users\<username>\AppData\Roaming`) is hidden by default in Explorer and
> is harder for users to locate; it is no longer the default for session files.

### Configurable Storage Path

The session save directory must be configurable in the Settings dialogue
(Config → Settings → Terminal tab).

**Setting key:** `terminal.session_dir`  
**Default value:** `%USERPROFILE%\.ecode\sessions\`  
**Stored in:** `settings.ini` under `[Terminal]` section

```ini
[Terminal]
SessionDir=%USERPROFILE%\.ecode\sessions\
AutoSaveOnExit=0
```

`%USERPROFILE%` expands to `C:\Users\<username>` (the user's home folder).
The `.ecode` directory there mirrors the Unix convention of dotfile config
directories and keeps all Ecode data in one predictable location.

The path is expanded at runtime via `ExpandEnvironmentStringsW()`.  Users may
set it to any local or network path, e.g. `D:\work\sessions\` or
`%USERPROFILE%\Documents\ecode-sessions\`.

### Settings Dialogue — Terminal Tab

Add a **Terminal** tab to the existing Settings dialogue (currently has General
and AI tabs).

```
┌─ Settings ─────────────────────────────────────────────┐
│  [General]  [AI]  [Terminal]                           │
│                                                        │
│  Session storage                                       │
│  ┌────────────────────────────────────┐ [Browse...]   │
│  │ C:\Users\bluen\.ecode\sessions\    │               │
│  └────────────────────────────────────┘               │
│                                                        │
│  [ ] Auto-save transcript on session exit              │
│                                                        │
│  Default shell profile:  [PowerShell        ▼]        │
│                                                        │
│                          [OK]  [Cancel]                │
└────────────────────────────────────────────────────────┘
```

`SettingsManager` additions:

```cpp
// include/SettingsManager.h
std::wstring GetSessionDir()    const;
bool         GetSessionAutoSave() const;
void         SetSessionDir(const std::wstring& dir);
void         SetSessionAutoSave(bool enabled);
```

`GetSessionDir()` calls `ExpandEnvironmentStringsW()` on the stored value
before returning it, so callers always receive a fully-expanded absolute path.

### Session File Organization

With many sessions, files are organized under the session directory as:

```
%USERPROFILE%\.ecode\sessions\
  <profile_name>\
    <YYYYMMDD_HHMMSS>_<pid>.txt
    <YYYYMMDD_HHMMSS>_<pid>.txt
    ...
  claude-agent\
    20260523_142301_4812.txt
    20260523_091544_7203.txt
  powershell\
    20260522_183012_9104.txt
  index.json
```

The `<profile_name>` subdirectory is sanitized (spaces → `_`, strip illegal
chars) before use as a directory name.

### Session Index

A lightweight index file tracks all saved sessions for quick browsing:

`<SessionDir>\index.json`

```json
[
  {
    "file":    "claude-agent/20260523_142301_4812.txt",
    "profile": "Claude Agent",
    "shell":   "powershell.exe",
    "start":   "2026-05-23T14:23:01",
    "end":     "2026-05-23T15:47:22",
    "exit_code": 0
  },
  ...
]
```

Entries are appended when a session is saved.  `index.json` is capped at 500
entries (oldest removed first).

---

## Session Export (Save Scrollback)

```cpp
// TerminalView.h addition
bool SaveScrollback(const std::wstring& path) const;
```

```cpp
bool TerminalView::SaveScrollback(const std::wstring& path) const {
    HANDLE hf = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return false;
    for (int row = 0; row < buffer_.totalLineCount(); ++row) {
        std::wstring line = buffer_.lineAt(row).plainText();
        line += L"\r\n";
        std::string utf8 = WideToUtf8(line);
        DWORD written;
        WriteFile(hf, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    }
    CloseHandle(hf);
    return true;
}
```

**Manual save:** right-click on the terminal surface → "Save Session
Transcript…" opens a `GetSaveFileName` dialog pre-filled with the path
`<SessionDir>\<profile>\<timestamp>_<pid>.txt`.

**Auto-save on exit:** when `terminal.auto_save_on_exit = true`, `TerminalView`
calls `SaveScrollback()` automatically when handling `WM_SESSION_ENDED`, then
appends the new entry to `index.json` — no user interaction needed.

---

## Session Profiles

### Storage

`%APPDATA%\Ecode\terminal_profiles.json`

> **Note:** profiles (shell configurations) are always stored in
> `%APPDATA%\Ecode\` regardless of the session storage directory setting.
> Only transcript files go to the configurable `SessionDir`.

```json
{
  "default_profile": "PowerShell",
  "profiles": [
    {
      "name":  "PowerShell",
      "shell": "powershell.exe",
      "args":  [],
      "cwd":   "",
      "env":   {},
      "inject_ai_key": false
    },
    {
      "name":  "CMD",
      "shell": "cmd.exe",
      "args":  ["/K", "chcp 65001 > nul"],
      "cwd":   "",
      "env":   {},
      "inject_ai_key": false
    },
    {
      "name":  "Claude Agent",
      "shell": "powershell.exe",
      "args":  ["-NoProfile", "-Command", "claude"],
      "cwd":   "${project_dir}",
      "env":   {},
      "inject_ai_key": true
    },
    {
      "name":  "WSL",
      "shell": "wsl.exe",
      "args":  [],
      "cwd":   "",
      "env":   {},
      "inject_ai_key": false
    }
  ]
}
```

**Variable expansion:**

| Token | Resolves to |
|-------|-------------|
| `${project_dir}` | `SettingsManager::Instance().GetProjectDirectory()` |
| `${appdata}` | `%APPDATA%` |
| `${user_home}` | `%USERPROFILE%` |

### `inject_ai_key`

When `true`, the profile loader reads API keys from `SettingsManager` and
appends them to `extraEnv` before calling `ConPtySession::Start()`:

```cpp
if (profile.injectAiKey) {
    auto& sm = SettingsManager::Instance();
    auto key = sm.GetAIApiKey(L"Anthropic");
    if (!key.empty()) extraEnv.push_back({L"ANTHROPIC_API_KEY", key});
    key = sm.GetAIApiKey(L"OpenAI");
    if (!key.empty()) extraEnv.push_back({L"OPENAI_API_KEY", key});
    key = sm.GetAIApiKey(L"Gemini");
    if (!key.empty()) extraEnv.push_back({L"GEMINI_API_KEY", key});
}
```

Keys never appear in the command line; they are injected only into the child
process environment via `ConPtySession::BuildEnvBlock()`, which already handles
the `extraEnv` parameter.

### Profile Loading

`SettingsManager` (or a thin `ProfileLoader` helper) reads
`terminal_profiles.json` on startup.  The host builds the `Tools → Terminal`
submenu dynamically from this list, replacing the current hard-coded
PowerShell / CMD / Bash items:

```
Tools
  └── Terminal
        ├── PowerShell          (IDM_TERMINAL_PROFILE + 0)
        ├── CMD                 (IDM_TERMINAL_PROFILE + 1)
        ├── Claude Agent        (IDM_TERMINAL_PROFILE + 2)
        ├── WSL                 (IDM_TERMINAL_PROFILE + 3)
        ├── ─────────────────
        └── Edit Profiles…      (opens terminal_profiles.json in editor)
```

Selecting a profile calls `LaunchApp(hwnd, termExe, shellArgs, name,
TAB_TYPE_TERMINAL)` with the fully-expanded command line.

---

## AI CLI Session Handling

AI CLIs (`claude`, `opencode`, `aider`, `gemini`) run inside a standard
terminal session; no special process type is needed.  The `inject_ai_key`
profile flag ensures API keys are present in the environment automatically.

**Recommended profiles:**

| Tool | Shell arg | `inject_ai_key` |
|------|-----------|----------------|
| `claude` | `powershell -NoProfile -Command claude` | `true` |
| `opencode` | `powershell -NoProfile -Command opencode` | `true` |
| `aider` | `powershell -NoProfile -Command aider` | `true` |
| `gemini` | `powershell -NoProfile -Command gemini` | `true` |

These tools use the full PTY (ANSI colors, interactive prompts) and work with
the existing `TerminalView` / `ConPtySession` stack without modification.

**Future (out of scope):** parse OSC markers emitted by AI CLIs to surface
turn count, token usage, or cost in the status bar.

---

## Implementation Plan

### New Files

| File | Purpose |
|------|---------|
| `Application/Terminal/src/SessionState.h` | `SessionInfo` struct + `SessionState` enum |

### Modified Files

| File | Change |
|------|--------|
| `Application/Terminal/include/ConPtySession.h` | Add `WM_SESSION_ENDED`, `SetNotifyHwnd()`, `ExitCode()`, `exitCode_`, `notifyHwnd_` |
| `Application/Terminal/src/ConPtySession.cpp` | Post `WM_SESSION_ENDED` at end of `ReaderLoop()` |
| `Application/Terminal/include/TerminalView.h` | Add `SaveScrollback()`, `RestartSession()`, `state_`, `info_`, `extraEnv_` |
| `Application/Terminal/src/TerminalView.cpp` | Handle `WM_SESSION_ENDED`; restart on Enter; context menu for save; auto-save if enabled |
| `Application/Terminal/src/main.cpp` | Parse `--cwd`, `--session-dir`, `--auto-save`, `--env KEY=VAL` args; pass to `StartSession` |
| `src/WindowHandlers_Command.inl` | Load `terminal_profiles.json`; build dynamic submenu; pass profile + session-dir args to `LaunchApp` |
| `include/SettingsManager.h` / `src/SettingsManager.cpp` | Add `GetSessionDir()`, `GetSessionAutoSave()`, `SetSessionDir()`, `SetSessionAutoSave()`, `LoadTerminalProfiles()`, `GetTerminalProfiles()` |
| Settings dialogue (`src/` — existing settings dialog code) | Add Terminal tab with session dir browse field, auto-save checkbox, default profile dropdown |

**No new CMake sources.** `SessionState.h` is header-only.

---

## Verification Checklist

1. **Normal exit** — type `exit` in PowerShell → tab appends  
   `[Process exited with code 0]` and `Press Enter to restart.`
2. **Abnormal exit** — kill process via Task Manager → tab appends  
   `[Process exited with code 1]`
3. **Restart** — press Enter after exit message → new shell prompt appears;
   scrollback from previous session preserved above
4. **Dead-tab label** — tab title changes to `"powershell [exited]"` on exit;
   reverts to `"powershell"` on restart
5. **Manual save** — right-click → "Save Session Transcript…" → save dialog
   appears pre-filled with `<SessionDir>\powershell\<timestamp>.txt`;
   saved file contains plain text, no ANSI codes
6. **Auto-save** — enable "Auto-save transcript on session exit" in Settings →
   Terminal; type `exit` → file appears in `<SessionDir>\powershell\` and
   `index.json` gains a new entry automatically
7. **Session storage dir** — change path in Settings → Terminal → Browse to
   `D:\work\sessions\`; next session exit saves there
8. **Default path** — fresh install with no `settings.ini` → `GetSessionDir()`
   returns `C:\Users\<username>\.ecode\sessions\` (fully expanded)
9. **Profile: Claude Agent** — `ANTHROPIC_API_KEY` env var set inside PTY  
   (`$env:ANTHROPIC_API_KEY` in PowerShell returns the key value)
10. **Profile: Claude Agent** — CWD resolves to project directory  
    (`Get-Location` matches configured project dir)
11. **Multiple profiles simultaneously** — open PowerShell tab and Claude Agent
    tab; both save to their respective subdirectories independently
12. **Edit Profiles…** — opens `terminal_profiles.json` in a new editor buffer
13. **`${project_dir}` expansion** — profile with `"cwd": "${project_dir}"`
    starts shell in the project directory even when project dir changes between
    sessions
14. **Index cap** — add 501 sessions; verify `index.json` contains exactly 500
    entries (oldest evicted)
