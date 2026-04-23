# ADR: `mod-conversion.txt` 廃止と `commonTable.txt` 移行方針

## 日付
2026-04-23

## 背景
- これまで拡張修飾、単打、修飾キー変換は主に `mod-conversion.txt` を中心に扱ってきた。
- 今後はすべてのキーを、通常入力キー、HoldShift キー、同時打鍵構成キーとして同等に扱う方向へ整理する。
- その中心定義ファイルとして `commonTable.txt` を導入する。
- 既存の `mod-conversion.txt` 関連処理は移行期間中の互換資産として残すが、新しい実行経路では利用しない。

## 決定
- 現行の `mod-conversion.txt` は廃止予定とし、定義は `commonTable.txt` へ移行する。
- `commonTable.txt` への移行が完了するまでは、`mod-conversion.txt` 関係の処理は削除しない。
- ただし、新しい `commonTable.txt` 実行経路では既存の `mod-conversion` 関係処理を流用しない。
- `commonTable.txt` の読み込み、解析、実行時判定、変換適用は新規に書き起こす。
- サンプルとして `[../userFiles/commonTable.txt](/F:/Dev/CSharp/AyaoriHIME/userFiles/commonTable.txt)` を用意し、このサンプルが意図通りに動作することを最初の受け入れ基準とする。
- サンプルが意図通りに動作するようになった後、既存の `mod-conversion.txt` から `commonTable.txt` へのコンバータを作成する。

## スコープ
- 対象はフロント側のキー入力解釈、単打定義、HoldShift 定義、同時打鍵前後の優先順位制御である。
- `kw-uni` デコーダ本体へ `commonTable.txt` の処理を委譲しない。
- `mod-conversion.txt` の既存 UI や既存パーサを新経路の基盤として使わない。

## `commonTable.txt` の初期ターゲット
- `#singleHit` ブロックで、単打時に実行する機能を定義できる。
- `#holdShift key PLANE both` 形式で、任意のキーを HoldShift キーとして扱える。
- HoldShift 対象キーには、文字キー、Space、Tab、BackSpace、Shift、Ctrl、Alt、Win など、deckey 化できるキーを指定できる。
- HoldShift ブロック内の矢印記法は、その HoldShift キーが押されている間の対象キー定義として扱う。
- 未定義の組み合わせは、本来の OS 入力または既存の通常入力へ戻す。

## 優先順位
- 同時打鍵定義が成立する場合は、同時打鍵を優先する。
- 同時打鍵が成立しない場合に、`commonTable.txt` の HoldShift 定義を評価する。
- `commonTable.txt` にも定義がない場合は、本来のキー入力として扱う。
- 単打は、対象キーが他キーとの組み合わせとして成立しなかった場合に評価する。

## 移行期間の扱い
- `mod-conversion.txt` の読み込み・保存・既存データ構造は、移行が完了するまでは残す。
- 新規機能追加や不具合修正の主対象は `commonTable.txt` 経路とする。
- `mod-conversion.txt` 経路を延命するための追加実装は原則行わない。
- コンバータ作成までは、既存 `mod-conversion.txt` の内容を自動移行しない。

## 実装方針
- `commonTable.txt` 用のモデル、パーサ、実行時辞書を新設する。
- 既存 `mod-conversion` の辞書や `ExtraModifiers` の内部表現に依存しない。
- まず `[../userFiles/commonTable.txt](/F:/Dev/CSharp/AyaoriHIME/userFiles/commonTable.txt)` の `#singleHit` と `#holdShift` が動く最小実装を目標にする。
- 実装後に、サンプル定義に含まれる `lctrl`、`rctrl`、`nfer` などの HoldShift 動作をログで確認する。
- サンプルの動作確認が完了してから、`mod-conversion.txt` から `commonTable.txt` への変換仕様を確定する。

## 受け入れ基準
- `[../userFiles/commonTable.txt](/F:/Dev/CSharp/AyaoriHIME/userFiles/commonTable.txt)` が読み込まれる。
- `#singleHit` の単打定義が、単独タップ時に実行される。
- `#holdShift` の定義が、対象キー Down 時点で適用される。
- HoldShift 定義がない組み合わせは、不要に捕捉されず本来の入力へ戻る。
- `mod-conversion.txt` を使わなくても、サンプルで定義された操作が成立する。
- サンプル確認後に、`mod-conversion.txt` から `commonTable.txt` へのコンバータ作成に進める。

## 保留事項
- `commonTable.txt` の厳密な文法エラー処理。
- `DlgModConversion` を `commonTable.txt` 編集 UI として再構成する時期。
- `mod-conversion.txt` 読み込みを完全停止する時期。
- コンバータの出力形式と、変換不能な定義の警告方式。
