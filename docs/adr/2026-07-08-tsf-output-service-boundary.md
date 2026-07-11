# ADR: TSF 出力サービス導入時の責務境界

## 日付
2026-07-08

## 状態
置換済み

このADRの「TSFを確定文字列出力sinkとして扱う」という決定は、
`2026-07-11-tsf-composition-lifecycle.md` により置き換えられた。

## 背景
- AyaoriHIME は現在、低レベルキーボードフックで入力を捕捉し、`AyaoriHIME` 側で `deckey` に変換した後、`kw-uni` の `HandleDeckeyDecoder()` で日本語文字列を生成している
- フロント側の編集バッファは `AyaoriHIME/Forms/FrmEditBuffer.cs` が保持し、`FrmKanchoku.CallHandleDeckeyDecoder()` は `frmEditBuf.GetPreText()` を `DecoderHandleDeckeyParams.editBufferData` として `kw-uni` に渡している
- `docs/spec/2026-01-output_stack_整合.md` と `docs/adr/2026-01-18-outputstack-editbuffer-sync.md` では、現行設計として EditBuffer を正とし、`kw-uni` 側の OutputStack/Lattice をこれに同期する方針を採っている
- 対象アプリへの出力は主に `SendInputHandler.SendStringViaClipboardIfNeeded()` に集約されており、`SendInput`、Backspace、クリップボード経由貼り付けを組み合わせている
- `SendInput`/クリップボード経由の出力はアプリ依存の揺れがあり、将来的には TSF の text service によって対象アプリのテキストコンテキストへ直接出力する構成を検討したい

## 決定
- 初期段階では、キーボード入力の捕捉、`deckey` 生成、デコード、候補選択、EditBuffer 管理は現行の `AyaoriHIME` / `kw-uni` 側に残す
- TSF はまず「対象アプリへの確定文字列出力を担当する出力 sink」として導入する
- `FrmEditBuffer` の機能全体を TSF text service へ移すことは初期段階の対象外とする
- TSF text service 本体は別 exe ではなく、TSF に登録される COM in-process DLL として扱う
- 必要に応じて補助プロセスを追加する余地は残すが、初期構成では既存の `AyaoriHIME.exe` を入力・デコード側の controller とする
- `SendInputHandler` へ直接依存している出力箇所を段階的に抽象化し、既存の `SendInput`/クリップボード出力をフォールバックとして残す
- EditBuffer は引き続きフロント側を正とし、`kw-uni` へ渡す `editBufferData` の意味を変えない

## 目標
- 対象アプリへの文字列確定出力を、可能な範囲で TSF の text context 経由にする
- `SendInput`/クリップボード出力に起因するアプリ依存の不安定さを減らす
- 現行の配列テーブル、複数ストローク、MultiStream、Ngram/K-Best、OutputStack/EditBuffer 同期の動作を維持する
- TSF 導入後も 1 ストロークごとの処理時間を悪化させないよう、TSF 側の処理を確定出力中心に限定する

## 非目標
- 初期段階で TSF text service 側にキー入力捕捉を移すこと
- 初期段階で `ITfKeyEventSink` を主入力経路にすること
- 初期段階で `FrmEditBuffer` の表示、候補表示、仮想鍵盤表示を TSF composition UI に移すこと
- `kw-uni` のデコード責務や OutputStack/Lattice の意味を変更すること
- Windows 以外への対応

## 設計方針
- `FrmEditBuffer` は、当面は「フロント側の編集バッファ状態」と「必要に応じた編集ウィンドウ表示」を担当し続ける
- 対象アプリへの出力だけを `ITextOutputSink` のような小さな境界で抽象化する
- 既存経路は `SendInputTextOutputSink` として残し、現在の `SendInputHandler.SendStringViaClipboardIfNeeded()` の挙動を維持する
- TSF 経路は `TsfTextOutputSink` として追加し、TSF text service と通信して確定文字列挿入と前方削除を要求する
- TSF 出力に失敗した場合、または対象コンテキストが取得できない場合は、既存の `SendInputTextOutputSink` にフォールバックする

## 想定コンポーネント
- `ITextOutputSink`
  - `SendText(char[] text, int textLength, int numBackSpaces, bool forceString)`
  - `SendVKeyCombo(uint modifier, uint vkey, int count)`
  - `CanUseForActiveContext`
- `SendInputTextOutputSink`
  - 既存の `SendInputHandler` を包む
  - 現行互換の既定実装とする
- `TsfTextOutputSink`
  - AyaoriHIME 本体側の出力 sink
  - TSF text service に対して、確定文字列、削除数、必要なメタ情報を送る
- `AyaoriHimeTsfTextService`
  - 新規の COM DLL として作成する TSF text service
  - 対象アプリのプロセス内に in-process でロードされる
  - アクティブな TSF context に対して edit session を発行し、選択位置前方の削除と文字列挿入を行う
  - AyaoriHIME 本体からの要求を受ける IPC/COM 境界を持つ

## 出力操作の対応
- `numBackSpaces > 0`
  - 既存経路では Backspace 送出
  - TSF 経路では selection 前方の range を `numBackSpaces` 文字分削除する
- `outString`
  - 既存経路では Unicode `SendInput` またはクリップボード貼り付け
  - TSF 経路では active context の selection 位置へ確定文字列として挿入する
- `FlushBuffer(true)`
  - 既存の EditBuffer 文字列確定を維持し、確定文字列の出力先だけを sink に委譲する
- `PutVkeyCombo()`
  - 初期段階では原則として既存 `SendInput` 経路を維持する
  - TSF で文字列出力できない制御キーは無理に TSF 化しない

## 通信境界
- AyaoriHIME 本体は従来通り単一の常駐プロセスとして入力とデコードを担当する
- TSF text service は別 exe ではなく、対象アプリのプロセス内にロードされる COM DLL として扱う
- そのため AyaoriHIME 本体とは別プロセス上の別インスタンスになり得る
- AyaoriHIME 本体は、現在のアクティブ TSF context を持つ text service インスタンスへ出力要求を送る
- 出力要求は少なくとも以下を含む
  - 確定文字列
  - 前方削除文字数
  - 要求時刻またはシーケンス番号
  - フォールバック可否
- text service 側がアクティブ context を持たない、edit session に入れない、または対象アプリが TSF 出力に応答しない場合は失敗として返す

## 移行手順
1. `SendInputHandler` 直呼び出し箇所を調査し、対象アプリへの文字列出力と仮想キー送出を分類する
2. `ITextOutputSink` を導入し、`FrmEditBuffer.FlushBuffer()` と `FrmEditBuffer.PutString()` の出力箇所を既存 sink 経由にする
3. 既定実装として `SendInputTextOutputSink` を接続し、既存挙動が変わらないことを確認する
4. TSF text service の最小実装を別プロジェクトとして追加し、確定文字列挿入のみを検証する
5. `numBackSpaces` に対応する前方削除を TSF edit session 上で実装する
6. AyaoriHIME 本体と TSF text service の通信経路を追加する
7. 設定で TSF 出力を opt-in にし、失敗時は既存 sink へフォールバックする
8. 対象アプリ別に TSF 出力可否を記録し、既定有効化の判断材料にする

## 影響範囲
- `AyaoriHIME/Forms/FrmEditBuffer.cs`
  - `FlushBuffer()` と `PutString()` の出力処理を sink 経由に変更する候補
- `AyaoriHIME/FrmKanchoku.cs`
  - `SendInputHandler.SendStringViaClipboardIfNeeded()` 直呼び出しの整理候補
  - `frmEditBuf.GetPreText()` を `kw-uni` に渡す既存仕様は維持する
- `AyaoriHIME/Handler/SendInputHandler.cs`
  - 既存出力実装として残す
  - 新しい sink から利用される側に寄せる
- `kw-uni/Decoder.cpp`
  - 初期段階では変更しない
- `kw-uni/StateCommonInfo.h`
  - 初期段階では変更しない
- 新規 TSF text service プロジェクト
  - COM 登録、TSF profile 登録、edit session、対象 context への出力を担当する

## 既知のリスク
- TSF text service は対象アプリのプロセス内にロードされるため、AyaoriHIME 本体との通信と寿命管理が複雑になる
- アクティブ context の特定、focus 遷移、対象アプリの TSF 対応状況により、常に TSF 出力できるとは限らない
- `numBackSpaces` を UTF-16 code unit 単位で扱うか、ユーザー認識文字単位で扱うかの整理が必要になる
- 既存の EditBuffer は `wchar_t` / UTF-16 前提だが、合成文字やサロゲートペアを含む場合の削除単位に注意が必要
- 制御キー、ショートカット、機能キーは TSF の文字列挿入とは別扱いになるため、`SendInput` 経路を完全には排除できない
- TSF 化によりアプリ別の互換性問題が `SendInput` 経路とは別の形で発生する可能性がある

## 検証方針
- まず `SendInputTextOutputSink` 導入のみでビルドし、既存挙動が変わらないことを確認する
- `tools/deckey_sequence_probe.ps1` は `kw-uni` 側の逐次生成確認として引き続き使用する
- TSF 経路は、メモ帳、Visual Studio、ブラウザ入力欄、ターミナル系アプリを対象に、確定文字列挿入と前方削除を分けて確認する
- フォールバック発生時は、対象ウィンドウクラス、失敗理由、出力文字列長、`numBackSpaces` をログに残す
- `UseEditWindow` の ON/OFF で、EditBuffer と OutputStack の同期結果が変わらないことを確認する

## 未決事項
- AyaoriHIME 本体と TSF text service の通信方式を COM local server、named pipe、window message のどれにするか
- TSF text service を C++ で実装するか、C# COM で実装するか
- TSF profile の登録、更新、削除を既存インストーラまたは設定画面にどう組み込むか
- TSF 出力を既定有効にする対象アプリの判定方法
- `numBackSpaces` の削除単位を UTF-16 code unit として固定するか、将来的に grapheme cluster 単位へ拡張するか

## 参照
- `docs/spec/2026-01-output_stack_整合.md`
- `docs/adr/2026-01-18-outputstack-editbuffer-sync.md`
- `AyaoriHIME/Forms/FrmEditBuffer.cs`
- `AyaoriHIME/FrmKanchoku.cs`
- `AyaoriHIME/Handler/SendInputHandler.cs`
- `kw-uni/Decoder.cpp`
