---
name: agent-messaging
description: Cross-agent messaging via localmsg-cli. Send/receive messages and files between AI agents on the same machine using localmsg-cli.exe.
---

# agent-messaging

エージェント間でメッセージやファイルを送受信するスキル。
`localmsg-cli.exe` を使用して同一マシン上の別エージェントと通信する。

## Agent Identity

すべてのコマンドに `--agent <name>` が必要。

```
localmsg-cli --send --agent <name> --to <peer> "message"
localmsg-cli --receive --agent <name>
```

## エラーハンドリング（必須）

他エージェントから受け取ったリクエストの処理中にエラーが発生した場合、
**必ずエラー内容を依頼元に返信すること。**

```bash
msg=$(localmsg-cli --receive --agent <name>)
from=$(echo "$msg" | ...)  # 依頼元を特定

if error; then
  localmsg-cli --send --agent <name> --to "$from" "ERROR: <エラー内容>"
fi
```

エラーを握りつぶさず、依頼元が状況を把握できるようにする。

## 基本コマンド

### ログイン / ログアウト
```bash
localmsg-cli --login  --agent <name>
localmsg-cli --logout --agent <name>
```

### メッセージ送信
```bash
localmsg-cli --send --agent <name> --to <peer> "text"
localmsg-cli --send --agent <name> --to <peer> --file <path>
```

### メッセージ受信
```bash
localmsg-cli --receive --agent <name>
localmsg-cli --wait --agent <name> [--timeout <sec>]
```

### エージェント一覧
```bash
localmsg-cli --list
```

## 典型的なフロー

```bash
# エージェントA → エージェントB にリクエスト
localmsg-cli --send --agent A --to B "調査して"

# エージェントB が受信して処理
msg=$(localmsg-cli --receive --agent B)
# ...処理...
if ok; then
  localmsg-cli --send --agent B --to A "結果"
else
  localmsg-cli --send --agent B --to A "ERROR: 理由"
fi
```
