# Prompts Application Design

## Overview

Prompts is a desktop application for **managing** and **executing** AI prompt strings. It is an "IDE for prompts" — users can create, edit, organize, run AI pipelines, and accumulate results — all within a tree-based node structure with multiple tabs.

**Architecture:** WebView2 (whole UI) + RichEdit HWND (RTF editing) hybrid.

## Directory Structure

```
Application/Prompts/
├── CMakeLists.txt
├── resources/
│   ├── resource.h
│   └── resource.rc
├── frontend/                         ← WebView2 で読み込む UI
│   ├── index.html                    ← エントリポイント
│   ├── app.js                        ← メインロジック（Tree/List/Editor/Messages）
│   ├── style.css
│   ├── lang/                         ← 翻訳ファイル（JSON）
│   │   ├── en.json
│   │   ├── ja.json
│   │   ├── fr.json
│   │   ├── es.json
│   │   ├── pt.json
│   │   └── de.json
│   └── lib/
│       ├── marked.min.js             ← Markdown → HTML 変換
│       ├── mark.min.js               ← 検索ハイライト
│       ├── mermaid.min.js            ← フロー図自動生成（Basic モード）
│       └── cytoscape.min.js          ← インタラクティブノードエディタ（Expert モード将来）
└── src/
    ├── main.cpp
    ├── App.h/cpp                     ← ウィンドウ管理・WebView2 ホスト・RichEdit HWND
    ├── Bridge.h/cpp                  ← Win32 ↔ JS メッセージブリッジ
    ├── PromptsLocalization.h/cpp     ← ローカライゼーション（C++ 側ログ・固定文字列）
    ├── NodeData.h/cpp                ← データモデル（Node, Attachment, Tab）
    ├── Storage.h/cpp                 ← File I/O (session.json, data/*.json, blobs/)
    ├── PipelineRunner.h/cpp          ← AI パイプライン実行（WinHTTP 非同期）
    ├── AIProvider.h/cpp              ← AI プロバイダ共通インターフェース
    ├── JsonParser.h/cpp              ← 手書き再帰下降 JSON パーサ
    └── Base64.h/cpp                  ← Base64 エンコード/デコード
```

## UserData Storage

```
%APPDATA%/Ecode/Prompts/
├── session.json         ← Tab list (authoritative; lone source of truth for startup)
├── providers.json       ← API keys (NOT in source tree, NOT in git)
├── pipeline.json        ← Pipeline definitions (no keys)
├── data/
│   ├── general.json     ← Per-tab tree data
│   ├── code.json
│   └── ...
├── blobs/               ← External media files from pipeline runs
│   ├── pipeline_20250530_153042_0.png
│   ├── pipeline_20250530_153042_1.webp
│   └── ...
└── history/              ← Pipeline execution history
    └── run_20250530_153042.json
```

- Path: `SHGetFolderPathW(CSIDL_APPDATA)` + `Ecode\Prompts\`
- Created on first launch
- **Startup behavior**: `session.json` → read `tabs[]` → load each `file` → restore all tabs.  
  If `tabs[]` is empty, auto-create `data/general.json` with a single empty node and add it.
- `providers.json` contains API keys — NEVER stored in source tree
- `blobs/` contains large media files from pipeline runs, referenced externally

### Blob Garbage Collection

| タイミング | 動作 |
|-----------|------|
| **アプリ起動時** | `blobs/` をスキャン、全 `data/*.json` の `attachments[].file` から参照されていないファイルを削除（確認なし） |
| **Node 削除時** | その node の attachment が参照する blob ファイルを削除 |
| **手動** | Config に "Clean up unreferenced blobs" ボタン |

参照チェック: 全 `data/*.json` + `history/run_*.json` をパースし、`attachments[].file` に登場するパスのみ保持。それ以外を削除する。
history が参照している blob ファイルは、history が存在する限り削除しない（history 削除時に合わせて削除）。

## Architecture

```
 ┌─────────────────────────────────────────────────────┐
 │  Win32 Shell（薄いホスト層）                          │
 │  ┌─────────────────────────────────────────────┐    │
 │  │  WebView2（全体 UI）                          │    │
 │  │  Tree(JS) │ List(JS) │ Editor(JS/HTML)       │    │
 │  │  Messages / Toolbar / Pipeline Runner (JS)   │    │
 │  └──────────────────────────┬──────────────────┘    │
 │                             │ RTF ノード選択時        │
 │  ┌──────────────────────────▼──────────────────┐    │
 │  │  RichEdit HWND（エディタエリアに重ねて表示）   │    │
 │  └─────────────────────────────────────────────┘    │
 │  ┌─────────────────────────────────────────────┐    │
 │  │  C++ Backend                                 │    │
 │  │  Storage / PipelineRunner / AIProvider       │    │
 │  │  Bridge (PostWebMessageAsJson ↔ JS)          │    │
 │  └─────────────────────────────────────────────┘    │
 └─────────────────────────────────────────────────────┘
```

## UI Architecture（WebView2 全 UI 原則）

**視覚的な UI はすべて WebView2（HTML+JS）で実装する。**
C++ は OS レベルの操作のみを担当し、Bridge 経由で JS と連携する。

### C++ が担う操作（OS 操作のみ）

| 操作 | API |
|------|-----|
| ファイル開く/保存ダイアログ | `GetOpenFileName` / `GetSaveFileName` |
| RichEdit HWND（RTF 編集） | `riched20.dll` |
| クリップボード（Win32 固有形式） | `CF_RTF`、JSON payload の `CF_UNICODETEXT` |
| ファイル I/O | JSON 読み書き、blob 保存 |
| PipelineRunner | WinHTTP 非同期、`CreateProcess` |

### WebView2（JS）が担う UI

| UI 要素 | 実装方法 |
|---------|---------|
| コンテキストメニュー | HTML カスタムドロップダウン（`contextmenu` イベントで OS メニューを抑制） |
| Config ダイアログ | HTML モーダルパネル（CSS overlay） |
| Pipeline Runner ダイアログ | HTML モーダルパネル + mermaid.js フロー図 |
| Dynamic Queue 編集 UI | HTML フォーム（モーダル内） |
| Splitter | CSS/JS リサイズ |
| ノード DnD（並び替え） | HTML5 DnD API |
| ファイル DnD（ドロップ） | `drop` イベントでファイル取得 → Bridge で C++ に渡す |
| 検索バー | HTML インライン UI |
| RTF フォーマットツールバー | HTML ボタン → Bridge → C++ → `EM_SETCHARFORMAT` |
| メディア再生（WAV 等） | HTML `<audio>` / `<video>` タグ |

### UI ライブラリ方針

Vanilla JS（フレームワーク・バンドラ不使用）で実装する。

- **状態管理**: `window.appState = { tabs, nodes, pipeline }`（シングルステート）
- **UI 更新**: Bridge メッセージ受信 → appState 更新 → 該当ペーンの再描画関数を呼ぶ
- **ファイル構成**: `index.html` / `app.js` / `style.css` の3ファイル + `lib/`（依存ライブラリ）
- **依存ライブラリ**: marked.js / mark.js / mermaid.js のみ（すべて Vanilla JS 対応）
- **移植性**: HTML/JS/CSS をそのまま Electron に移せる（C++ バックエンドのみ再実装）

### ファイルダイアログの Bridge フロー（全 Open/Save 共通）

```
JS:  postMessage({ type: "open_file_dialog", filter: "Images|*.png;*.jpg" })
C++: GetOpenFileName → パス取得
C++: PostWebMessageAsJson({ type: "open_file_dialog_result", path: "C:\\..." })
JS:  結果を受け取って処理
```

### クリップボードの Bridge フロー

| 操作 | 実装 |
|------|------|
| テキストコピー（text/plain, text/html） | JS: `navigator.clipboard.writeText()` |
| RTF コピー | JS → Bridge → C++: `SetClipboardData(CF_RTF, ...)` |
| Node JSON コピー | JS → Bridge → C++: `SetClipboardData(CF_UNICODETEXT, json)` |
| ペースト | JS → Bridge → C++: `GetClipboardData` → 結果を JS に返す |

### frontend/ 配信方法

```cpp
webview->SetVirtualHostNameToFolderMapping(
    L"prompts.app", frontendPath,
    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
webview->Navigate(L"https://prompts.app/index.html");
```

開発中はソース直接編集可能、F12 DevTools 使用可。リリース時も同一方式。

## Localization

6 言語対応。JS 側（UI 文字列）と C++ 側（Messages ペインのログ）の両方に適用する。

### 対応言語

| コード | 言語 |
|--------|------|
| `en` | English |
| `ja` | 日本語 |
| `fr` | Français |
| `es` | Español |
| `pt` | Português |
| `de` | Deutsch |

### 実装方式

JYEditor と同様の C++ static map 方式 + WebView2 JS 側の JSON 翻訳ファイルの2層構造。

**C++ 側（Messages ログ、Config ダイアログの固定文字列）:**

```cpp
class PromptsLocalization {
    std::string m_currentLang = "en";
    std::wstring GetString(const std::string &key) const;
    // 内部: static map<string, map<string, wstring>> で全言語の翻訳を保持
};
```

**JS 側（UI 全般）:**

```
frontend/
├── index.html
├── app.js
├── style.css
├── lib/ ...
└── lang/                           ← 翻訳ファイル
    ├── en.json
    ├── ja.json
    ├── fr.json
    ├── es.json
    ├── pt.json
    └── de.json
```

起動時に C++ → JS: `init` メッセージに `{ language: "ja" }` を含めて通知。
JS 側は `fetch("lang/ja.json")` で該当ファイルを読み込み、UI 文字列を差し替える。

### 類似ソフトウェアとの比較

Prompts は n8n のローカル特化版 + プロンプト管理に相当する。

#### n8n 詳細比較

| カテゴリ | 項目 | n8n | Prompts |
|---------|------|-----|---------|
| **基本設計** | アーキテクチャ | Web (React) + Node.js バックエンド | Win32 + WebView2 (HTML/JS) |
| | 実行環境 | SaaS / Docker self-hosted | ローカル Win32 ネイティブ |
| | オフライン動作 | ❌ 一部不可 | ✅ 完全オフライン |
| | データ保存 | 内部 DB (SQLite/PostgreSQL) + エクスポート | ローカル JSON ファイル (`%APPDATA%`) |
| | ライセンス | Sustainable Use License (SSPL 類似) | 独自（ecode 準拠） |
| **ノード/ステップ** | 総ノード数 | 200+（サービス別コネクタ） | 5 種汎用ステップ + OS コマンド委譲 |
| | AI ノード | OpenAI / Anthropic / Ollama / HuggingFace 等 | `ai` (4 プロバイダ統合) |
| | ファイル変換 | CSV / XML / JSON / Excel 専用ノード | `command` (任意 CLI) |
| | 画像処理 | なし（専用ノードなし） | `command` (ffmpeg/ImageMagick) + image ノード |
| | ユーザー確認 | Wait + Webhook（間接的） | `manual` (view/edit/select) + `choices[]` |
| | 外部サービス連携 | 200+ ノード（各サービス専用） | `fetch` + auth (providers.json) |
| **実行制御** | 条件分岐 | IF / Switch ノード | `onSelect.goto_step`（ループ含む）/ `condition` ステップ |
| | 並列実行 | Split / Merge In Batches | `"parallel"` ステップ（Basic 互換）/ Cytoscape.js Expert モード（将来） |
| | エラーハンドリング | Error Trigger / Retry | `retry.count` / `delayMs` / `onSelect` |
| | 実行中編集 | ❌ 不可 | ✅ Dynamic Queue (`std::deque`) |
| | 手動トリガー | Manual Trigger ノード | Toolbar `[▶ Run Pipeline]` |
| **UI/UX** | フロー図 | インタラクティブノードエディタ | mermaid.js 自動生成（Basic）/ Cytoscape（Expert） |
| | 実行中ハイライト | ✅ ノード色変更 | ✅ ハイライト + 点滅 |
| | プログレス表示 | 全体の進行状況バー | ステップ単位バー + スピナー |
| | ログ表示 | Execution タブ | Messages 第4ペイン |
| | 実行履歴 | ✅ 全実行記録・ステップ別入出力確認 | ✅ `history/` + History UI |
| **データ管理** | プロンプト編集 | ❌ なし（専用エディタなし） | ✅ 内蔵エディタ (text/RTF/HTML/image) |
| | プロンプト蓄積・再利用 | ❌ なし | ✅ ツリーノード + 子ノード履歴 |
| | 添付ファイル | ❌ なし | ✅ attachments + `blobs/` |
| | 全文検索 | ❌ なし | ✅ Ctrl+F 全タブ横断 |
| | エクスポート | JSON ワークフロー定義 | ZIP (Node + blobs) |
| **拡張性** | カスタムコード | Code / Function ノード (JS/Python) | `command` (任意言語) |
| | テンプレート変数 | Expression (`{{ $json.xxx }}`) | `variables[]` + `{content}` / `{result}` |
| | サブルーチン | Sub-workflow ノード | Dynamic Queue `[+ Pipeline]` |
| | コミュニティ共有 | n8n.io ワークフロー公開 | 設計段階（将来検討） |

#### Prompts の独自優位点（対 n8n）

1. **プロンプト管理 + 実行の一体化** — 単なるパイプラインエディタではなく、プロンプトをツリーで管理・蓄積・再利用する
2. **ローカルファースト完全オフライン** — 設定・データ・パイプラインがすべてローカルファイル。API キーもローカル保存
3. **マルチモーダルノード** — text / RTF / HTML / image を単一ノードに格納 + 添付ファイル
4. **Dynamic Queue 実行中編集** — パイプライン実行中に待機ステップの追加・編集・削除・順序変更が可能
5. **OS ツール統合** — awk/sed/ffmpeg/ImageMagick 等の CLI ツールや GUI アプリを直接パイプラインに統合
6. **ecode 埋め込み** — エディタ内でタブとして利用可能

## Layout

```
Menu* → Toolbar → Tab bar → WebView2 全体 UI（3-pane(Tree / List / Editor) + Bottom(Messages)）
RTF ノード選択時のみ RichEdit HWND をエディタエリアに重ねて表示
```
- `*` Menu bar visible in standalone mode only (hidden with `--embedded`)

### --embedded モード詳細

| 項目 | Standalone | Embedded (`--embedded`) |
|------|-----------|------------------------|
| メニューバー | ✅ 表示 | ❌ 非表示 |
| ウィンドウ枠 | `WS_OVERLAPPEDWINDOW` | `WS_POPUP`（ecode が `SetParent` 後に `WS_CHILD` を付与） |
| タイトルバー | 標準タイトルバー | なし |
| サイズ変更 | 手動リサイズ可 | ecode の `WM_SIZE` に追随 |
| 閉じる動作 | `WM_CLOSE` → `SaveData` → `DestroyWindow` | `WM_CLOSE` → `SaveData` → `PostQuitMessage(0)` |
| プロセス管理 | なし | ecode が `hProcess` を管理、`TerminateProcess` 可能 |
| 多重起動 | 可能（独立ウィンドウ） | 不可（ecode が 1 タブ = 1 プロセスとして管理） |

### Panes

| Pane | Position | Content | Collapsible |
|------|----------|---------|-------------|
| Tree | Left | Full hierarchy tree view | ✅ |
| List | Right-top | Children + nav `[▲][◀ Up][▶ Down]` breadcrumb + copy | ✅ |
| Editor | Right-bottom | Title + Content per mimetype + Attachments + CRUD | ✅ |
| Messages | Bottom | Operation log + pipeline streaming output | ✅ |

### Toolbar

`[📄 New] [📂 Open] [💾 Save] [💾 Save As] │ [▶ Run Pipeline] ⚙ Config`

### Splitters

- Vertical: Tree ↔ Right panes
- Horizontal: List ↔ Editor
- Cross splitter (drag to resize)

## Node Data Format

```json
{
  "title": "base64...",
  "content": "base64...",
  "mimetype": "text/plain | text/html | application/rtf | image/png | image/jpeg | image/webp",
  "attachments": [
    {
      "id": "img_001",
      "mimetype": "image/png",
      "inline": true,
      "content": "base64..."            ← small files inline
    },
    {
      "id": "img_002",
      "mimetype": "image/webp",
      "file": "blobs/pipe_2025...png",  ← pipeline media: external ref
      "size": 52428800
    }
  ],
  "children": [ ... ]
}
```

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `title` | string | base64. If empty, auto-generated from content |
| `content` | string | base64 body |
| `mimetype` | string | One of 6 types |
| `attachments` | array | Optional. Media attached to the node |
| `children` | array | Recursive child nodes |

### Title Display Rules

| Condition | Display |
|-----------|---------|
| `title` non-empty | decode and display |
| `title` empty + `text/plain` | first 3-5 words + `...` |
| `title` empty + `text/html` | `[HTML] nnn bytes` |
| `title` empty + `application/rtf` | `[RTF] nnn bytes` |
| `title` empty + `image/*` | `[Image] nnn KB` |

### Media Storage Rules

| Source | Storage | Format |
|--------|---------|--------|
| Manually attached (DnD, small) | `data/*.json` inline | base64 (`inline: true`) |
| Pipeline-generated media | `blobs/` directory | External file ref (`file: "blobs/..."`) |

Threshold: `< 1 MB` inline, `>= 1 MB` external (configurable).

## session.json

起動時に全 `tabs[]` を読み込む。`initialLoadFiles` は存在せず、`tabs` が唯一のタブリスト。

```json
{
  "tabs": [
    { "name": "General", "file": "data/general.json" },
    { "name": "Code",    "file": "data/code.json" }
  ]
}
```

## Bridge（Win32 ↔ JS 通信）

C++ → JS: `webview->PostWebMessageAsJson(L"{...}")`
JS → C++: `window.chrome.webview.postMessage({...})`

### メッセージ一覧

| 方向 | type | payload | 説明 |
|------|------|---------|------|
| C++→JS | `init` | `{tabs, nodes, pipelines}` | 起動時の全データ送信 |
| C++→JS | `node_updated` | `{tabId, node}` | ノード変更後の同期 |
| C++→JS | `stream_chunk` | `{stepIndex, text}` | ストリーミングチャンク |
| C++→JS | `step_done` | `{stepIndex, tokens, ms}` | ステップ完了 |
| C++→JS | `pipeline_error` | `{stepIndex, message}` | エラー通知 |
| C++→JS | `step_started` | `{index, name}` | ステップ開始（mermaid ハイライト用） |
| C++→JS | `trigger_fired` | `{pipelineName, triggerType}` | トリガー発火通知 |
| C++→JS | `rtf_position` | `{x, y, w, h}` | RichEdit 位置要求への応答 |
| JS→C++ | `save_node` | `{tabId, node}` | ノード保存要求 |
| JS→C++ | `run_pipeline` | `{pipelineName, nodeId}` | パイプライン実行 |
| JS→C++ | `run_pipeline_queue` | `{steps[], nodeId}` | 動的キューで実行 |
| JS→C++ | `cancel_pipeline` | `{}` | キャンセル |
| JS→C++ | `update_pending_step` | `{index, step}` | 実行中に待機ステップを編集 |
| JS→C++ | `remove_pending_step` | `{index}` | 待機ステップを削除 |
| JS→C++ | `append_step` | `{step}` | 実行中に単一ステップをキュー末尾追加 |
| JS→C++ | `append_pipeline_steps` | `{pipelineName}` | 別パイプラインの全ステップをキュー末尾に展開 |
| JS→C++ | `get_rtf_position` | `{rect}` | RichEdit 配置要求 |
| JS→C++ | `rtf_content_changed` | `{rtf}` | RTF 内容変更通知 |
| JS→C++ | `export_node` | `{nodeId}` | ZIP エクスポート |
| C++→JS | `trigger_list_result` | `{triggers[]}` | 登録済みトリガー一覧（`trigger_list` への応答） |
| JS→C++ | `enable_trigger` | `{pipelineName, triggerType, enable}` | トリガーの有効/無効切替 |
| JS→C++ | `trigger_list` | `{}` | 登録済みトリガー一覧要求 |

## Editor — Per-mimetype Controls

全ペーン（Tree / List / Editor / Messages / Toolbar）は WebView2 内の HTML+JS で実装する。
C++ 側は Bridge 経由でデータを送受信し、Storage / PipelineRunner / AIProvider を担当する。

### text/plain

- WebView2 内の `<textarea>` または `contenteditable` で実装
- 編集中の内容は JS → C++ へ `save_node` メッセージで通知 → 自動保存

### text/html

- WebView2 内で `marked.js` を使って Markdown → HTML 変換してレンダリング
- **View/Source toggle**: ボタンで `<textarea>`（ソース編集）と `<div>`（プレビュー）を切り替え
- AI 出力受け取り時に `text/plain` → `text/html` 変換オプション（Config で on/off）
- WebView2 初期化完了前にノード選択 → JS 側で **"Loading..."** プレースホルダ表示、`init` メッセージ受信後に描画

### image/png | image/jpeg | image/webp

- WebView2 内の `<img>` タグで表示（C++ 側が base64 → data URI に変換して渡す）
- DnD で画像ファイルをドロップ → JS が C++ に通知 → C++ が blob 保存 → node 更新

### application/rtf（ハイブリッド）

RTF ノード選択時の動作:

1. JS → C++: `get_rtf_position { rect: {x,y,w,h} }`（エディタエリアの座標を要求）
2. C++ → RichEdit: `MoveWindow(hRichEdit, x, y, w, h, TRUE)` + `ShowWindow(SW_SHOW)`
3. C++ → WebView2: `rtf_position` メッセージで JS のエディタエリアを `visibility: hidden`（透過）
4. RTF ノードの content を RichEdit に `EM_STREAMIN` でロード

非 RTF ノード選択時:

1. RichEdit の内容を `EM_STREAMOUT` で取得 → C++ が base64 encode → `save_node` で保存
2. `ShowWindow(hRichEdit, SW_HIDE)`
3. JS のエディタエリアを再表示（`visibility: visible`）

ウィンドウリサイズ時: WebView2 が `get_rtf_position` を送り直す → C++ が `MoveWindow` で追従。

**フォーマットツールバー（B/I/U/S/Font/Size/Color/Align/Bullets）:**
HTML ツールバーとして WebView2 側に配置。各ボタンクリック → JS → Bridge → C++ → `EM_SETCHARFORMAT` / `EM_SETPARAFORMAT` で制御。

### Attachments section in Editor

```
Attachments:
┌─────────────────────────────────────┐
│ [img: diagram.png]        [✕][👁]  │
│ [img: photo.jpg]          [✕][👁]  │
│ [+ Add File...]                     │
└─────────────────────────────────────┘
```

- `[+ Add File...]` → JS → Bridge → C++: `open_file_dialog` → `GetOpenFileName` → ファイル読み込み → small files inline, large to `blobs/` → 結果を JS に返す
- `[✕]` → remove attachment
- `[👁]` → preview（<img> / <audio> / <video> で data URI 表示）

## Context Menu (Right-click on Tree/List)

HTML カスタムドロップダウンとして WebView2 内に実装（`contextmenu` イベントで OS メニューを抑制）。

```
Create Child
Create Sibling
──────────────
✂ Cut Node
📋 Copy Node
📋 Copy Content
📄 Paste
──────────────
🗑 Remove
✏ Rename
Update
──────────────
▲ Move Up
▼ Move Down
──────────────
Content Type  →  text/plain / text/html / application/rtf / image/png / image/jpeg / image/webp
▶ Run Pipeline →  [pipeline list from pipeline.json] / Custom...
──────────────
🔍 Search Node (Ctrl+F)
──────────────
📦 Export Node with Media  (ZIP)
▶ Collapse All
▶ Expand All
```

### Copy / Cut / Paste

クリップボード操作は Bridge 経由で C++ と JS が連携する。

| Operation | Action |
|-----------|--------|
| **Copy Content（text/plain, text/html）** | JS: `navigator.clipboard.writeText()` — WebView2 内で完結 |
| **Copy Node（JSON）** | JS → Bridge → C++: `SetClipboardData(CF_UNICODETEXT, json)` |
| **Copy Content（RTF）** | JS → Bridge → C++: `SetClipboardData(CF_RTF, rtf)` |
| **Cut** | Copy + Remove（Bridge 経由） |
| **Paste** | JS → Bridge → C++: `GetClipboardData` → 結果を JS に返す → JSON パース → 子ノード追加。Fallback: `{content: rawtext}` |
| **Export Node** | JS → Bridge → C++: `SaveFileDialog` → Node JSON + blobs → ZIP |

## Drag & Drop

| Context | Operation |
|---------|-----------|
| Tree DnD | Change parent |
| List DnD | Reorder within same level |
| Tree ↔ List | Inter-pane reorder/parent change |
| RTF Editor | Image drop → embed in RTF |
| Image Editor | Image file drop → update content |
| Attachments | File drop → add as attachment |

## Search (Ctrl+F)

ツールバーまたは `Ctrl+F` で検索バーを表示。全タブ・全ノードの `title` / `content` を対象に全文検索。

### Search Scope

| スコープ | 対象 |
|----------|------|
| **All tabs** | 全タブの全ノード（デフォルト） |
| **Current tab only** | 現在のタブのみ |
| **Selected node & children** | Tree で選択中のノード以下のサブツリーのみ |
| **Specific tabs...** | ユーザーが選んだタブのみ（サブダイアログで複数選択） |

- デフォルトスコープは Config で変更可能
- 結果は Messages ペーンに表示（専用結果リストは作らない）
- 結果行はクリック可能。クリックで該当タブに切替 + Tree/List で該当ノードを選択

### 検索クエリ Bridge フロー

```
JS:  postMessage({ type: "search", query: "hello", scope: "all_tabs" })
C++: 全 data/*.json をパース、title/content(base64decode) を grep
C++: PostWebMessageAsJson({ type: "search_results",
       results: [{ tabId, nodeId, title, excerpt }] })
JS:  Messages ペインに結果リスト描画。クリックで該当ノードにジャンプ
```

## Auto Save

- 1.5s debounce after last edit
- Tab switch: save current tab before switching
- Application exit: save current tab + `session.json`

## Config Dialog (⚙)

HTML モーダルパネル（CSS overlay）として WebView2 内に実装。

### General
- Tab list management (add/remove tabs; each tab = one `.json` file)
- Media inline size limit (default 1024 KB)
- Default search scope (default: All tabs)
- Multi-media output mode: `attachments` / `children` / `ask`
- Max attachments per node (default 5) — 超過分は警告なしで切り捨てる。上限は Config で変更可能
- Blob storage directory (read-only display)

### Provider Settings (stored in `providers.json`)
```
┌──────────────────────────────────┐
│  Provider: [OpenAI____________]  │
│  API Key:  [••••••••••••••] 👁  │
│  Base URL: [https://api.ope...]  │
│  [Test Connection]               │
│  [Save] [Cancel]                 │
└──────────────────────────────────┘
```

Supported providers: OpenAI, Anthropic, Gemini, Ollama

`command` / `tool` ステップにより任意の OS コマンド（sed, awk, ffmpeg, ImageMagick 等）をパイプラインに統合可能。外部プログラムの豊富さがそのまま Prompts のノード数となる（Unix 哲学: 小さなツールを組み合わせる）。

## AI Pipeline Feature

### Concept

```
Input Node → Step 1 (AI) → Step 2 (AI) → ... → Output Node
                                                   ├─ title: {pipeline_name}_{timestamp}
                                                   ├─ content: last step output
                                                   ├─ attachments: inherited (external blob refs)
                                                   └─ children: if multiMedia="children"
```

- Each step output becomes `{result}` for the next step
    - `{content}` / `{title}` also available as placeholders  
      `{attachments}` is NOT used as a text placeholder — media is passed via API multi-modal payload (`attachMedia` field)
    - **Step 1**: `{result}` is synonymous with `{content}` (input node content)
    - **Inter-step images**: if `Step N` produces `mediaOutputs`, those are inherited by `Step N+1` via `AIRequest.attachments` when `attachMedia: "all"`. `{result}` remains text-only.
    - **`{result.field}` syntax** (for `resultAs: "json"` only): dot-separated key path, e.g. `{result.data.text}` → `response["data"]["text"]`. Array access: `{result.items[0]}`. Unparseable paths return empty string.
- Media from pipeline → always stored as external files in `blobs/`
- Output node placed as child or sibling of input node (see `outputMode` below)

### Pipeline Mode（Basic / Expert）

| Mode | UI | 将来拡張 |
|------|----|----------|
| **Basic** | 直列 JSON ステップ一覧 + **mermaid.js** によるフロー図自動生成。実行中ノードをハイライト表示 | デフォルト |
| **Expert** | **Cytoscape.js** によるインタラクティブノードエディタ。分岐・並列実行・条件分岐に対応（将来実装） | 未実装 |

Basic モードの pipeline.json は `steps[]` の直列リスト。Expert モードでは `nodes[]` / `edges[]` の DAG 形式。  
両形式は相互変換可能 — Basic の直列リストは Expert の `nodes: [s1,s2,...], edges: [{from:s1,to:s2}, {from:s2,to:s3}, ...]` の線形特殊ケースとして表現できる。

```json
// Basic 形式（現行）
{ "steps": [ { "name": "A" }, { "name": "B" } ] }

// Expert 形式（将来）
{ "nodes": [{ "id": "A" }, { "id": "B" }],
  "edges": [{ "from": "A", "to": "B" }] }
```

pipeline.json には `"mode": "basic"`（デフォルト）または `"mode": "expert"` をパイプラインレベルで指定する。

#### Pipeline UI レイアウト（Basic モード）

```
┌────────────────────────────────────────────────────┐
│ Pipeline: Translate → Review    [Basic] [Expert]   │
├──────────────────────┬─────────────────────────────┤
│ Step List (編集)      │ Flow View (mermaid 自動生成) │
│                      │                             │
│ 1. [ai] Translate    │ [Input]→[Translate]         │
│    provider: openai  │     ↓                       │
│    model: gpt-4.1    │ [Review]→[Output]           │
│    [prompt: ...]     │                             │
│                      │ 実行中: [Translate] 点滅     │
│ 2. [ai] Review       │                             │
│    provider: anthropic│                            │
│    [prompt: ...]     │                             │
│                      │                             │
│ [+ Add Step]         │                             │
├──────────────────────┴─────────────────────────────┤
│ Input: "selected node"    [▶ Run] [⏸ Pause] [✕]    │
└────────────────────────────────────────────────────┘
```

### Dynamic Execution Queue

パイプライン実行中、未実行の待機ステップは C++ 側で `std::deque<Step>` として管理する。

| 操作 | タイミング | 動作 |
|------|-----------|------|
| **追加（Step）** | 実行中 | 待機キュー末尾に Step を追加（JS → Bridge → C++ `append_step`） |
| **追加（Pipeline）** | 実行中 | 別パイプラインの全 Step を現在のキュー末尾に展開（JS → Bridge → C++ `append_pipeline_steps`） |
| **削除** | 実行中 | 待機キューから指定 Step を削除 |
| **編集** | 実行中 | 待機キュー内の Step パラメータを変更（model, prompt, temperature 等） |
| **順序変更** | 実行中 | 待機キュー内の Step を移動 |
| **スキップ** | 実行中 | 現在実行中の Step 完了後、次をスキップしてその次の Step へ |

- 実行中の Step は変更不可（完了後にキューから除去される）
- キャンセル時はキューをクリア
- フロー図はメッセージ受信のたびに JS 側で再描画（mermaid.js）
- 「追加（Pipeline）」により、1 つのパイプライン実行中に別のパイプライン定義をキューにインジェクト可能（サブルーチン的利用）

### pipeline.json (`%APPDATA%/Ecode/Prompts/pipeline.json`)

```json
{
  "pipelines": [
    {
      "name": "Translate → Review",
      "retry": { "count": 3, "delayMs": 2000, "on": ["*"] },
      "test_input": "Hello, world!",
      "onError": { "action": "cancel" },
      "steps": [
        {
          "name": "Translate",
          "provider": "openai",
          "model": "gpt-4.1",
          "systemPrompt": "You are a translator.",
          "userPrompt": "Translate to Japanese:\n\n{content}",
          "temperature": 0.3,
          "maxTokens": 4096,
          "attachMedia": "all"
        },
        {
          "name": "Review",
          "provider": "anthropic",
          "model": "claude-sonnet-4-6",
          "systemPrompt": "You review translations.",
          "userPrompt": "Review:\n\n{result}",
          "temperature": 0.5,
          "maxTokens": 2048,
          "attachMedia": "all"
        }
      ],
      "outputMode": "child",
      "outputNaming": "{pipeline_name}_{timestamp}",
      "multiMedia": "attachments"
    }
  ]
}
```

### pipeline.json — Field Reference

#### outputMode

| 値 | 動作 |
|----|------|
| `"child"` | 出力ノードを入力ノードの**子**として追加。ツリーが深くなる（デフォルト） |
| `"sibling"` | 出力ノードを入力ノードの**兄弟**として追加（同じ親の末尾）。結果を横に並べる |
| `"replace"` | 入力ノードの `content` をパイプライン結果で上書き。添付ファイルも入れ替わる |

#### outputMode × multiMedia 組み合わせ

| outputMode | multiMedia | 動作 |
|-----------|-----------|------|
| `child` | `attachments` | 出力ノードを子に追加、メディアは attachments（デフォルト） |
| `child` | `children` | 出力ノードを子に追加、メディアはさらにその子ノード |
| `sibling` | `attachments` | 出力ノードを兄弟に追加、メディアは attachments |
| `sibling` | `children` | 出力ノードを兄弟に追加、メディアはその子ノード |
| `replace` | `attachments` | 入力ノードを上書き、attachments を入れ替え |
| `replace` | `children` | 入力ノードの content を上書き、既存の子を保持しメディア子を追加 |

#### attachMedia

| 値 | 動作 |
|----|------|
| `"all"` | 入力ノードの全 attachments を AI にマルチモーダル送信（デフォルト） |
| `"none"` | attachments を送信しない（テキストのみ） |
| `"first"` | `attachments[0]` のみ送信 |
| `"selected"` | パイプライン実行前にユーザーが選択（ダイアログ表示） |

### Pipeline Step Types

Each step in `pipeline.json` has a `type` field. Default: `"ai"`.

| Type | Description | block UI? |
|------|-------------|-----------|
| `"ai"` | AI API call (OpenAI / Anthropic / Gemini / Ollama) | No (streaming) |
| `"manual"` | User review/edit pause point | Yes (dialog) |
| `"command"` | Non-interactive CLI command execution | No |
| `"tool"` | Interactive external GUI tool launch | Yes (waits for tool exit) |
| `"fetch"` | HTTP GET/POST to arbitrary URL | No |
| `"condition"` | 前ステップ出力を評価して分岐・ループ制御 | No（自動） |
| `"history"` | 実行履歴から入出力を取得 → `{result}` に設定 | No |
| `"transform"` | JS/CSS セレクタ様式でテキスト変換（正規表現・JSON Path・テンプレート） | No |
| `"call_pipeline"` | 別パイプラインをサブルーチンとして呼び出し、結果を `{result}` に格納 | No |
| `"foreach"` | 配列入力を1件ずつループ処理。子ステップを各要素に対して実行 | Yes |
| `"parallel"` | 複数子ステップを並列実行、全完了後に出力を集約 | Yes |
| `"wait"` | 指定秒数待機、または条件が満たされるまで待機 | No |

#### `"manual"` — User review step

単一ステップの定義:

```json
{ "type": "manual", "mode": "edit", "prompt": "結果を確認し、編集してから Continue を押してください" }
```

| mode | 動作 |
|------|------|
| `"view"` | 読み取り専用表示、[Continue] / [Cancel] のみ |
| `"edit"` | 編集可能なテキストエリア、[Continue] で編集内容を `{result}` として次へ |
| `"select"` | 前ステップの出力を自動分割、または `choices[]` で明示的に定義した選択肢を UI 表示 → ユーザー選択 → `onSelect` に従い進行制御 |

`select` モードには3種類の選択肢定義方式がある:

| 方式 | 定義方法 | ユースケース |
|------|---------|-------------|
| **自動分割** | 前ステップ出力を `\n---\n` で分割 | AI が複数候補を列挙 → 選択 |
| **choices[] 明示** | ステップ定義内に `choices[]` 配列を記述 | 固定操作選択（OK/Re-generate/Cancel） |
| **動的 JSON** | 前ステップ出力を JSON パース → 配列を選択肢化 | `resultAs: "json"` で出力されたリストから動的生成 |

`choices[]` + `onSelect` の定義例:

```json
{
  "type": "manual",
  "mode": "select",
  "prompt": "生成された WAV を聴いて判定してください",
  "choices": [
    { "label": "✅ OK — 次のステップへ", "action": "next_step" },
    { "label": "🔄 Re-generate",         "action": "goto_step", "index": 0 },
    { "label": "✕ Cancel pipeline",      "action": "cancel" }
  ]
}
```

| action | 動作 |
|--------|------|
| `"next_step"` | 通常通り次ステップへ進行（デフォルト） |
| `"goto_step"` | 指定 `index` のステップに戻る（ループ実現） |
| `"skip"` | 指定 `count` だけステップをスキップ |
| `"cancel"` | パイプライン中断 |

`goto_step` により「WAV を聴いて気に入らなければ最初から再生成」のループが可能。実質的な条件分岐とループ制御を提供する。

#### `"command"` — CLI command step

```json
{
  "type": "command",
  "command": "python",
  "args": ["script.py", "{content_file}"],
  "timeout": 30,
  "workingDir": "%APPDATA%/Ecode/Prompts/",
  "resultAs": "text"    // "text" | "exitcode"
}
```

- `{content_file}` = content を base64 デコード後、mimetype に応じたエンコーディングで一時ファイルに書き出したパス（ステップ終了後に削除）

  | mimetype | エンコーディング |
  |----------|----------------|
  | `text/plain`, `text/html` | UTF-8 |
  | `application/rtf` | ASCII（\uN エスケープ済み RTF そのまま。RTF は 7-bit ASCII ベース） |
  | `image/*` | 生バイナリ（そのまま） |

- `resultAs: "text"` = stdout を `{result}` に格納
- `resultAs: "exitcode"` = 終了コードを文字列化

#### `"tool"` — Interactive external tool step

```json
{
  "type": "tool",
  "command": "mspaint.exe",
  "args": ["{content_file}"],
  "waitForExit": true,
  "resultAs": "file",         // "file" | "clipboard" | "exitcode" | "attachment"
  "resultFile": "{content_file}",
  "confirm": true             // 起動前に確認ダイアログを表示（デフォルト true）
}
```

| フェーズ | 動作 |
|----------|------|
| 起動 | `{content}` を一時ファイルに書き出し、`CreateProcess` で外部ツールを起動 |
| 待機 | `WaitForSingleObject(hProcess, INFINITE)` — ツール終了までパイプライン停止 |
| 結果取得 | `resultAs` に従い結果を収集 |
| クリーンアップ | 一時ファイル削除 |
| 継続 | 収集した結果を `{result}` として次ステップへ |

`resultAs`:

| 値 | 動作 |
|----|------|
| `"file"` | `resultFile` を読み込んで `{result}` に格納 |
| `"clipboard"` | クリップボードの内容を `{result}` に格納 |
| `"exitcode"` | 終了コードを文字列化して `{result}` に格納 |
| `"attachment"` | `resultFile` を `blobs/` にコピーして添付ファイル化 |

`waitForExit: false` 時はプロセス起動後に即座に次ステップへ進むため、`resultAs: "file"` / `"clipboard"` / `"attachment"` は意味をなさない（無効）。`"exitcode"` のみ例外的に許可（`WaitForSingleObject(hProcess, 0)` で終了していれば取得）。プロセスがまだ終了していない場合（`WAIT_TIMEOUT`）は `{result}` を空文字列とする。

#### `"fetch"` — Web fetch step

認証が必要な場合は `providers.json` のプロバイダ認証情報を参照する（pipeline.json に直接キーを書かない）。

```json
{
  "type": "fetch",
  "url": "{content}",
  "method": "GET",
  "auth": "openai",       // providers.json のプロバイダ名を参照。省略可
  "resultAs": "text"      // "text" | "attachment" | "json"
}
```

| resultAs | 動作 |
|----------|------|
| `"text"` | レスポンス本文を `{result}` に格納 |
| `"attachment"` | レスポンス本文を `blobs/` に保存し、添付ファイルとして参照 |
| `"json"` | レスポンスを JSON パース → ドット区切りキーで子フィールド参照可能（例: `{result.data.text}`）。配列は `{result.items[0]}`。ネスト非対応の場合は空文字を返す |

#### `"condition"` — Automatic branch / loop step

前ステップ出力を評価し、結果に応じて分岐・ループ制御を自動実行する。

```json
{
  "type": "condition",
  "expression": "{result}",
  "operator": "contains",
  "value": "error",
  "onTrue":  { "action": "goto_step", "index": 0 },
  "onFalse": { "action": "next_step" }
}
```

| operator | 評価内容 |
|----------|---------|
| `"contains"` | `{expression}` に `value` が含まれるか |
| `"equals"` | `{expression}` が `value` と完全一致するか |
| `"startsWith"` | `{expression}` が `value` で始まるか |
| `"regex"` | `{expression}` が正規表現 `value` にマッチするか |
| `"json_path"` | `{expression}` を JSON パース → `value` の JSON Path が存在するか |

`action` は `onSelect` と同じ: `"next_step"` / `"goto_step"` / `"skip"` / `"cancel"`。

これにより「AI が 'error' と出力したら最初からやり直す」のような自動ループが可能。

#### `"history"` — Execution history reference step

実行履歴 (`history/run_*.json`) から特定ステップの入出力を取り出し、`{result}` に設定する。

```json
{
  "type": "history",
  "runId": "run_20250530_153042",
  "stepIndex": 0,
  "field": "output"    // "input" | "output"
}
```

- `runId`: 履歴ファイル名（拡張子なし）
- `stepIndex`: 参照するステップ番号（省略時は最終ステップ）
- `field`: `"input"` または `"output"`
- 戻り値: 指定されたステップの入出力テキストが `{result}` に格納される

**プレースホルダー構文**: 全ステップの任意の入出力を以下の構文で参照可能:

| 構文 | 内容 |
|------|------|
| `{history[runId].steps[n].input}` | 指定 run の n 番目ステップの入力 |
| `{history[runId].steps[n].output}` | 指定 run の n 番目ステップの出力 |

### Step Variable Reference（拡張プレースホルダー）

`{content}` / `{result}` に加え、以下の構文で全ステップの入出力を参照可能:

| 構文 | 内容 | 使用可能ステップ |
|------|------|----------------|
| `{input}` | 入力ノードの content（`{content}` と同義） | 全ステップ |
| `{step.N.result}` | N 番目ステップの出力 | N が現在より前のステップ |
| `{step.Name.result}` | name が "Name" のステップの出力 | 同上（同名が複数ある場合は最初の1つ） |
| `{step.N.input}` | N 番目ステップの入力 | 同上 |
| `{step.Name.input}` | name が "Name" のステップの入力 | 同上 |

### New Step Type Definitions

#### `"transform"` — Text/JSON transformation step

```json
{
  "type": "transform",
  "engine": "regex",       // "regex" | "json_path" | "template" | "js"
  "expression": "s/foo/bar/g",
  "input": "{result}",     // 入力値（省略時は前ステップ出力）
  "output": "{result}"     // 格納先
}
```

| engine | expression 例 | 動作 |
|--------|--------------|------|
| `"regex"` | `s/foo/bar/g` | sed 形式の置換 |
| `"json_path"` | `$.data.items[0].text` | JSON Path で抽出 |
| `"template"` | `Result: {result}` | テンプレート文字列の展開 |
| `"js"` | `return input.toUpperCase()` | JavaScript 評価（実装依存） |

#### `"call_pipeline"` — Sub-routine pipeline call

```json
{
  "type": "call_pipeline",
  "pipelineName": "Summarize",
  "input": "{result}",        // 呼び出し先パイプラインの入力
  "inheritAttachments": true, // 添付ファイルを継承するか
  "resultAs": "{result}"      // 呼び出し先の最終出力をこのステップの出力に
}
```

呼び出し先パイプラインが完了するまで現在のパイプラインは待機（同期呼び出し）。
実行履歴には呼び出し元・先がそれぞれ別の `run_*.json` として記録され、`steps[].childRunId` で関連付けられる。

#### `"foreach"` — Batch loop step

```json
{
  "type": "foreach",
  "input": "{result}",         // 改行区切り or JSON 配列 → 1件ずつループ
  "itemVariable": "item",      // 各要素を {item} で参照可能
  "steps": [
    { "type": "ai", "userPrompt": "Process: {item}" }
  ],
  "concurrency": 1              // 並列数（1 = 逐次）
}
```

ループ内の各ステップは `{item}` で現在の要素を参照可能。
ループ全体の出力は各要素の結果を改行連結した文字列。

#### `"parallel"` — Parallel branch execution

```json
{
  "type": "parallel",
  "branches": [
    { "name": "summary", "steps": [ { "type": "ai", ... } ] },
    { "name": "keywords", "steps": [ { "type": "ai", ... } ] }
  ],
  "outputMode": "merge"        // "merge" | "first" | "branch.*"
}
```

全ブランチを並列実行。全完了後に結果を集約。
`{branch.summary}` で個別ブランチの出力を参照可能。

#### `"wait"` — Delay / conditional wait step

```json
{
  "type": "wait",
  "durationMs": 5000,          // 固定待機（ミリ秒）
  "until": "{result}",         // 条件式（省略時は durationMs のみ）
  "pollIntervalMs": 1000,      // 条件チェック間隔
  "timeoutMs": 60000           // 最大待機時間（超過時はエラー）
}
```

`until` が空文字以外の場合、`{result}` を式として評価し `"true"`（大文字小文字不区別）が返るまでポーリングする。
ファイルが存在するまで待機、API が特定の値を返すまで待機、等に使用。

### Per-Step Retry / On-Error Filter

パイプライン全体の `retry` に加え、ステップ単位でも `retry` を指定可能（ステップ単位の設定がパイプライン全体を上書き）。

```json
{
  "steps": [
    {
      "type": "ai",
      "retry": { "count": 5, "delayMs": 1000, "on": ["rate_limit", "timeout", "network"] }
    }
  ]
}
```

`on[]` フィルタ:

| フィルタ値 | 対象エラー |
|-----------|-----------|
| `"rate_limit"` | HTTP 429 |
| `"timeout"` | 60s タイムアウト |
| `"network"` | ネットワーク / DNS / 接続エラー |
| `"auth"` | HTTP 401（リトライ不可＝デフォルトでは対象外） |
| `"*"` | 全エラー（非推奨） |

省略時は全エラー種別をリトライ対象とする（現在の動作と同一）。

### Pipeline-Level On-Error

```json
{
  "name": "My Pipeline",
  "onError": { "action": "goto_step", "index": 0 },
  "steps": [ ... ]
}
```

`onError` の `action` は `onSelect` / `condition` と同じ: `"next_step"` / `"goto_step"` / `"skip"` / `"cancel"`。
全てのステップでエラーが発生した場合のデフォルト動作。ステップ個別の `retry` を使い切った後に発動する。

### Trigger Definitions

パイプラインを自動起動するトリガーを定義可能。

```json
{
  "name": "Auto Summarize",
  "triggers": [
    {
      "type": "schedule",
      "cron": "0 9 * * 1-5",     // 平日 9:00
      "inputNodeId": "node_abc"
    },
    {
      "type": "file_watcher",
      "path": "%APPDATA%/Ecode/Prompts/inbox/",
      "pattern": "*.txt",
      "action": "new_file"        // "new_file" | "modified"
    },
    {
      "type": "webhook",
      "port": 9876,
      "endpoint": "/trigger/summarize",
      "method": "POST",
      "auth": "provider_name"     // providers.json の認証情報を参照
    }
  ]
}
```

| Trigger | 起動条件 |
|---------|---------|
| `"schedule"` | CRON 式に従い定期実行 |
| `"file_watcher"` | 指定ディレクトリにファイルが作成/変更されたら実行。新規ファイルを `{content}` として入力 |
| `"webhook"` | HTTP サーバーを起動し、指定エンドポイントへのリクエストで実行 |

Bridge メッセージ:

| 方向 | type | payload | 説明 |
|------|------|---------|------|
| C++→JS | `trigger_fired` | `{pipelineName, triggerType}` | トリガー発火通知 |
| JS→C++ | `enable_trigger` | `{pipelineName, triggerType, enable}` | トリガーの有効/無効切替 |
| C++→JS | `trigger_list_result` | `{triggers[]}` | 登録済みトリガー一覧（`trigger_list` への応答） |
| JS→C++ | `trigger_list` | `{}` | 登録済みトリガー一覧要求 |

Webhook の依存: `ws2_32.lib`（ローカル TCP サーバとして実装）。

### Test Mode & Test Input

パイプライン実行前に `"test_input"` を指定すると、選択ノードを実際の入力とせずテスト用入力を使用する。

```json
{
  "name": "Translate → Review",
  "test_input": "Hello, world!",
  "steps": [ ... ]
}
```

ツールバーに `[🧪 Test Mode]` トグルボタンを追加。
- オン: パイプライン実行時、任意の選択ノードがなくても `test_input` を入力として使用
- オフ: 通常通り選択ノードを入力として使用（デフォルト）

Test Mode オン時の実行は履歴の `steps[].test = true` で識別可能。

### Execution History — 追加フィールド

`run_*.json` の各ステップに以下を追加:

```json
{
  "steps": [{
    "test": false,              // Test Mode での実行か
    "retries": 2,               //  このステップのリトライ回数
    "iterations": 0,            // foreach ループ内での合計反復数
    "childRunId": null,         // call_pipeline の呼び出し先 run ID
    "errorPipelineRunId": null  // onError で起動されたエラー処理パイプラインの run ID
  }]
}
```

### Template Variables

pipeline.json のパイプラインに `variables` フィールドを追加。実行時にダイアログで値入力。

```json
{
  "name": "Translate",
  "variables": [
    { "key": "target_language", "default": "Japanese" },
    { "key": "tone", "default": "formal" }
  ],
  "steps": [
    { "type": "ai", "userPrompt": "Translate to {target_language} in {tone} tone:\n\n{content}" }
  ]
}
```

### Custom Pipeline

コンテキストメニューの「Custom...」は pipeline.json に保存せず一時的にステップを組んで実行するインラインパイプラインエディタダイアログ。
- ステップリスト（追加/削除/並び替え可）
- 各ステップ: type + provider + model + prompt + temperature を編集可能
- 実行後「名前を付けて保存」ボタンで pipeline.json に追記可能

### Pipeline Runner Dialog (Streaming)

WebView2 モーダルパネル（CSS overlay）として実装。**mermaid.js フロー図**を上部に表示。

Basic モードでは直列の矢印 + 実行中ノードのハイライト。Dynamic Queue 操作用に Queue パネルと Output パネルを並置。

```
┌──────────────────────────────────────────────┐
│  [Input]→[Translate]→[Review]                │  ← mermaid フロー図
│             🟢実行中    ⏳待機                 │
├────────────────────────┬─────────────────────┤
│  Queue                 │  Output             │
│  ▶ Translate [locked]  │  こんにちは...      │
│  ⏳ Review   [✏][🗑]   │  [waiting...]       │
│  [+ Add Step]          │                     │
│  [+ Pipeline]          │                     │
├────────────────────────┴─────────────────────┤
│  [████████░░] Step 1/2  [▶][⏸][✕ Cancel]    │
└──────────────────────────────────────────────┘
```

実行中のノードは mermaid 図内でハイライト（CSS クラスで色変更）。
完了ノードはチェックマーク、エラーノードは赤表示。

### Execution History

パイプライン実行ごとにステップ別の入出力を記録し、後から参照・再利用できる。

#### 保存形式

```
%APPDATA%/Ecode/Prompts/history/run_YYYYMMDD_HHmmss.json
```

`run_*.json` の構造:

```json
{
  "id": "run_20250530_153042",
  "pipelineName": "Translate → Review",
  "inputNodeId": "node_abc",
  "startedAt": "2025-05-30T15:30:42Z",
  "status": "completed",
  "steps": [
    {
      "index": 0,
      "name": "Translate",
      "type": "ai",
      "input": "Hello world",
      "output": "こんにちは世界",
      "tokens": { "prompt": 120, "completion": 45 },
      "durationMs": 2300,
      "status": "completed"
    }
  ],
  "outputNodeId": "node_xyz"
}
```

#### History UI

Messages ペイン内または専用ペインで以下を表示:

- 実行一覧（日時 / パイプライン名 / ステータス / 所要時間）
- クリックで展開 → ステップ別の入出力を確認
- 「この実行から再実行」ボタン → 同じ入力 + パイプラインで再実行
- `history` ステップまたはプレースホルダー構文 `{history[runId].steps[n].output}` で入出力を再利用可能

#### 保持ポリシー

- 保持件数: Config で設定（デフォルト 100 件）
- 超過分は古い順に自動削除
- Blob GC との連携: history が参照している blob ファイルは削除しない（history 削除時に合わせて削除）

### AI Provider Interface

```cpp
struct AIRequest {
    std::string model;
    std::string systemPrompt;
    std::string userPrompt;
    double temperature;
    int maxTokens;
    std::vector<Attachment> attachments;  // multi-modal input
    std::map<std::string, std::string> extraParams;  // provider-specific params (e.g. topK, topP)
};

struct AIResponse {
    std::string content;
    std::string model;
    std::vector<Attachment> mediaOutputs; // generated images, etc.
    int promptTokens;
    int completionTokens;
};
```

### SSE Parsing

WinHTTP 非同期コールバックで受信したチャンクは SSE 行境界と一致しないため、内部バッファで行分割する。

```
チャンク受信ループ:
1. WinHttpQueryDataAvailable → WinHttpReadData でバイト列取得
2. 内部バッファに追記
3. "\n" で行分割し、完全な行のみ処理。不完全な末尾はバッファに残す
4. "data: " プレフィックスの行を JSON としてパース
5. "[DONE]" または終端で完了
```

プロバイダごとの SSE フォーマット差異:

| Provider | content の取得パス | 備考 |
|----------|--------------------|------|
| OpenAI | `choices[0].delta.content` | 標準 SSE、`data: ` 行のみ |
| Anthropic | `event: content_block_delta` → 直後の `data.delta.text` | `event:` 行と `data:` 行がペア。`event: content_block_delta` の場合のみ data をパース、他は無視 |
| Gemini | `candidates[0].content.parts[0].text` | 標準 SSE |
| Ollama | `message.content` | 標準 SSE |

### Error Handling

Pipeline 実行中に発生するエラーはすべて **Messages ペインに赤文字 + エラーコード** で表示される。

| エラー種別 | 動作 |
|-----------|------|
| **Network error** | `retry.count` 回リトライ（間隔 `retry.delayMs`）。尽きたら中断し Messages に表示 |
| **Rate limit (429)** | `Retry-After` ヘッダがあればその秒数待機、なければ 5s 待機してリトライ |
| **Auth failure (401)** | リトライせず即中断。Messages に "API key invalid" 表示 |
| **Model unavailable** | 中断、Messages に表示。Config でモデル一覧の再取得を促す |
| **Timeout** | 各 API コール 60s でタイムアウト → `retry` に従いリトライ |
| **Streaming disconnect** | ストリーミング API はステートレスのため途中再開不可。該当ステップを**先頭からリトライ**する（部分取得データは破棄） |

### Execution Model

#### Thread Model

WinHTTP 非同期モード。ワーカースレッド不要。
コールバックはシステムスレッドプールで実行。

```
Main Thread:             UI メッセージループ + Bridge メッセージ処理
WinHTTP Callback Thread: データ受信 → PostWebMessageAsJson（stream_chunk）
```

**安定化の3原則:**

1. **`hRequest` は PipelineRunner が所有。** キャンセル時のみ `WinHttpCloseHandle` を呼ぶ
2. **コールバック冒頭で `cancelled` フラグを確認。** 閉鎖後の遅延コールバックを無視する
3. **コールバック内では `PostWebMessageAsJson` のみ実行。** JSON パース等はメインスレッドの Bridge ハンドラで行う

**キャンセル:** `WinHttpCloseHandle(hRequest)` を呼ぶだけ。
コールバックが `WINHTTP_CALLBACK_STATUS_REQUEST_ERROR` で返る → クリーンアップ。
ハンドル競合なし。

#### Progress Log (Messages 第4ペイン)

パイプライン実行の進捗はすべて Messages ペインに逐次追記される。

| タイミング | 出力例 |
|-----------|--------|
| ステップ開始 | `[Pipeline] [1/3] Translating... (gpt-4.1)` |
| ステップ完了 | `[Pipeline] [1/3] Done (2.3s, 412 tokens)` |
| エラー | `[Pipeline] [1/3] ERROR: Rate limited, retrying (2/3)...` |
| キャンセル | `[Pipeline] Canceled by user` |
| パイプライン完了 | `[Pipeline] Completed → New node: "Translate_20250530_153042"` |

Messages ペインの各行は自動スクロール（最新行が常に表示範囲に入る）。

#### Progress Bar

**Pipeline Runner ダイアログ内**にプログレスバーを表示。
「Step N / 全ステップ数」をバーに反映。ステップ内はスピナー表示（不定）。
Messages ペインはログテキストのみ（バーは表示しない）。

#### Cancel Behavior

- キャンセル時点でのステップ結果は破棄（出力ノード作成しない）
- 生成済み blob ファイルは削除する
- Messages ペインに「キャンセルされました」を表示

### API Key Security

- `providers.json` stored in UserData (`%APPDATA%/`), NOT in source tree
- `providers.json` NEVER committed to git
- Pipeline definitions (`pipeline.json`) contain no keys — only references to provider names
- Config Dialog supports "Test Connection" before saving

## Dependencies

| Component | Library | 用途 |
|-----------|---------|------|
| WebView2 | `WebView2Loader.dll` (Microsoft.Web.WebView2) | **全体 UI ホスト**（Runtime must be installed） |
| RichEdit | `riched20.dll` (LoadLibrary) | RTF ノード編集のみ |
| GDI+ | `gdiplus.dll` / `GdiplusStartup` | 画像プレビューの補助（WebP → data URI 変換） |
| WIC (WebP) | `WindowsCodecs.dll` | optional、WebP デコード |
| marked.js | `frontend/lib/marked.min.js` | Markdown → HTML 変換（ソース同梱） |
| mark.js | `frontend/lib/mark.min.js` | 検索ハイライト（ソース同梱） |
| mermaid.js | `frontend/lib/mermaid.min.js` | フロー図自動生成（Basic モード、ソース同梱） |
| cytoscape.js | `frontend/lib/cytoscape.min.js` | インタラクティブノードエディタ（Expert モード将来、ソース同梱） |
| Base64 | Hand-written | |
| JSON parser | Hand-written recursive descent | |
| HTTP / HTTPS | `WinHTTP` (Windows built-in) | |
| WebSocket / Webhook | `ws2_32.lib` | Webhook リスナー用 |

## Build & Integration

- CMakeLists.txt: `add_executable(Prompts WIN32 ...)`
- Links: `comctl32 shlwapi user32 kernel32 winhttp ws2_32 WebView2Loader.lib riched20`（gdiplus は WebP 変換が必要な場合のみ、ws2_32 は Webhook トリガーを実装する場合のみ残す）
- Output: `Application/Prompts/bin/Release/Prompts.exe`
- Top-level CMake copies to `bin/Release/plugins/Prompts.exe`
- ecode `ScanPlugins()` auto-discovers → Plugins menu
- Supports `--embedded` flag

## Pipeline Manager

パイプラインの作成・編集・管理を行う専用 UI。Toolbar `[⚡ Pipelines]` ボタンで開く HTML モーダルパネル。

### 概要

設計書の Basic/Expert 2モードを包含するコンテナとして実装する。

```
[⚡ Pipelines] ボタン → Pipeline Manager を開く

Pipeline Manager
├── 左列: パイプライン一覧 + [+ New]
└── 右列（選択中パイプラインの編集）
    ├── [Basic] タブ  ← ステップリスト + mermaid プレビュー（現実装ターゲット）
    └── [Expert] タブ ← Cytoscape.js ノードエディタ（将来実装）
```

### レイアウト（Basic タブ）

```
┌────────────────────────────────────────────────────────┐
│ Pipelines                                  [+ New]     │
├──────────────────┬─────────────────────────────────────┤
│ Translate→Review │  Name: [Translate → Review        ] │
│ Summarize        │                          [Basic][Expert]│
│ DailyReport 🕐   │                                     │
│                  │  Steps:                             │
│                  │  ┌─────────────────────────────┐   │
│                  │  │ 1. [ai] Translate  [✏][🗑][↕]│   │
│                  │  │ 2. [ai] Review    [✏][🗑][↕]│   │
│                  │  │ [+ Add Step ▾]               │   │
│                  │  └─────────────────────────────┘   │
│                  │  Flow Preview (mermaid):            │
│                  │  [Input]→[Translate]→[Review]→[Out] │
│                  │  ← ステップ追加・削除でリアルタイム更新│
│                  │                                     │
│                  │  Triggers: [+ Add Trigger ▾]        │
│                  │  Output: [child ▾]  Retry: [3x 2s▾] │
│                  │                                     │
│                  │  [▶ Run Now]  [💾 Save]  [🗑 Delete] │
└──────────────────┴─────────────────────────────────────┘
```

- 左列：パイプライン一覧。🕐 はスケジュールトリガー付きを示す
- Basic タブ：ステップフォームリスト + mermaid リアルタイムプレビュー
- Expert タブ：Cytoscape.js インタラクティブノードエディタ（将来実装）
- Basic → Expert 変換は自動（線形グラフ）。Expert → Basic は線形グラフのみ可能

### ステップ追加 UI（`[+ Add Step ▾]`）

```
┌──────────────────────────────────────────────────────┐
│  🤖 AI Call       — AI プロバイダへのプロンプト送信   │
│  📝 Manual Review — 人間によるレビュー・選択ポイント  │
│  ⚙️  CLI Command   — コマンド実行・スクリプト連携    │
│  🔧 External Tool — GUI アプリ起動・結果取得         │
│  🌐 HTTP Fetch    — Web API / サービス連携           │
│  🔀 Condition     — 自動分岐・ループ制御             │
│  🔄 Transform     — 正規表現・JSON Path・整形         │
│  📦 Call Pipeline — 別パイプラインをサブルーチン呼び出し│
│  🔁 Foreach       — 配列を1件ずつループ処理          │
│  ⚡ Parallel      — 複数ステップを並列実行           │
│  ⏱️  Wait          — 時間待機・条件待ち              │
│  📜 History       — 実行履歴から入出力を再利用       │
└──────────────────────────────────────────────────────┘
```

### ステップ編集フォーム（`[✏]` クリック時）

```
┌─────────────────────────────────────────────────────┐
│ ✏ Step: Translate                                   │
│ Type: [ai ▾]                                       │
│ Provider: [openai ▾]  Model: [gpt-4.1 ▾]           │
│ System Prompt: [You are a professional translator.] │
│ User Prompt:   [Translate to Japanese:\n{input}   ] │
│ 💡 使用可能変数: {input} {result} {step.N.result}   │
│ Temperature: [0.3]    Max Tokens: [4096]            │
│ Retry: count [3]  delay [2000] ms                  │
│        on: [rate_limit ✕] [timeout ✕] [+]          │
│ [✓ Save]  [✕ Cancel]                               │
└─────────────────────────────────────────────────────┘
```

step type ごとにフォームのフィールドが切り替わる。

| Type | 表示フィールド |
|------|--------------|
| `ai` | Provider / Model / System Prompt / User Prompt / Temperature / MaxTokens / attachMedia |
| `command` | Command / Args / WorkingDir / Timeout / resultAs |
| `tool` | Command / Args / waitForExit / resultAs / resultFile / confirm |
| `fetch` | URL / Method / Headers / Body / auth / resultAs |
| `condition` | expression / operator / value / onTrue / onFalse |
| `transform` | engine / expression / input |
| `manual` | mode / prompt / choices[] |
| `call_pipeline` | pipelineName / input / inheritAttachments |
| `foreach` | input / itemVariable / steps / concurrency |
| `parallel` | branches[] / outputMode |
| `wait` | durationMs / until / pollIntervalMs / timeoutMs |
| `history` | runId / stepIndex / field |

### Trigger 追加 UI（`[+ Add Trigger ▾]`）

```
┌────────────────────────────────────────┐
│  🕐 Schedule  — CRON 式で定期実行      │
│  📂 File Watch — ファイル変更で起動     │
│  🌐 Webhook   — HTTP リクエストで起動  │
└────────────────────────────────────────┘
```

### Toolbar 変更

```
[📄 New] [📂 Open] [💾 Save] [💾 Save As] │ [⚡ Pipelines] [▶ Run Pipeline] ⚙ Config
```

### Context Menu 追加

```
▶ Run Pipeline  →  [pipeline list] / Custom...
✏ Edit Pipeline →  [pipeline list]     ← 追加
+ New Pipeline                          ← 追加
```

### Bridge メッセージ追加

| 方向 | type | payload | 説明 |
|------|------|---------|------|
| JS→C++ | `save_pipeline` | `{pipeline}` | パイプライン定義を保存 |
| JS→C++ | `delete_pipeline` | `{pipelineName}` | パイプライン定義を削除 |
| C++→JS | `pipeline_list` | `{pipelines[]}` | パイプライン一覧（起動時 + 保存後） |

---

## File Operations

### Recent Files

最近開いたファイルを `recent_files.json` に保存し、起動時に `init` メッセージで JS へ送信する。

```
%APPDATA%/Ecode/Prompts/recent_files.json
{ "files": ["C:\\path\\to\\a.json", "C:\\path\\to\\b.json", ...] }
```

- 最大保持件数: 10（`MAX_RECENT_FILES`）
- 追加時: 先頭に挿入、重複削除、超過分を末尾から削除
- `init` payload の `recentFiles[]` フィールドに含めて JS へ通知

### init メッセージの完全 payload

`Bridge::SendInit` は廃止し、`App::SendFullInit` に置き換える。

```json
{
  "type": "init",
  "payload": {
    "language": "ja",
    "tabs": [{"name": "General", "file": "general.json"}],
    "nodes": {
      "general.json": { ...Node tree... }
    },
    "pipelines": [...],
    "recentFiles": ["C:\\path\\to\\a.json", ...]
  }
}
```

`SendFullInit` は `init_complete` メッセージ受信後に呼ぶ（WebView2 初期化完了後）。

### File Dialog Bridge フロー

#### Open Tab（`[📂 Open]`）

```
JS: postMessage({type: "open_tab"})
C++: GetOpenFileNameW(filter="JSON Files|*.json|All|*.*")
  → キャンセルなら return
  → storage_.AddToRecentFiles(path)
  → session に新 tab 追加、SaveSession()
  → SendFullInit() で全状態を再送信
```

#### Save As（`[💾 Save As]`）

```
JS: postMessage({type: "save_tab_as", payload: {tabId: "..."}})
C++: GetSaveFileNameW(defaultExt="json")
  → storage_.SaveTabData(newPath, node)
  → storage_.AddToRecentFiles(newPath)
  → session の tab.file を更新、SaveSession()
  → bridge_.PostToJS("tab_saved_as", {newPath})
```

#### New Tab（`[📄 New]`）

```
JS: postMessage({type: "new_tab", payload: {name: "Untitled"}})
C++: 新規 Node を生成（空のルートノード）
  → storage_.SaveTabData("untitled_TIMESTAMP.json", emptyNode)
  → session に追加、SaveSession()
  → SendFullInit()
```

### save_node ハンドラ

```
JS: postMessage({type: "save_node", payload: {tabId: "general.json", node: {...}}})
C++: JsonToNode(payload.node) でデシリアライズ
  → storage_.SaveTabData(tabId, node)
  → bridge_.PostToJS("node_saved", {tabId})
```

### Storage 実装状況

| メソッド | 状態 |
|---------|------|
| `LoadSession` / `SaveSession` | ✅ 実装済み |
| `LoadTabData` / `SaveTabData` | ✅ 実装済み |
| `LoadBlob` / `SaveBlob` / `RemoveBlob` / `GarbageCollectBlobs` | ✅ 実装済み |
| `LoadProviders` / `SaveProviders` | ✅ 実装済み |
| `LoadPipelines` | ✅ 実装済み |
| `SavePipelines` | ❌ TODO スタブ → 要実装 |
| `SaveHistory` / `ListHistory` | ❌ TODO スタブ → 要実装 |
| `LoadRecentFiles` / `AddToRecentFiles` | ❌ 未実装 → 要追加 |

---

## Implementation TODO

### Phase 0: プロジェクト骨格
- [ ] CMakeLists.txt（WIN32 exe、WebView2Loader.lib リンク）
- [ ] `main.cpp` — WinMain、メッセージループ
- [ ] `App.h/cpp` — ウィンドウ作成、WebView2 初期化、RichEdit HWND
- [ ] `Bridge.h/cpp` — PostWebMessageAsJson / postMessage ラッパー
- [ ] `JsonParser.h/cpp` — 手書き再帰下降 JSON パーサ
- [ ] `Base64.h/cpp` — エンコード/デコード
- [ ] `frontend/index.html` / `app.js` / `style.css` — 骨格
- [ ] `SetVirtualHostNameToFolderMapping` で `https://prompts.app/` をマップ

### Phase 1: Storage & File Operations
- [ ] `Storage::LoadRecentFiles` / `AddToRecentFiles`
- [ ] `Storage::SavePipelines`（TODO 解消）
- [ ] `Storage::SaveHistory` / `ListHistory`（TODO 解消）
- [ ] `App::SendFullInit`（tabs + nodes + pipelines + recentFiles）
- [ ] `App::HandleBridgeMessage` — `save_node` / `new_tab` / `open_tab` / `save_tab_as` / `close_tab` / `rename_tab`
- [ ] `App::OpenFileDialogW` / `SaveFileDialogW`
- [ ] Blob GC（起動時スキャン）

### Phase 2: Tree UI & Editor
- [ ] JS: Tree ペイン（展開・折りたたみ・選択・DnD）
- [ ] JS: List ペイン（子一覧・ブレッドクラム）
- [ ] JS: Editor ペイン（text/plain / text/html / image/*）
- [ ] JS: Attachments セクション（追加・削除・プレビュー）
- [ ] RTF ハイブリッド（RichEdit HWND overlay）
- [ ] Auto Save（1.5s debounce）
- [ ] Context Menu（HTML カスタムドロップダウン）
- [ ] Copy / Cut / Paste（Bridge + clipboard API）
- [ ] DnD（Tree/List 並び替え + ファイルドロップ）
- [ ] Ctrl+F 全文検索

### Phase 3: AI Pipeline — Basic
- [ ] `AIProvider.h/cpp` — OpenAI / Anthropic / Gemini / Ollama
- [ ] `PipelineRunner.h/cpp` — WinHTTP 非同期、SSE パーシング
- [ ] step type: `ai` / `manual` / `command` / `tool` / `fetch`
- [ ] step type: `condition` / `transform` / `history` / `call_pipeline` / `foreach` / `wait` / `parallel`
- [ ] Variable 参照展開（`{input}` / `{step.N.result}` / `{step.Name.result}`）
- [ ] Per-step retry（`on: []` フィルタ付き）
- [ ] Pipeline-level onError
- [ ] Dynamic Queue（`std::deque<Step>`）
- [ ] Pipeline Runner Dialog（mermaid フロー図 + Queue + Output）
- [ ] Execution History（`history/run_*.json` 書き込み・読み込み）

### Phase 4: Pipeline Manager UI
- [ ] Pipeline Manager パネル（HTML モーダル）
- [ ] Basic タブ: ステップリスト + mermaid プレビュー（リアルタイム）
- [ ] ステップ追加ドロップダウン（全 step type + 説明）
- [ ] ステップ編集フォーム（type 別フィールド切り替え）
- [ ] Trigger 追加 UI（schedule / file_watcher / webhook）
- [ ] `save_pipeline` / `delete_pipeline` Bridge 実装

### Phase 5: Triggers
- [ ] Schedule Trigger（SetTimer + cron パーサ）
- [ ] File Watcher Trigger（ReadDirectoryChangesW）
- [ ] Webhook Trigger（WinSock2 HTTP サーバ）

### Phase 6: Config & Polish
- [ ] Config Dialog（HTML モーダル）
- [ ] Test Mode（test_input トグル）
- [ ] Export Node（ZIP）
- [ ] --embedded モード
- [ ] Localization（PromptsLocalization + frontend/lang/*.json）
- [ ] Expert モード — Cytoscape.js ノードエディタ（将来）

---

## Test Specification

### Unit Tests（C++）

| テスト対象 | テストケース |
|-----------|------------|
| `JsonParser` | ネスト JSON・Unicode エスケープ・空オブジェクト・不正 JSON |
| `Base64` | encode/decode ラウンドトリップ・バイナリ・空文字列 |
| `Storage::LoadTabData` / `SaveTabData` | children 再帰・attachments inline/external・ラウンドトリップ |
| `Storage::LoadRecentFiles` / `AddToRecentFiles` | 重複除去・MAX_RECENT_FILES 超過・空リスト |
| `Blob GC` | 参照あり保持・参照なし削除・history 参照ファイル保持 |
| Variable 展開 | `{input}` `{step.0.result}` `{step.Translate.result}` `{history[id].steps[0].output}` |
| `condition` 評価 | contains / equals / startsWith / regex / json_path |
| Retry ロジック | `on[]` フィルタ一致時のみリトライ・count 超過で中断 |
| cron パーサ | `0 9 * * 1-5` などのパターンと時刻マッチング |
| SSE パーサ | 複数チャンク分割・不完全行バッファ残留・`[DONE]` 検出 |

### Integration Tests（手動確認）

| シナリオ | 確認内容 |
|---------|---------|
| アプリ起動 | session.json から全タブ復元、Tree/List/Editor が表示される |
| New Tab | 新規タブが作成され session.json に保存される |
| Open Tab | GetOpenFileName → ファイル読み込み → recentFiles に追加 |
| Save As | GetSaveFileName → 別パスに保存 → タブ名更新 |
| Recent Files | 再起動後に前回開いたファイルが一覧に表示される |
| ノード編集・保存 | text/plain / text/html / RTF / 画像でラウンドトリップ |
| パイプライン実行 | ストリーム出力が Messages に表示、完了後に子ノード作成 |
| Dynamic Queue | 実行中にステップ追加・削除・編集が反映される |
| キャンセル | 即座に停止、blob 削除、ハンドル競合なし |
| Execution History | run_*.json が作成され、History UI で参照・再実行できる |
| File Watcher Trigger | 監視フォルダへのファイル追加でパイプライン自動実行 |
| Webhook Trigger | `curl -X POST http://localhost:PORT/...` でパイプライン起動 |
| RTF overlay | RTF ノード選択で RichEdit 表示、リサイズ追従 |
| --embedded | ecode から SetParent してサイズ追随・多重起動不可 |
| Localization | Config で言語切り替え後に UI 文字列が切り替わる |

### Edge Cases

| ケース | 期待動作 |
|--------|---------|
| pipeline.json が存在しない | エラーなし、空リストで起動 |
| providers.json が存在しない | 実行時に "API key not configured" |
| blob ファイル欠損 | `[missing]` 表示、クラッシュしない |
| WebView2 未インストール | 起動時にフォールバック UI 表示 |
| 並列ステップ中にキャンセル | 全スレッド停止、リソースリークなし |
| foreach の入力が空 | ループ0回、`{result}` は空文字列 |
| goto_step で無限ループ | 最大繰り返し回数（デフォルト 100）でキャンセル |
| recent_files.json が存在しない | エラーなし、空リストで起動 |
