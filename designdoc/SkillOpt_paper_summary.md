# SkillOpt 論文要旨

**原題:** SkillOpt: Executive Strategy for Self-Evolving Agent Skills  
**著者:** Microsoft / 上海交通大学 / 同済大学 / 復旦大学 (2025年5月)  
**arXiv:** 2605.23904

---

## 核心的アイデア

「スキル（SKILL.md のような指示文書）を、モデルの重みではなく**外部状態**として訓練する」。

ディープラーニングの最適化ループ（forward → loss → backward → update → validate）を、テキスト空間のスキル文書に適用する。

```
パラメータ      → スキル文書
勾配            → ロールアウトトレースから導かれた編集方向
学習率          → 編集バジェット（1回に変更できる箇所の上限数）
検証チェック    → held-out バリデーションゲート
安定化手法      → バッチ/ミニバッチ/スケジュール/ゲート
```

---

## アーキテクチャ（SkillOpt パイプライン）

### 3つのデータ分割
- **Train split**: ロールアウト証拠を生成
- **Selection split**: 編集候補をゲーティング（strict greater-than のみ accept）
- **Test split**: 最終報告のみ（訓練中は触らない）

### Forward Pass（ロールアウト証拠）
- フリーズされたターゲットモデルが現在のスキルを使ってタスクを実行
- ハーネスがメタデータ・ツール呼び出し・出力・スコアを記録

### Backward Pass（ミニバッチリフレクション）
- **別の Optimizer モデル**（フロンティアLLM）がトレースを分析
- 失敗群・成功群に分けてリフレクションミニバッチを生成
- 構造化された **add / delete / replace** 編集を提案
- 編集バジェット（lr=4〜8）で変更量を制限

### バリデーションゲート
- 候補スキルを Selection split で評価
- **strictly improves** した場合のみ accept → `best_skill.md` に書き出し
- reject された編集は **rejected-edit buffer** に蓄積され、次のリフレクションで負のフィードバックとして活用

### Epoch-wise Slow/Meta Update
- 前後のエポックを比較：improvements / regressions / persistent failures / stable successes
- Optimizer が **protected slow-update field**（markup-fenced region）に長期知識を書き込む
- この field は step-level 編集では上書きできない（分離設計）
- Meta skill は Optimizer 側のみ（デプロイ成果物には含まれない）

---

## 重要な設計原則

| 原則 | 実装 |
|------|------|
| 編集を bounded に保つ | 学習率バジェット（lr=4〜8 edits/step） |
| 悪い更新を積み上げない | Validation gate（strictly greater-than） |
| 負のフィードバック | Rejected-edit buffer（エポック内） |
| 長期知識の分離 | Slow-update protected field |
| デプロイコスト0 | Optimizer は訓練時のみ。デプロイは best_skill.md のみ |

---

## 成果（ベンチマーク）

- **52/52** の (モデル × ベンチマーク × ハーネス) セルで最良または同率最良
- GPT-5.5 直接チャット: ノースキル比 **+23.5pt** 平均
- GPT-5.5 Claude Code ハーネス: **+19.1pt**
- 手書きスキルより平均 **+5.4pt** 上回る

### 転移実験
- **Cross-model**: GPT-5.4 で最適化したスキルが mini / nano でもプラス
- **Cross-harness**: Codex で訓練したスキルを Claude Code に転移で +59.7pt（SpreadsheetBench）
- **Cross-benchmark**: OlympiadBench → Omni-MATH でも正の転移

---

## 学習されるスキルの特性

- **コンパクト**: 最終スキルは 379〜1,995 tokens（中央値 920 tokens）
- **手続き的**: インスタンス固有の例ではなく、再利用可能なルール
- **編集数が少ない**: 1〜4回の accepted edit で大幅な改善
- **検査可能**: 人間が読んで理解できるテキスト

代表的な学習ルール例：
- ALFWorld: 「訪問済み/フロンティア台帳を管理し、繰り返し失敗後は探索を多様化せよ」
- SpreadsheetBench: 「Excelの再計算に頼らず、評価済み静的値を完全な対象範囲に書き込め」

---

## Prompts 機能強化への示唆

この論文の骨子を Ecode の **Prompts** 機能に組み込む場合の着眼点：

### 1. スキル文書 = Prompt テンプレート
- Prompts に保存されている各プロンプトを「スキル文書」として扱う
- 実行ログ（成功・失敗）をフィードバックソースとする

### 2. ループ構造の対応
- **Forward**: ユーザーがプロンプトを実行 → 結果を記録
- **Backward**: AI が成功/失敗パターンを分析 → add/delete/replace 編集を提案
- **Gate**: ユーザーがレビュー・承認する（または自動 A/B test）

### 3. Bounded Edit
- 一度に変更できる箇所を制限（"learning rate" に相当）
- 無制限の書き換えではなく差分ベースの改善

### 4. 実行履歴の活用
- 既存の「実行履歴 UI」をフィードバック収集インフラとして活用
- 失敗パターン・成功パターンをタグ付けして蓄積

### 5. Protected Fields
- プロンプトの「コアセクション」と「改善可能セクション」を分離
- ユーザーが固定したい部分は Optimizer が変更できないようにする

---

## 参照

- コード: https://aka.ms/SkillOpt
- ハーネスとして Claude Code が直接評価対象になっている（Table 1 の Claude Code 列）
