# ADR: PowerShell による `deckey` 列逐次観測手順

## 日付
2026-04-19

## 背景
- `kw-uni.dll` の `HandleDeckey()` に対して、`deckey` 列を順に与えたときの逐次的な生成文字列を確認したい
- 既存の `[tools/maze_probe.ps1](/F:/Dev/CSharp/AyaoriHIME/src/tools/maze_probe.ps1)` は MazeConversion の確認まで含むため、`deckey` 列だけを観測したい用途には役割が広い
- `OutputStack` と `numBackSpaces` の反映結果を、最終変換を挟まずにその場で確認できる入口が必要

## 決定
- `deckey` 列の逐次観測専用の入口として `[tools/deckey_sequence_probe.ps1](/F:/Dev/CSharp/AyaoriHIME/src/tools/deckey_sequence_probe.ps1)` を使う
- 本スクリプトは `MazeDeckey` を送らず、`DeckeySequence` のみを `HandleDeckeyDecoder()` に順次渡す
- 各入力ごとに `edit`、`out`、`numBS`、更新後 `buffer` を標準出力へ出す
- 実ログ出力先と settings 抽出元は既存運用に合わせて `[src/kw-uni.log](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni.log)` を既定値とする

## 実装上の前提
- `kw-uni.dll` の単体実行は 32bit Windows PowerShell から P/Invoke で行う
- settings は `[src/kw-uni.log](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni.log)` から抽出し、`presendSettings` を複数チャンクで送信する
- `initializeDecoder` の直後に `setLogLevel\t{LogLevel}` を送る
- `INFO` / `INFOH` をログへ反映したい場合は、終了前に `saveTraceLog` を送る

## 初期化と実行の流れ
- `CreateDecoder(LogLevel)`
- `InitializeDecoder()`
- `presendSettings` を複数チャンクで送信
- `initializeDecoder`
- `setLogLevel\t5`
- `HandleDeckeyDecoder()` を `DeckeySequence` の各要素に対して順次実行
- `saveTraceLog`
- `FinalizeDecoder()`

## `deckey_sequence_probe.ps1` の役割
- `[src/bin/Release/kw-uni.dll](/F:/Dev/CSharp/AyaoriHIME/src/bin/Release/kw-uni.dll)` を P/Invoke でロードする
- 引数 `-DeckeySequence` で受けた `deckey` 列を、先頭から順に `HandleDeckeyDecoder()` へ渡す
- 各ステップで `STEP key=... edit='...' out='...' numBS=... buffer='...'` を出力する
- 全ステップ終了後に `FINAL buffer='...'` を出力する
- 実行後に `saveTraceLog` を呼び、`TRACE_LOG=` として `[src/kw-uni.log](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni.log)` を表示する

## 引数
- `DeckeySequence`: 逐次投入する `deckey` 列
- `InitialEdit`: 初期の edit buffer 文字列。未指定時は空文字
- `LogLevel`: `setLogLevel` に渡すレベル
- `DllPath`: 既定では `[src/bin/Release/kw-uni.dll](/F:/Dev/CSharp/AyaoriHIME/src/bin/Release/kw-uni.dll)`
- `SettingsLogPath`: 既定では `[src/kw-uni.log](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni.log)`
- `SkipSaveTraceLog`: `saveTraceLog` を省略したい場合に使う

## 32bit PowerShell からの実行
- 64bit PowerShell では `kw-uni.dll` を読み込めないため、必ず 32bit Windows PowerShell を使う

```powershell
& "C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" `
  -ExecutionPolicy Bypass `
  -File "F:\Dev\CSharp\AyaoriHIME\src\tools\deckey_sequence_probe.ps1" `
  -DeckeySequence 21,27,28,23
```

- 初期 edit buffer を与えたい場合は以下

```powershell
& "C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" `
  -ExecutionPolicy Bypass `
  -File "F:\Dev\CSharp\AyaoriHIME\src\tools\deckey_sequence_probe.ps1" `
  -InitialEdit "きみ" `
  -DeckeySequence 21,27,28,23
```

## 出力例
- 既定の `DeckeySequence=21,27,28,23` では以下のように出力される

```text
STEP key=21 edit='' out='は' numBS=0 buffer='は'
STEP key=27 edit='は' out='ばか' numBS=1 buffer='ばか'
STEP key=28 edit='ばか' out='な' numBS=0 buffer='ばかな'
STEP key=23 edit='ばかな' out='の' numBS=0 buffer='ばかなの'
FINAL buffer='ばかなの'
TRACE_LOG=F:\Dev\CSharp\AyaoriHIME\src\kw-uni.log
```

## 既知の注意点
- 本スクリプトはビルドを行わない。単体で使う場合は先に `kw-uni.dll` を更新しておく
- `presendSettings` を省略すると、テーブルや辞書設定が不足して再現性が崩れる
- `saveTraceLog` を呼ばないと、`INFO` / `INFOH` はファイルへ反映されないことがある
- `InitialEdit` と逐次更新された `buffer` の合計長が `DecoderHandleDeckeyParams.editBufferData` の上限を超えるケースには注意する
