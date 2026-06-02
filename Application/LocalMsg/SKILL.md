# localmsg-cli — Cross-Agent Messaging Skill

Enable AI agents (Claude Code, Codex, Gemini CLI, OpenCode, etc.) to send and receive
messages and files on the same machine using `localmsg-cli.exe`.

`localmsg.exe` is started automatically on first use — no manual setup required.

---

## Agent Identity

Every command requires `--agent <name>` to identify who is calling.
The name must match the pseudo-peer you registered with `--login`.

**Fallback (no flag needed):** If the current working directory contains a file
named `.agent-identity`, the CLI reads the agent name from it automatically.

```
.agent/
  SKILL.md              ← this file (shared)
  claude/
    .agent-identity     ← contains: claude
  codex/
    .agent-identity     ← contains: codex
  gemini/
    .agent-identity     ← contains: gemini
```

When an agent's CWD is `.agent/claude/`, `--agent claude` is inferred automatically.

---

## Commands

### Register / unregister
```bash
localmsg-cli --login  --agent <name>    # announce presence; auto-starts localmsg.exe
localmsg-cli --logout --agent <name>
```

### Send a text message
```bash
localmsg-cli --send --agent <name> --to <peer> <text>
```

### Send one or more files
```bash
localmsg-cli --send --agent <name> --to <peer> --file <path> [--file <path2> ...]
```

### Receive next message (dequeue)
```bash
localmsg-cli --receive --agent <name>
# Exit 0 → message printed as JSON
# Exit 1 → inbox empty
```

### Wait for a message (blocking)
```bash
localmsg-cli --wait --agent <name> [--timeout <seconds>]
# Blocks until a message arrives or timeout expires (default 30s)
# Exit 1 on timeout
```

### List received files
```bash
localmsg-cli --files --agent <name>
# → [{"filename":"plan.md","path":"C:\\...","from":"codex","timestamp":"..."}]
```

### List all known agents
```bash
localmsg-cli --list
# → [{"username":"codex","ip":"127.0.0.1","is_me":false}, ...]
```

---

## Typical Agent-to-Agent Flow

```bash
# Agent A (Claude) delegates a review to Agent B (Codex)
localmsg-cli --send --agent claude --to codex --file plan.md

# Agent B (Codex) — waiting for work
localmsg-cli --wait --agent codex --timeout 60
# receives: {"from":"claude","filename":"plan.md","path":"C:\\..."}
# ... Codex reads the file and composes a reply ...
localmsg-cli --send --agent codex --to claude "LGTM — minor comment on section 3."

# Agent A waits for the reply
localmsg-cli --wait --agent claude --timeout 120
# → {"from":"codex","text":"LGTM — minor comment on section 3."}
```

---

## Cancelling a Wait Loop

`--wait` blocks until a message arrives. Press **Ctrl+C** at any time to cancel.

For programmatic cancellation, send a `STOP` message to the waiting agent:

```bash
localmsg-cli --send --agent human --to <agent> "STOP"
```

An agent loop should check for `STOP` after each timeout:

```bash
# Pseudo-code for agent loop
while true:
  localmsg-cli --wait --agent <name> --timeout 30
  if exit 1 (timeout):
    msg=$(localmsg-cli --receive --agent <name>)
    if msg contains "STOP": break
  process message
```

---

## Notes

- `localmsg.exe` launches automatically (hidden) when `localmsg-cli` is first run.
- Multiple agents run simultaneously; each gets its own server instance on a unique port.
- Port assignments: `%LOCALAPPDATA%\localmsg\agents\<name>.port`
- File transfers use LocalSend HTTPS on localhost — no external network required.
- `--agent` is **required**. If omitted, the CLI checks for `.agent-identity` in the CWD.
  If neither is present, the command fails with a clear error.
