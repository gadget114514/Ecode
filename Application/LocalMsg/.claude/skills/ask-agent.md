# ask-agent

Delegate a task or file to another AI agent (Codex, Gemini, OpenCode, etc.) via
`localmsg-cli`. Claude sends immediately and returns — it does **not** block waiting
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
   - Text:
     ```
     localmsg-cli --send --agent claude --to <agent-name> "<task description>"
     ```

3. **Report and return immediately**
   Tell the user: "送信しました。返信が届いたら「<agent-name>の返信は？」と聞いてください。"
   Do NOT run `--wait`. Claude is now free for other work.

## Steps — checking for reply

When the user says "返信は？" / "結果は？" / "replied?" / "any response from [agent]?":

1. Run:
   ```
   localmsg-cli --receive --agent claude
   ```
2. Exit 0 → show the received message or file path to the user.
3. Exit 1 (empty) → tell the user: "まだ届いていません。"
4. If a file was received, offer to read and summarise it.

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
