# Ecode
This application is terminal(shell gui) and text editor.


There are same name editor gainning a lot of stars. but this editor and terminal just one star from me. Hahaha.

The development of this application is very early stage now. 

# Purpose

I want to use CLI and small editor in a pane. I've not edited at all. I always go and back between several windows in which CLI runs.
Maybe such application exists, but I started the development. it was hard even using AI CLIs.

Talk to AI, edit code, run commands — all in one fast native Windows app.

Minimal, No Electron. No webView. No 500MB install.

> **Process-bounded architecture**: Plugins like the terminal run as **separate child processes**, not as in-process DLLs or web views. Each dies cleanly when closed. No process hikes or memory leaks.

| | Ecode | VS Code | Neovim + terminal | Notepad++ |
|---|---|---|---|---|
| **Architecture** | Native Win32 | Electron | TUI editor | Native Win32 |
| **Terminal integrated** | Yes (process-bounded) | Yes (Electron) | Uses host terminal | No |
| **Editor + Terminal in one window** | Yes | Yes | No (two windows) | No |
| **Click file paths in terminal** | Yes (OSC 8) | No | No | N/A |
| **CLI command launcher** | Built-in | Tasks/scripts | Shell aliases | No |

---

## Why Ecode for AI CLI

A terminal alone is not enough. AI coding tools (Claude Code, OpenCode, etc.) produce output with **file paths**, **code snippets**, and **long conversations**. You need to act on that output — not just read it.

| With just a terminal | With Ecode |
|---|---|
| AI says "edit line 42 in main.c" → you open a separate editor, find the file, scroll to the line | Click the file path in terminal output → it opens in the editor at the right line |
| AI outputs a code block → you select, copy, switch windows, paste, save | Select in terminal, paste into editor tab beside it |
| Long AI conversation scrolls off screen → lost forever | Shift+PageUp to browse history |
| Type the same build/test commands over and over | Save them as CLI entries, run from a menu |
| Heavy Electron editor takes 5+ seconds to start and 500MB RAM | Starts in <1 second, uses <20MB RAM |

### What you get

- **Terminal + Editor in one window**: Run `claude` or `opencode` while editing files right next to it — no alt-tabbing
- **Click file paths in AI output**: OSC 8 hyperlinks render as clickable paths — one click opens the file
- **Copy-paste between AI and code**: AI proposes a change → select in terminal → paste into editor → save
- **Scrollable history**: Shift+PageUp/Down through AI conversations — nothing gets lost
- **CLI launcher**: Save frequently used commands (build, test, deploy) with names and directories
- **Snappy and lightweight**: Native Win32 app, <1s startup, <20MB RAM, no background processes

---

## LocalMsg — Cross-Agent Messaging

LocalMsg is a built-in plugin that lets AI agents (Claude Code, Codex, Gemini CLI, OpenCode) send messages and files to each other on the same machine or across the LAN. It acts as a primitive message bus for agent-to-agent communication.

**Key capabilities**:
- **REST API** on `127.0.0.1:2426` — agents interact via JSON HTTP
- **IPMsg protocol** (UDP 2425) — interoperates with legacy LAN messenger clients
- **LocalSend protocol** (HTTPS 53317) — file transfers and large messages (>1KB)
- **Pseudo-user routing** — each agent registers a name; messages addressed by name
- **File inbox** — files auto-accepted for known agents, stored in `%USERPROFILE%/Downloads/`
- **CLI tool** — `localmsg-cli.exe` for scripting by AI agents

```
claude ──▶ localmsg-cli ──▶ LocalMsg ──▶ localmsg-cli ──▶ codex
                              │
                              ├── IPMsg (LAN broadcast)
                              └── LocalSend (HTTPS file xfer)
```

👉 [Full specification →](Application/LocalMsg/SPEC.md)
👉 [Agent skill reference →](Application/LocalMsg/SKILL.md)

## Features at a Glance

### Terminal (Process-Bounded)
A real terminal runs as a child process inside an editor tab. Works with any CLI tool.

- Stream AI tool output in real time
- Click on file paths and URLs in AI output (OSC 8 hyperlinks)
- Browse scrollback history with mouse wheel or Shift+PageUp/Down
- Copy text from terminal output and paste into your code
- Colors, bold, italic, underline — everything renders correctly


This figure shows the opencode running in terminal.
![](https://github.com/user-attachments/assets/5a0ab0e6-0b00-4ca2-8784-2256205d9191)

### CLI Command Launcher
Save your most-used shell commands with names and working directories. Run them from a menu anytime.

| Example | Command | Directory |
|---------|---------|-----------|
| Build | `cmake --build .` | `D:\project\build` |
| Test | `pytest tests/` | `D:\project` |
| Git status | `git status` | `D:\project` |
| AI chat | `claude` | `D:\project` |

This figure shows the psmux(third party) running in terminal.
![](https://github.com/user-attachments/assets/ff2abfe1-512d-4255-9f26-6afff6cdda63)

### Git-Friendly
- Atomic saves (write to temp, rename) — no corrupted files even on crash
- Handles huge files instantly (memory-mapped I/O)

### Find in Files (Grep)
Search across your project with pattern matching, extension filtering, and regex support. Results appear in a dedicated buffer with clickable file paths.

### Scriptable (optional)
JavaScript engine built in. Create macros, automate editing tasks, add custom key bindings. Not required for day-to-day AI CLI use.

---

## Getting Started

### Download
Download the latest installer from [Releases](https://github.com/gadget114514/Ecode/releases).

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
- [x] Simple Text editor
- [x] Find in files (grep with regex)
- [x] Huge file support
- [ ] Multi-language UI
- [x] CLI (agy,opencode,claude CLI)
- [x] Launcher Applications/Plugins
- [x] Applications(admin)
- [x] Inline IME input 
- [x] osc1337
- [x] sixel
- [x] Cross CLI communication
- [ ] Theming
- [ ] Session save/resume
- [ ] Notepad keybindings
- [ ] JavaScript macros / bytecode compiler and loader
- [ ] MCP server
- [ ] Document/About/Help
- [ ] Autosave editor
- [ ] UI/small icons
- [ ] Language Server or AI assistant
      

---

## Security

See [SECURITY.md](SECURITY.md) for security considerations, especially regarding the LocalMsg LAN messenger plugin.

## License

MIT

## Thirdparty

Ecode incorporates the following third-party components (see [src/copyright.c](file:///D:/ws/Ecode/src/copyright.c) for full copyright notices):

- **Duktape**: JavaScript Engine (MIT License)
- **libsixel**: SIXEL Image Encoding/Decoding (MIT License)
- **Microsoft ConPTY / OpenConsole**: Pseudo Console host (MIT License)
- **MIT VT420 Parser**: Terminal parser reference implementation (MIT License)
- **JSON for Modern C++ (nlohmann/json)**: JSON serialization/deserialization (MIT License)
- **yaml-cpp**: YAML Parser (MIT License)
- **md4c**: Markdown Parser (MIT License)
- **dukluv**: Duktape + libuv scripting runtime (MIT License)
- **Mbed TLS**: Cryptographic and TLS implementation for LocalMsg plugin (Apache 2.0 or GPL 2.0+)
- **Microsoft WebView2 SDK**: Embedded web browser engine for Prompts plugin (BSD 3-Clause License)
- **Cytoscape.js**: Graph theory visualization library for Prompts plugin (MIT License)
- **mark.js**: Text highlighting library for Prompts plugin (MIT License)
- **marked**: Fast markdown parser for Prompts plugin (MIT License)
- **mermaid**: Generation of diagrams and flowcharts for Prompts plugin (MIT License)
- **24x24 Free Application Icons**: Application icons by Aha-Soft (Creative Commons Attribution 3.0 US)
