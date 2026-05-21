# Ecode

Talk to AI, edit code, run commands — all in one fast native Windows app.

No Electron. No webView. No 500MB install.

---

## Why Ecode for AI CLI

AI coding tools (Claude Code, OpenCode, etc.) work best when you can **see the code**, **edit it**, and **run commands** side by side. Ecode gives you all three in one window:

- **Terminal + Editor side by side**: Run `claude` or `opencode` in the terminal tab while editing files next to it
- **Copy AI output directly into code**: Select text from the AI's response and paste it right into your editor buffer
- **Scroll back through AI conversations**: Browsable history with Shift+PageUp/Down — no more losing output off screen
- **Snappy and lightweight**: Starts in under a second, uses <20MB RAM, no background processes

---

## Features at a Glance

### Terminal (Process-Bounded)
A real terminal runs as a child process inside an editor tab. Works with any CLI tool.

- Stream AI tool output in real time
- Click on file paths and URLs in AI output (OSC 8 hyperlinks)
- Browse scrollback history with mouse wheel or Shift+PageUp/Down
- Copy text from terminal output and paste into your code
- Colors, bold, italic, underline — everything renders correctly

### CLI Command Launcher
Save your most-used shell commands with names and working directories. Run them from a menu anytime.

| Example | Command | Directory |
|---------|---------|-----------|
| Build | `cmake --build .` | `D:\project\build` |
| Test | `pytest tests/` | `D:\project` |
| Git status | `git status` | `D:\project` |
| AI chat | `claude` | `D:\project` |

### File Manager (Dired)
Dual-pane file browsing with keyboard navigation. Copy, move, rename, delete files, open terminals in any directory.

### Fast File Dialog (FastFD)
A graphical file picker with multiple panes and keyboard navigation. Browse directories, select paths, send them back to the editor.

### Git-Friendly
- Atomic saves (write to temp, rename) — no corrupted files even on crash
- Handles huge files instantly (memory-mapped I/O)

### Scriptable (optional)
JavaScript engine built in. Create macros, automate editing tasks, add custom key bindings. Not required for day-to-day AI CLI use.

---

## Getting Started

### Download
Download the latest installer from [Releases](https://github.com/user/Ecode/releases).

### Build from Source
```powershell
git clone https://github.com/user/Ecode.git
cd Ecode
mkdir build; cd build
cmake ..
cmake --build . --config Release --target installer
```
Requires: Windows 10/11, Visual Studio 2022, Powershell.

---

## Project Status

- [x] Terminal + CLI launcher
- [x] File manager (Dired, FastFD)
- [x] Huge file support
- [x] Multi-language UI
- [ ] Theming
- [ ] Search & replace

---

## License

MIT
