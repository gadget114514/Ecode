---
name: ask-agent
description: Delegate tasks to other AI agents (Codex, Gemini, OpenCode) via localmsg-cli. Use when the user says "ask [agent] to...", "[agent]に聞いて", "/ask-agent", "send this to [agent]", "delegate to [agent]", or asks for replies from another agent ("返信は？", "結果は？", "replied?", "any response?").
---

# ask-agent

Delegate a task or file to another AI agent (Codex, Gemini, OpenCode, etc.) via
`localmsg-cli`. Sends immediately and returns — does **not** block waiting
for a reply. The user asks separately when they want the result.

## Usage

```
/ask-agent <agent-name> <task description>
/ask-agent <agent-name> --file <path>
/ask-agent <agent-name> --file <path> <optional note>
```

## Steps — sending (non-blocking)

1. **Confirm the target agent is available**
   ```
   localmsg-cli --list
   ```
   Check that `<agent-name>` appears. If not found, report to the user and stop.

2. **Send the task**
   - File:
     ```
     localmsg-cli --send --agent claude --to <agent-name> --file <path>
     ```
   - Text (stdin — always use this for non-ASCII text):
     ```
     $utf8 = [System.Text.UTF8Encoding]::new($false)
     $OutputEncoding = [Console]::OutputEncoding = $utf8
     Write-Output "<task description>" | localmsg-cli --send --agent claude --to <agent-name> --stdin
     ```
   - Text (legacy, may garble Japanese on some shells):
     ```
     localmsg-cli --send --agent claude --to <agent-name> "<task description>"
     ```

3. **Report and return immediately**
   Tell the user: "送信しました。返信が届いたら「<agent-name>の返信は？」と聞いてください。"
   Do NOT run `--wait`. You are now free for other work.

## Steps — checking for reply

When the user says "返信は？" / "結果は？" / "replied?" / "any response from [agent]?":

1. Run:
   ```
   localmsg-cli --receive --agent claude
   ```
2. Exit 0 → show the received message or file path to the user.
3. Exit 1 (empty) → tell the user: "まだ届いていません。"
4. If a file was received, offer to read and summarise it.

## Steps — running as an agent (wait loop)

When running as a child/sub-agent that continuously waits for tasks:

1. **Login** as the agent:
   ```
   localmsg-cli --login --agent <name>
   ```

2. **Start wait loop** (PowerShell):
   ```powershell
   .opencode\skills\ask-agent\agent-wait.ps1 -agent <name>
   ```
   Or manually:
   ```powershell
   while ($true) {
       $r = localmsg-cli --wait --agent <name> --timeout 120
       if ($LASTEXITCODE -eq 0) { Write-Host $r }
   }
   ```

3. The loop blocks until a message arrives (up to 120s timeout), processes it, and immediately waits again.

4. **Reply** to a task (stdin):
   ```powershell
   $utf8 = [System.Text.UTF8Encoding]::new($false)
   $OutputEncoding = [Console]::OutputEncoding = $utf8
   Write-Output "<result>" | localmsg-cli --send --agent <name> --to <sender> --stdin
   ```

## Examples

```
User: codexにこのプランをレビューさせて
→ localmsg-cli --send --agent claude --to codex --file plan.md
→ "送信しました。返信が届いたら「codexの返信は？」と聞いてください。"

User: codexの返信は？
→ localmsg-cli --receive --agent claude
→ 届いていればレビュー内容を表示、なければ "まだ届いていません。"

User: ask gemini to suggest optimisations for src/main.cpp
→ localmsg-cli --send --agent claude --to gemini --file src/main.cpp "suggest optimisations"
→ "Sent. Ask me 'any response from gemini?' when you want to check."

User: any response from gemini?
→ localmsg-cli --receive --agent claude
```

## Skill trigger phrases (Japanese)

- "[エージェント名]に聞いて"
- "[エージェント名]にレビューさせて"
- "[エージェント名]にこれを送って"
- "[エージェント名]に依頼して"
- "[エージェント名]の返信は？"
- "[エージェント名]から返事きた？"
- "[エージェント名]の結果は？"

## Skill trigger phrases (English)

- "ask [agent] to …"
- "have [agent] review …"
- "send this to [agent]"
- "delegate to [agent]"
- "any response from [agent]?"
- "[agent] replied?"
- "/ask-agent …"
