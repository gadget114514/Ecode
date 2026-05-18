# Ecode Terminal Tab — Design & Implementation Guide

## Overview

This document describes the architecture, implementation plan, and API reference for the embedded terminal tab added to Ecode. The terminal is capable of running fully interactive CLI tools such as `claude`, `codex`, and `opencode` that require a real PTY, ANSI color support, and interactive prompt handling.

---

## Architecture

```
[Tab Bar (WC_TABCONTROL)]
   ├── [Buffer tabs 0..N-1]  → EditorBufferRenderer (existing D2D pipeline)
   └── [Terminal tab N]      → TerminalView (child HWND)
                                    │
                         ┌──────────┴──────────┐
                   TerminalEmulator        ConPtySession
                   (ANSI/VT parser)        (ConPTY + reader thread)
                         │                      │
                   TerminalBuffer          WriteFile / ReadFile
                   (cell grid + history)   (inputWrite / outputRead pipes)
```

### Data Flow

```
User keystroke
  └─► TerminalView::WndProc (WM_KEYDOWN / WM_CHAR)
        └─► key → VT sequence translation
              └─► ConPtySession::Write()
                    └─► WriteFile(inputWrite_)
                          └─► [Shell / AI CLI process receives input]

[Shell output]
  └─► ReadFile(outputRead_)  [reader std::thread]
        └─► PostMessage(hwnd, WM_TERMINAL_OUTPUT, ...)
              └─► TerminalEmulator::process(utf8 → wstring)
                    └─► TerminalBuffer mutations
                          └─► InvalidateRect → WM_PAINT
                                └─► TerminalView D2D render
```

---

## Windows API Requirements

| Feature | Minimum Windows Version |
|---------|------------------------|
| ConPTY (`CreatePseudoConsole`) | Windows 10 1809 (Build 17763) |
| Direct2D rendering | Windows 7 SP1 |
| `EXTENDED_STARTUPINFO_PRESENT` | Windows Vista |

ConPTY functions are loaded **dynamically** at runtime via `GetProcAddress` on `kernel32.dll`, so the binary can still launch on older systems and show a graceful error instead of crashing.

---

## Reference

- `references/termdock-main/` — Qt-based terminal whose logic was ported to pure C++
- [ConPTY documentation](https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session) — Microsoft docs
- [XTerm control sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html) — Full VT reference
- [VT100 sequences](https://vt100.net/docs/vt100-ug/chapter3.html) — Classic terminal reference

---

## New Source Files

| File | Description |
|------|-------------|
| `include/TerminalCell.h` | POD struct for one terminal cell: character, foreground/background `TermColor`, bold/dim/underline/inverse/wide flags |
| `include/TerminalBuffer.h` | Terminal grid: cursor position, scroll region, history lines, alternate screen. Pure C++17, no external deps |
| `src/TerminalBuffer.cpp` | Full VT buffer implementation ported from termdock-main (QVector → std::vector, QChar → wchar_t) |
| `include/TerminalEmulator.h` | ANSI/VT100/VT220/xterm sequence parser. Callbacks replace Qt signals |
| `src/TerminalEmulator.cpp` | Parser implementation ported from termdock-main (QString → std::wstring) |
| `include/ConPtySession.h` | ConPTY lifecycle wrapper: create, write, resize, close, env block builder |
| `src/ConPtySession.cpp` | Pure Win32 implementation using std::thread and std::function. No Qt |
| `include/TerminalView.h` | Win32 child window class. Owns buffer, emulator, session. D2D render target |
| `src/TerminalView.cpp` | WM_PAINT (D2D), WM_KEYDOWN/WM_CHAR (VT input), WM_SIZE (resize PTY), WM_MOUSEWHEEL (scroll) |

---

## Existing Files Modified

| File | Change |
|------|--------|
| `src/Globals.inl` | Added `g_terminalView`, `g_terminalViewHwnd`, `g_terminalTabIndex`; new command `IDM_TOOLS_TERMINAL 512` |
| `src/main.cpp` | Global declarations for terminal objects; includes for new headers |
| `src/WindowHandlers_Core.inl` `HandleCreate()` | Inserts "Terminal" tab into `g_tabHwnd`; creates `TerminalView` child HWND (hidden) |
| `src/WindowHandlers_Core.inl` `HandleSize()` | Positions and shows/hides `g_terminalViewHwnd` alongside editor content area |
| `src/WindowHandlers_Command.inl` | Handles `WM_NOTIFY / TCN_SELCHANGE` to switch focus between editor and terminal view |
| `src/WindowHandlers_Input.inl` | Routes `WM_KEYDOWN` / `WM_CHAR` to terminal when terminal tab is active |
| `CMakeLists.txt` | Appends `ConPtySession.cpp`, `TerminalBuffer.cpp`, `TerminalEmulator.cpp`, `TerminalView.cpp` to SOURCES |

---

## Component Details

### TerminalCell (`include/TerminalCell.h`)

```cpp
struct TermColor {
    uint8_t r, g, b;
    bool isDefault;
    static TermColor fromRgb(uint8_t r, uint8_t g, uint8_t b);
};

struct TerminalCell {
    wchar_t       ch;            // BMP codepoint (primary)
    std::wstring  text;          // full grapheme cluster (surrogates / combining)
    TermColor     foreground;    // default: rgb(204,204,204)
    TermColor     background;    // default: rgb(12,12,12)
    bool bold, dim, underline, inverse, wide, wideContinuation;
};
```

### TerminalBuffer (`include/TerminalBuffer.h`)

Manages the visible screen grid and scrollback history. All VT cursor/scroll operations are implemented here.

Key methods:

| Method | Description |
|--------|-------------|
| `resize(cols, rows)` | Reflows grid; trims/pads lines |
| `putChar(ch, attrs)` | Writes one character at cursor, advances cursor |
| `putText(text, attrs)` | Writes a grapheme cluster (handles wide chars) |
| `carriageReturn()` / `lineFeed()` | CR / LF |
| `reverseIndex()` | ESC M — scroll down one line |
| `moveCursorTo(row, col)` | CUP — absolute cursor position (respects origin mode) |
| `setScrollRegion(top, bottom)` | DECSTBM |
| `useAlternateScreen(enabled)` | Switches between normal and alternate screen buffers |
| `lineAt(logicalRow)` | Returns line from history+screen by logical index |
| `totalLineCount()` | `historyLineCount() + rows()` — used for scrollbar |

### TerminalEmulator (`include/TerminalEmulator.h`)

Parses raw UTF-8 output from the PTY and mutates a `TerminalBuffer`. Uses `std::function` callbacks instead of Qt signals.

```cpp
class TerminalEmulator {
public:
    void reset(TerminalBuffer* buffer);
    void process(const std::wstring& text);   // feed decoded PTY output
    void setResponseCallback(std::function<void(const std::wstring&)> cb);
    void setTitleCallback(std::function<void(const std::wstring&)> cb);
};
```

**Supported escape sequences:**

| Category | Sequences |
|----------|-----------|
| Cursor movement | CUP (H/f), CUU/D/F/B/C/D, CHA (G), VPA (d), CNL/CPL |
| Erase | ED (J), EL (K), ECH (X) |
| Scroll | SU (S), SD (T), DECSTBM (r) |
| Insert/Delete | ICH (@), DCH (P), IL (L), DL (M) |
| SGR colors | 16-color (30–37/90–97), 256-color (38;5;n), truecolor (38;2;r;g;b) |
| SGR attributes | Bold, dim, underline, inverse, reset |
| Private modes | `?6` origin, `?7` autowrap, `?25` cursor vis, `?1000/1002/1003` mouse, `?1047/1049` alt screen, `?2004` bracketed paste |
| OSC | `0;` / `2;` window/tab title |
| Character sets | Line-drawing charset (ESC ( 0) — box-drawing Unicode substitution |
| Device reports | DA (c), DSR (n), CPR (6n), window reports (t) |
| Cursor style | DECSCUSR (SP q) |
| Kitty keyboard | `>u` push/pop flags, `?u` query |

### ConPtySession (`include/ConPtySession.h`)

Pure Win32 ConPTY wrapper. No Qt, no MFC.

```cpp
class ConPtySession {
public:
    void SetOutputCallback(std::function<void(const char*, size_t)> cb);
    bool Start(const std::wstring& shell, int cols, int rows,
               const std::vector<std::pair<std::wstring,std::wstring>>& extraEnv = {});
    void Write(const void* data, size_t bytes);
    void Resize(int cols, int rows);
    void Close();
    bool IsRunning() const;
private:
    HPCON  m_hPC        = nullptr;
    HANDLE m_inputWrite = nullptr;
    HANDLE m_outputRead = nullptr;
    HANDLE m_hProcess   = nullptr;
    std::thread        m_readerThread;
    std::atomic<bool>  m_running{false};
    std::function<void(const char*, size_t)> m_onOutput;
    // dynamic function pointers
    using FnCreate = HRESULT(WINAPI*)(COORD,HANDLE,HANDLE,DWORD,HPCON*);
    using FnResize = HRESULT(WINAPI*)(HPCON,COORD);
    using FnClose  = void   (WINAPI*)(HPCON);
    FnCreate m_fnCreate = nullptr;
    FnResize m_fnResize = nullptr;
    FnClose  m_fnClose  = nullptr;
    void ReaderLoop();
    std::wstring BuildEnvBlock(const std::vector<std::pair<std::wstring,std::wstring>>& extra);
};
```

**Startup sequence:**
1. `GetProcAddress(kernel32, "CreatePseudoConsole")` — fail gracefully on old Windows
2. `CreatePipe(&inputRead, &inputWrite, nullptr, 0)` × 2 (in + out)
3. `CreatePseudoConsole(size, inputRead, outputWrite, 0, &hPC)`
4. `InitializeProcThreadAttributeList` + `UpdateProcThreadAttribute(PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE)`
5. `CreateProcessW` with `EXTENDED_STARTUPINFO_PRESENT` and custom env block
6. Close `inputRead` and `outputWrite` handles (PTY owns them now)
7. Launch `std::thread` reader loop → `ReadFile(outputRead)` → callback

### Environment Variable Passing

`BuildEnvBlock()` procedure:

1. `GetEnvironmentStringsW()` — snapshot current process environment
2. Copy all existing `KEY=VALUE\0` pairs into a `std::wstring`
3. Append mandatory terminal identity vars:
   - `TERM=xterm-256color`
   - `COLORTERM=truecolor`
   - `FORCE_COLOR=3`
   - `CLICOLOR_FORCE=1`
4. Append caller-supplied pairs (e.g. API keys from `SettingsManager`)
5. Append final `\0` terminator (double-null end of block)
6. Pass `lpEnvironment` pointer to `CreateProcessW`

API keys are read from `SettingsManager::Instance()` so they persist across sessions and can be edited via `IDM_TOOLS_AI_SET_KEY`.

### TerminalView (`include/TerminalView.h`)

Win32 child window with its own `ID2D1HwndRenderTarget`. Registered as window class `"EcodeTerminalView"`.

**Rendering (WM_PAINT):**
```
BeginDraw()
  FillRectangle(clientRect, bgBrush)          // clear background
  for each visible logical row:
    for each cell in row:
      if bg != default: FillRectangle(cellRect, cellBgBrush)
      DrawText(cell.text, textFormat, cellRect, fgBrush)
  draw cursor rectangle at (cursorRow, cursorCol)
EndDraw()
```

Uses `IDWriteTextFormat` with "Cascadia Mono" (falls back to "Consolas"). Cell metrics computed once in `UpdateMetrics()` and cached as `cellWidth_` / `cellHeight_`.

**Keyboard input (WM_KEYDOWN / WM_CHAR):**

| Key | VT Sequence Sent |
|-----|-----------------|
| Arrow Up/Down/Left/Right | `\x1b[A` / `B` / `C` / `D` |
| Home / End | `\x1b[H` / `\x1b[F` |
| Page Up / Page Down | `\x1b[5~` / `\x1b[6~` |
| Insert / Delete | `\x1b[2~` / `\x1b[3~` |
| F1–F4 | `\x1bOP` / `OQ` / `OR` / `OS` |
| F5–F12 | `\x1b[15~` … `\x1b[24~` |
| Backspace | `\x7f` |
| Enter | `\r` |
| Tab | `\x09` |
| Ctrl+C | `\x03` |
| Ctrl+D | `\x04` |
| Ctrl+Z | `\x1a` |
| Ctrl+L | `\x0c` |
| Printable WM_CHAR | UTF-8 encoded bytes |

**Resize (WM_SIZE):**
```cpp
cols = clientWidth  / cellWidth_;
rows = clientHeight / cellHeight_;
buffer_.resize(cols, rows);
session_.Resize(cols, rows);
InvalidateRect(hwnd, nullptr, FALSE);
```

**Output thread safety:**

The ConPTY reader runs on a background `std::thread`. It posts chunks via:
```cpp
PostMessage(hwnd_, WM_TERMINAL_OUTPUT, (WPARAM)chunk_ptr, (LPARAM)chunk_len);
```
The main thread receives this in `WndProc`, calls `TerminalEmulator::process()`, then `InvalidateRect`. No locking required — all buffer mutations happen on the UI thread.

---

## Tab Integration

### Tab Index

The "Terminal" tab is appended after all buffer tabs. Its index is stored in `g_terminalTabIndex` (set in `HandleCreate`). When the editor opens new buffers, terminal tab index stays fixed because it is always the last item.

### Tab Switch Logic (`WM_NOTIFY / TCN_SELCHANGE`)

```cpp
int sel = TabCtrl_GetCurSel(g_tabHwnd);
if (sel == g_terminalTabIndex) {
    // Show terminal, hide editor scrollbars
    ShowWindow(g_terminalViewHwnd, SW_SHOW);
    ShowScrollBar(hwnd, SB_BOTH, FALSE);
    SetFocus(g_terminalViewHwnd);
    // Lazy-start PTY on first use
    if (g_terminalView && !g_terminalView->IsStarted())
        g_terminalView->StartSession(L"powershell.exe", {});
} else {
    ShowWindow(g_terminalViewHwnd, SW_HIDE);
    ShowScrollBar(hwnd, SB_BOTH, TRUE);
    // Switch editor to correct buffer
    g_editor->SwitchToBuffer(sel);
    InvalidateRect(hwnd, nullptr, FALSE);
}
```

---

## Build System

New sources added to `CMakeLists.txt`:

```cmake
list(APPEND SOURCES
    src/ConPtySession.cpp
    src/TerminalBuffer.cpp
    src/TerminalEmulator.cpp
    src/TerminalView.cpp
)
```

No new link libraries needed: `d2d1` and `dwrite` are already linked. ConPTY is loaded dynamically from `kernel32.dll`.

---

## Supporting Additional Shells

To launch a different shell, call `TerminalView::StartSession` with a different command line:

| Shell | Command |
|-------|---------|
| PowerShell 7 | `L"pwsh.exe"` |
| CMD | `L"cmd.exe /K chcp 65001 > nul"` |
| Git Bash | `L"C:\\Program Files\\Git\\bin\\bash.exe --login -i"` |
| WSL (default distro) | `L"wsl.exe"` |

The shell command line can be made configurable via `SettingsManager` (e.g., key `"terminal.shell"`).

---

## Verification Checklist

1. **Build**: `cmake --build build --config Debug` — zero errors
2. **Terminal tab visible**: Ecode starts → "Terminal" tab appears in tab bar
3. **Shell launches**: Click terminal tab → PowerShell prompt renders
4. **ANSI colors**: Run `Write-Host "Hello" -ForegroundColor Green` → green text
5. **Interactive AI CLI**: Type `claude` → Claude Code TUI renders correctly with colors and animated prompts
6. **Resize**: Drag window edge → terminal reflows; shell `$COLUMNS`/`$ROWS` update
7. **Ctrl+C**: Kills running foreground process without closing shell
8. **Scrollback**: Mouse wheel scrolls through history
9. **Tab switching**: Switch to editor tab and back — terminal state preserved
10. **API key**: Set `ANTHROPIC_API_KEY` in settings → `echo $env:ANTHROPIC_API_KEY` shows it inside the PTY
