# Prompts ツリーノード 色付けロジック

## 概要

ツリー内の各ノードは、現在選択中のノードとの**関係**に基づいて色付けされる。
モードによって適用される色クラスが異なる。

---

## ノードの種別

| 種別 | 定義 | 判定方法 |
|------|------|----------|
| **演算ノード** | プロンプト・レシピを持つノード。子を生成する | `node.children.length > 0`（ブランチ）|
| **データノード** | パイプライン実行結果として生成されたノード | `!node.children || node.children.length === 0`（リーフ）|

---

## CSS クラス一覧

| クラス | 色 | 意味 |
|--------|----|------|
| `selected` | `#094771`（青） | 選択中の**演算ノード** |
| `selected-data` | `#8a2a2a`（暗赤） | 選択中の**データノード** |
| `selected-input` | `#2a6b2a`（緑） | 選択ノードの**親**（入力元）|
| `selected-result` | `#8a5a1a`（橙） | 選択演算ノードの**直接の子**（出力結果）|
| `selected-linked` | `#3a3a3a`（暗灰） | （将来: リンク済みノード用）|
| `completed` | `#1a4f1a` / `#68d391`（緑） | 連結モードで**完了したパイプラインステップ** |

---

## 色付けルール（`buildTreeHTML` の実装）

実装: `Application/Prompts/frontend/app.js` — `buildTreeHTML(node, path)`

```
現在選択中パス = state.currentNodePath
選択ノードがリーフか = _selectedIsLeaf (renderTree で事前計算)

各ノードの色クラス決定:

1. path === currentNodePath (自分自身が選択中)
   → _selectedIsLeaf == true  → selected-data
   → _selectedIsLeaf == false → selected

2. currentNodePath の親パス === path (このノードが選択ノードの親)
   → selected-input

3. このノードの親パス === currentNodePath かつ _selectedIsLeaf == false
   (演算ノードが選択中のとき、その直接の子)
   → selected-result

4. 上記いずれにも該当しない
   → クラスなし（デフォルト）
```

### パス計算

```js
// path の親パスを求める
const parentOf = (p) =>
    p === '' ? null :
    (p.lastIndexOf('/') === 0 ? '' : p.substring(0, p.lastIndexOf('/')));
```

---

## モード別の表示

### 通常モード (`viewMode === 'node'`)

ツリーに `buildTreeHTML` の結果を表示。上記の色付けルールが適用される。

```
例: 演算ノード A が選択中
  [A]  selected (青)
    ├── [child1]  selected-result (橙)
    ├── [child2]  selected-result (橙)
    └── [child3]  selected-result (橙)

例: データノード child1 が選択中
  [A]  selected-input (緑)  ← A は child1 の親
    ├── [child1]  selected-data (暗赤)  ← 選択中
    ├── [child2]  (なし)
    └── [child3]  (なし)
```

### 閲覧モード（データノード選択時）

データノードをクリックしたとき:
- ツリー: データノードは `selected-data`、親演算ノードは `selected-input`
- **演算ペイン**: 親演算ノードのプロンプトを表示（データノードの内容で上書きしない）
- 出力ペイン: そのデータノードの内容を表示

### 連結モード (`viewMode === 'pipeline'`)

ツリー領域を `renderPipelineSteps` が上書き。パイプラインのステップ一覧を表示。

```
ステップの表示クラス:
  完了済み → tree-node.completed  (暗緑 #1a4f1a / 文字色 #68d391)
  選択中   → tree-node.selected   (青 #094771)
  未実行   → tree-node (デフォルト)
```

---

## 演算ペインのノード隔離ルール

実装: `renderPrompt()` の冒頭

```
選択ノードがリーフ（データノード）かつ path !== '' のとき:
  → 演算ペインは親ノードのプロンプトを表示する
  → データノードの content は演算ペインに反映しない
```

これにより、データノードの閲覧中も演算ノードの設定が破壊されない。

---

## 実装ファイル

| ファイル | 関数 | 内容 |
|----------|------|------|
| `Application/Prompts/frontend/app.js` | `renderTree()` | `_selectedIsLeaf` を事前計算 |
| `Application/Prompts/frontend/app.js` | `buildTreeHTML(node, path)` | 色クラスの決定ロジック |
| `Application/Prompts/frontend/app.js` | `renderPrompt()` | データノード選択時の親へのフォールバック |
| `Application/Prompts/frontend/app.js` | `renderPipelineSteps()` | 連結モードのステップ色付け |
| `Application/Prompts/frontend/style.css` | — | 各色クラスの CSS 定義 |
