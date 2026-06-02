# LocalMsg — Specification

LocalMsg is a LAN messenger plugin for Ecode that enables AI agents (Claude Code, Codex, OpenCode, Gemini CLI, etc.) to send and receive messages and files on the same machine or across the LAN.

- **Executable**: `bin/Release/plugins/LocalMsg.exe` (GUI server)
- **CLI**: `bin/Release/plugins/localmsg-cli.exe` (console client)

---

## Architecture

```
┌──────────────────────────┐     REST API (127.0.0.1:2426)
│  localmsg-cli (agent A)  │ ────────────────────┐
└──────────────────────────┘                     │
                                                 ▼
┌──────────────────────────┐     REST API     ┌──────────────────────────────┐
│  localmsg-cli (agent B)  │ ────────────────▶│  LocalMsg.exe (server)       │
└──────────────────────────┘                   │                              │
                                                │  ┌────────────────────────┐ │
┌──────────────────────────┐     REST API      │  │  g_messages[ ]         │ │
│  localmsg-cli (agent C)  │ ────────────────▶│  │  g_pseudoUsers[ ]      │ │
└──────────────────────────┘                   │  │  g_peers[ ]            │ │
                                                │  │  g_fileInbox{ }        │ │
                                                │  └────────────────────────┘ │
                                                │                              │
                                                │  HTTPS (0.0.0.0:53317)      │
                                                │  IPMsg UDP (0.0.0.0:2425)   │
                                                └──────────────────────────────┘
                                                         │
                                           ┌─────────────┴─────────────┐
                                           ▼                           ▼
                                    Remote LocalMsg             Remote IPMsg
                                    (LocalSend HTTPS)            client
```

### Processes

| Process | Subsystem | Role |
|---------|-----------|------|
| `LocalMsg.exe` | `WIN32` (GUI window) | Message server, REST API, LocalSend HTTPS, IPMsg UDP |
| `localmsg-cli.exe` | `CONSOLE` | CLI client for agents |

### Threads (inside LocalMsg.exe)

| Thread | Function | Line |
|--------|----------|------|
| HTTP Accept | `HttpAcceptThread` | Accepts TLS connections on port 53317 |
| HTTP Connection | `HttpConnThread` (per-connection) | TLS handshake, HTTP parsing, dispatch |
| UDP Discovery | `DiscoveryThread` | LocalSend UDP multicast/unicast |
| IPMsg | `IpMsgThread` | IPMsg UDP protocol on port 2425 |
| REST API | `RestApiThread` | REST API on 127.0.0.1:2426 |
| GUI (main) | `WndProc` | Window message loop and UI |

---

## Ports

| Protocol | Default Port | Binding | Config |
|----------|-------------|---------|--------|
| REST API | 2426 | `127.0.0.1` only | `LOCALMSG_HTTPPORT` env, `--httpport` flag |
| IPMsg UDP | 2425 | `0.0.0.0` (all interfaces) | `LOCALMSG_UDPPORT` env, `--udpport` flag |
| LocalSend HTTPS | 53317 | `0.0.0.0` (all interfaces) | Fixed (per LocalSend spec) |

Each port falls back to `port + 1` through `port + 5` if the default is occupied.

> **⚠ Notice — IPMsg coexistence**: LocalMsg uses IPMsg protocol on UDP port 2425. It **cannot coexist** with another IPMsg client (e.g., `ipmsg.exe`) on the same computer — both try to bind port 2425 and the second one fails. To run both, change the port via `LOCALMSG_UDPPORT` environment variable or `--udpport` flag.

> **⚠ Notice — Windows Firewall**: LocalMsg listens on **UDP 2425** (IPMsg) and **TCP 53317** (LocalSend HTTPS) on **all network interfaces** (`0.0.0.0`). Windows Defender Firewall may block these ports on private/public networks, preventing LAN discovery and file transfers from other machines. To allow LAN communication, add inbound rules:
> ```
> netsh advfirewall firewall add rule name="LocalMsg IPMsg"  dir=in protocol=udp localport=2425  action=allow profile=private
> netsh advfirewall firewall add rule name="LocalMsg HTTPS"  dir=in protocol=tcp localport=53317 action=allow profile=private
> ```
> The REST API on **TCP 2426** binds to `127.0.0.1` only and does not need a firewall rule.

---

## REST API

All REST endpoints are served on `127.0.0.1:<httpPort>` with `Content-Type: application/json`.

### `GET /api/ping`

Health check. No authentication required.

**Response**:
```json
{
  "ok": true,
  "status": "running",
  "rest_port": 2426,
  "users": ["claude", "codex"],
  "user_count": 2,
  "peer_count": 3
}
```

### `POST /api/login`

Register a pseudo-user (agent).

**Request**: `{"username": "claude"}`
**Response (201)**: `{"ok": true}`
**Response (409)**: `{"ok": false, "error": "username already exists"}`

The server also broadcasts an IPMsg `BR_ENTRY` packet on login.

### `POST /api/logout`

Unregister a pseudo-user.

**Request**: `{"username": "claude"}`
**Response**: `{"ok": true}`

The server broadcasts an IPMsg `BR_EXIT` packet on logout.

### `GET /api/users`

List all registered pseudo-users and discovered LAN peers.

**Response**:
```json
[
  {"username": "claude", "hostname": "PC-01", "ip": "", "protocol": "", "is_me": true},
  {"username": "gadge-pc", "hostname": "PC-02", "ip": "192.168.1.5", "protocol": "IPMsg", "is_me": false}
]
```

- `is_me: true` → pseudo-user on this machine
- `is_me: false` → discovered LAN peer

### `POST /api/send`

Send a text message.

**Request**: `{"from": "claude", "to": "codex", "text": "Hello"}`
**Response (200)**: `{"ok": true}`

**Routing logic**:
1. If `to` is a local pseudo-user → store in `g_messages` (local delivery)
2. If message size > 1024 bytes → send via **HTTPS** (`/api/localmsg/v1/message`) with `toUser` in payload
3. Otherwise → send via **IPMsg UDP**

### `GET /api/messages?user=<name>[&peek][&advance=0]`

Retrieve pending messages for a pseudo-user.

| Param | Default | Description |
|-------|---------|-------------|
| `user` | (primary user) | Pseudo-user name |
| `peek` | false | If set, don't advance read pointer |
| `advance` | 1 | Set to `0` to peek (same as `peek`) |

**Response**: `[{"id":1,"from":"codex","to":"claude","text":"Hello","time":"14:30:00"}, ...]`

### `GET /api/wait?user=<name>&timeout=<sec>`

Block until a new message arrives or timeout.

**Response (message)**: Same as `/api/messages`
**Response (timeout)**: `{"ok": false, "empty": true}`

### `GET /api/files?user=<name>[&peek]`

List files received for a pseudo-user via auto-accept.

**Response**: `[{"filename":"plan.md","path":"C:\\Users\\...","from":"codex","timestamp":"..."}]`

---

## LocalSend HTTPS Protocol

LocalMsg implements the [LocalSend v2 protocol](https://github.com/localsend/protocol) for file transfers and text messaging.

### Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/api/localsend/v2/info` | Device info (alias, version, fingerprint) |
| `POST` | `/api/localsend/v2/prepare-upload` | Prepare file upload (returns sessionId + tokens) |
| `POST` | `/api/localsend/v2/upload` | Upload file data |
| `POST` | `/api/localsend/v2/cancel` | Cancel active transfers |
| `POST` | `/api/localmsg/v1/message` | Receive text message with pseudo-user routing |

### `/api/localmsg/v1/message` — Text Message Payload

```
POST /api/localmsg/v1/message
Content-Type: text/plain; charset=utf-8

<senderAlias>\x01<toUser>\x01<text>
```

**Fields**:

| Field | Separator | Description |
|-------|-----------|-------------|
| `senderAlias` | `\x01` | Display name of the sender |
| `toUser` | `\x01` | Destination pseudo-user on the receiving side |
| `text` | (end of body) | Message content |

**Backward compatibility**: If `toUser` is omitted (legacy 2-field format `senderAlias\x01text`), the server delivers to the primary user.

**Server-side processing**:
1. Parse `toUser` from the body
2. If `IsPseudoUser(toUser)`, call `PushMessage(senderAlias, toUser, text)`
3. Otherwise, deliver to `GetPrimaryUser()`
4. Post `WM_IPMSG_TEXT_RCVD` to the GUI window for chat log display

### File Transfer Flow (agent-to-agent)

1. **prepare-upload**: CLI sends `POST /api/localsend/v2/prepare-upload` with `"destinationUser": "<agent>"` field
2. **Auto-accept**: Server detects `destinationUser` is a registered pseudo-user and auto-accepts
3. **Upload**: CLI sends file data via `POST /api/localsend/v2/upload`
4. **Storage**: File saved to `%USERPROFILE%/Downloads/`, metadata added to `g_fileInbox[destinationUser]`
5. **Retrieval**: Receiver calls `GET /api/files?user=<agent>` to list received files

---

## IPMsg Protocol

LocalMsg implements a subset of the IP Messenger (IPMsg) protocol for backwards compatibility with existing LAN clients.

### Supported Commands

| Command | Value | Direction |
|---------|-------|-----------|
| `IPMSG_BR_ENTRY` | `0x00000001` | In/Out |
| `IPMSG_BR_EXIT` | `0x00000002` | In/Out |
| `IPMSG_ANSENTRY` | `0x00000003` | In/Out |
| `IPMSG_SENDMSG` | `0x00000020` | In/Out |
| `IPMSG_RECVMSG` | `0x00000021` | In/Out |
| `IPMSG_READMSG` | `0x00000030` | In/Out |
| `IPMSG_DELMSG` | `0x00000031` | In |
| `IPMSG_ANSREADMSG` | `0x00000032` | In |

### Encoding

| Flag | Value | Behavior |
|------|-------|----------|
| `IPMSG_UTF8OPT` | `0x00800000` | Message body is UTF-8 |
| (absent) | — | Message body is system ANSI encoding (Shift-JIS on Japanese Windows) |

Outgoing messages always set `IPMSG_UTF8OPT | IPMSG_CAPUTF8OPT`.

---

## CLI Reference (`localmsg-cli.exe`)

### Commands

| Command | Flags | Description |
|---------|-------|-------------|
| `--login` | `--agent <name>` | Register a pseudo-user |
| `--logout` | `--agent <name>` | Unregister a pseudo-user |
| `--send` | `--agent <name> --to <peer> [<text>]` | Send text message (arg) |
| `--send` | `--agent <name> --to <peer> --stdin` | Send text message (stdin) |
| `--send` | `--agent <name> --to <peer> --file <path>` | Send file(s) |
| `--receive` | `--agent <name> [--peek]` | Dequeue next message |
| `--wait` | `--agent <name> [--timeout <sec>]` | Block until message arrives |
| `--receive --wait` | `--agent <name> [--timeout <sec>]` | Drain then block |
| `--files` | `--agent <name> [--peek]` | List received files |
| `--ping` | (none) | Server health check |
| `--list` | (none) | List registered users and peers |

### Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Empty/timeout (for `--receive`, `--wait`) |
| 2 | Error (missing args, server not running) |

### Port Overrides

```bash
localmsg-cli --send ... --httpport 2427 --udpport 2426
```

Or via environment:
```bash
set LOCALMSG_HTTPPORT=2427
set LOCALMSG_UDPPORT=2426
```

---

## Encoding

### Internal Encoding

| Component | Encoding | Rationale |
|-----------|----------|-----------|
| `std::wstring` fields | UTF-16 | Windows API requirement (WinHttp, WinSock, GUI) |
| `std::string` JSON | UTF-8 | Network transport (HTTP, IPMsg) |
| Console output | UTF-16 via `WriteConsoleW` | Bypasses console code page |
| Pipe/redirect fallback | Raw UTF-8 bytes | Caller sets `[Console]::OutputEncoding = UTF-8` |
| stdin input | Raw bytes (UTF-8 or OEM CP) | Depends on `$OutputEncoding` |

### Conversion Functions

All use `CP_UTF8` exclusively:

- `ws2s(w)`: `WideCharToMultiByte(CP_UTF8, ...)` — UTF-16 → UTF-8
- `s2ws(s)`: `MultiByteToWideChar(CP_UTF8, ...)` — UTF-8 → UTF-16

### PowerShell Pipe Encoding

When piping text to `localmsg-cli --stdin`, both must be set:

```powershell
$utf8 = [System.Text.UTF8Encoding]::new($false)  # BOM-less UTF-8
$OutputEncoding = [Console]::OutputEncoding = $utf8
Write-Output "日本語" | localmsg-cli --send --agent claude --to codex --stdin
```

- `$OutputEncoding` controls **pipe input** to native executables (default: ASCII!)
- `[Console]::OutputEncoding` controls **pipe output** capture from native executables
- Using `[Text.Encoding]::UTF8` adds a BOM (`EF BB BF`) — use `UTF8Encoding::new($false)` instead

---

## Large Message Transport

Messages > 1 KB are automatically switched from IPMsg UDP to LocalSend HTTPS:

```
┌──────────┐   POST /api/send    ┌──────────────┐
│  CLI     │ ──────────────────▶ │  LocalMsg    │
│ (sender) │                     │  (server)    │
└──────────┘                     └──────┬───────┘
                                        │
                          ┌─────────────┴─────────────┐
                          │ text.size() > 1024?       │
                          └─────────────┬─────────────┘
                                        │
                    ┌───────────────────┴───────────────────┐
                    ▼                                       ▼
           ┌──────────────────┐                    ┌──────────────────┐
           │ SendTextHttps()  │                    │ SendTextIpMsg()  │
           │ (LocalSend 53317)│                    │ (IPMsg UDP 2425) │
           └────────┬─────────┘                    └────────┬─────────┘
                    │                                       │
                    ▼                                       ▼
       POST /api/localmsg/v1/message              UDP datagram
       Body: sender\x01toUser\x01text             (limited to ~4KB)
```

---

## Data Structures

### `g_messages` — Message queue

```cpp
struct Message {
    int id;
    std::wstring from;
    std::wstring to;
    std::wstring text;
    std::wstring time;
};
static std::vector<Message> g_messages;  // capped at 1000
static std::map<std::wstring, int> g_msgReadPtr;  // per-user read cursor
```

### `g_pseudoUsers` — Registered agents

```cpp
struct PseudoUser {
    std::wstring username;
    std::wstring hostname;
    bool active = true;
};
static std::vector<PseudoUser> g_pseudoUsers;
```

### `g_peers` — Discovered LAN peers

```cpp
enum class Proto { LocalSend, IPMsg };
struct PeerInfo {
    std::wstring alias;
    std::wstring hostname;
    std::wstring ip;
    int          port;         // LocalSend HTTPS port
    DWORD        lastSeenMs;
    Proto        protocol;
};
static std::vector<PeerInfo> g_peers;  // TTL: 120s
```

### `g_fileInbox` — Received files

```cpp
struct ReceivedFile {
    std::wstring filename;
    std::wstring localPath;   // %USERPROFILE%/Downloads/
    std::wstring from;
    std::wstring timestamp;
};
static std::map<std::wstring, std::vector<ReceivedFile>> g_fileInbox;
```

---

## Agent Workflow

### Registration

```
Agent A                          LocalMsg Server
  │                                    │
  ├── POST /api/login ────────────────▶│
  │    {"username":"claude"}           │
  │◀─────────── {"ok":true} ───────────┤
  │                                    ├── IPMsg BR_ENTRY (broadcast)
  │                                    │
  │◀── IPMsg ANSENTRY (from peers) ───┤
```

### Message Send (local)

```
Agent A                          LocalMsg Server
  │                                    │
  ├── POST /api/send ────────────────▶│
  │    {"from":"claude",              │
  │     "to":"codex",                 │
  │     "text":"Hello"}               │
  │◀─────────── {"ok":true} ──────────┤
  │                                    ├── PushMessage("claude","codex","Hello")
  │                                    ├── PostMessage(WM_IPMSG_TEXT_RCVD)
```

### Message Receive

```
Agent B                          LocalMsg Server
  │                                    │
  ├── GET /api/wait?user=codex ───────▶│
  │         &timeout=120               │
  │                                    │ (blocks until message arrives)
  │◀─── [{"id":1,"from":"claude", ─────┤
  │        "to":"codex",               │
  │        "text":"Hello"}]            │
  │                                    ├── advance read pointer
```

### Large Message (>1KB)

```
Agent A (sender)                 LocalMsg Server A         LocalMsg Server B
      │                                │                        │
      ├─ POST /api/send ──────────────▶│                        │
      │  text.size() = 2048            │                        │
      │                                ├─ SendTextHttps() ─────▶│
      │                                │  POST /api/localmsg/   │
      │                                │  v1/message            │
      │                                │  body: claude\x01      │
      │                                │  codex\x01<long text>  │
      │                                │                        ├─ PushMessage
      │                                │                        ├─ WM_IPMSG_TEXT_RCVD
      │◀─────────── {"ok":true} ───────┤                        │
```

### File Transfer (agent-to-agent)

```
Agent A (sender)                 LocalMsg Server A         LocalMsg Server B
      │                                │                        │
      ├─ POST /api/localsend/v2/ ──────▶│                        │
      │  prepare-upload                 │                        │
      │  destinationUser:"codex"        │                        │
      │◀── sessionId + token ──────────┤                        │
      │                                │(auto-accept for agent) │
      ├─ POST /api/localsend/v2/ ──────▶│                        │
      │  upload (file data)             │                        │
      │                                ├── save to Downloads/ ──┤
      │                                ├── g_fileInbox[codex]   │
      │◀─────────── 200 OK ────────────┤                        │
      │                                │                        │
Agent B                           LocalMsg Server B
      │                                    │
      ├── GET /api/files?user=codex ──────▶│
      │◀── [{"filename":"plan.md",...}] ───┤
```

---

## Build

```powershell
# From repository root
cmake -S Application/LocalMsg -B Application/LocalMsg/build
cmake --build Application/LocalMsg/build --config Release
```

Output:
- `bin/Release/plugins/LocalMsg.exe`
- `bin/Release/plugins/localmsg-cli.exe`

Dependencies: Visual Studio 2022, Windows SDK 10.0.26100.0+, mbedTLS (in-tree at `third_party/mbedtls/`)

---

## Shared Mode

When ecode's **Settings → General → Share LocalMsg** is enabled:
- All ecode instances share one `LocalMsg.exe` on default ports
- `LOCALMSG_HTTPPORT` / `LOCALMSG_UDPPORT` are NOT set → CLI uses defaults (2426/2425)
- Agents from all instances share the same message queue

When disabled (default):
- Each ecode instance runs its own `LocalMsg.exe` with unique ports
- Ports passed via environment variables to child `localmsg-cli` processes
