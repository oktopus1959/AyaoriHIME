# ADR: PowerShell による `kw-uni.dll` 単体テスト手順

## 日付
2026-04-18

## 背景
- `kw-uni.dll` の挙動を、AyaoriHIME GUI を介さずに局所再現したい
- とくに `HandleDeckey()` の `editBufferData`、`OutputStack`、`Lattice/KBest` の相互作用を任意の入力列で確認したい
- MazeConversion のように、実行時ログの確認が必要なケースでは `INFO` レベルの `kw-uni.log` を確実に採取したい

## 決定
- `kw-uni.dll` の単体テストは 32bit の Windows PowerShell から P/Invoke で行う
- テスト用の入口として `[tools/maze_probe.ps1](/F:/Dev/CSharp/AyaoriHIME/src/tools/maze_probe.ps1)` を使う
- ビルドからスモークテストまで一続きで行う入口として `[tools/build_and_probe_kw_uni.ps1](/F:/Dev/CSharp/AyaoriHIME/src/tools/build_and_probe_kw_uni.ps1)` を使う
- `kw-uni.dll` の実ログ出力先は DLL 隣ではなく、実行カレント相対の `kw-uni.log` とする
- 本リポジトリの PowerShell テスト実行では、実ログ出力先と settings 抽出元をともに `[src/kw-uni.log](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni.log)` に統一する

## 実装上の前提
- `[kw-uni/Decoder.cpp](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni/Decoder.cpp:1133)` で `Reporting::Logger::SetLogFilename(_T("kw-uni.log"));` を呼んでいる
- そのため、`kw-uni` のログ出力先は `bin\Release\kw-uni.log` ではなく、実行カレント相対の `kw-uni.log` になる
- `[kw-uni/Decoder.cpp](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni/Decoder.cpp:424)` のコマンド名は `setLog` ではなく `setLogLevel`
- `INFO` レベルのログを有効にするには、`ExecCmdDecoder("setLogLevel\t5")` を送る
- キューに溜まった `INFO` / `INFOH` をファイルに書き出すには、終了前に `ExecCmdDecoder("saveTraceLog")` を送る必要がある
- `WARNH` は即時ファイル書き込みなので、`saveTraceLog` を呼ばなくても見えることがある

## 初期化と実行の流れ
- `CreateDecoder(LogLevel)`
- `InitializeDecoder()`
- `presendSettings` を複数チャンクで送信
- `initializeDecoder`
- `setLogLevel\t5`
- 必要な `HandleDeckeyDecoder()`
- `saveTraceLog`
- `FinalizeDecoder()`

## `maze_probe.ps1` の役割
- `[src/bin/Release/kw-uni.dll](/F:/Dev/CSharp/AyaoriHIME/src/bin/Release/kw-uni.dll)` を P/Invoke でロードする
- settings 抽出元として既定で `[src/kw-uni.log](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni.log)` を読む
- 抽出した settings 内の `logLevel=` を、引数 `-LogLevel` の値で上書きしてから `presendSettings` する
- `initializeDecoder` の直後に `setLogLevel\t{LogLevel}` を送る
- 実行後に `saveTraceLog` を呼び、`TRACE_LOG=` として `[src/kw-uni.log](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni.log)` を表示する

## 32bit PowerShell からの単体実行
- 64bit PowerShell では `kw-uni.dll` を読み込めないため、必ず 32bit Windows PowerShell を使う
- 実行例は以下

```powershell
& "C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" `
  -ExecutionPolicy Bypass `
  -File "F:\Dev\CSharp\AyaoriHIME\src\tools\maze_probe.ps1"
```

- settings 抽出元を明示したい場合は以下

```powershell
& "C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" `
  -ExecutionPolicy Bypass `
  -File "F:\Dev\CSharp\AyaoriHIME\src\tools\maze_probe.ps1" `
  -SettingsLogPath "F:\Dev\CSharp\AyaoriHIME\src\kw-uni.log"
```

## `kw-uni.dll` のビルド方針
- 出力 DLL は `[src/bin/Release/kw-uni.dll](/F:/Dev/CSharp/AyaoriHIME/src/bin/Release/kw-uni.dll)`
- `kw-uni.vcxproj` は `PlatformToolset=v143` なので、Visual Studio 2022 系の `MSBuild.exe` を使う
- `AyaoriHIME.sln` 上の指定は `x86`、個別 `.vcxproj` の指定は `Win32` を使う
- 実運用では solution 一括ビルドではなく、依存プロジェクトを順次ビルドする
  - `DyMazinLib.vcxproj`
  - `NgramCoreLib.vcxproj`
  - `kw-uni.vcxproj`

## `build_and_probe_kw_uni.ps1` の役割
- Visual Studio 2022 の `MSBuild.exe` を解決する
- `Path/PATH` 重複による `MSB6001` 回避のため、`MSBuild` 実行前にプロセス環境の `PATH` を正規化する
- ビルド前に次の古い出力物を削除する
  - `bin\Release\DyMazinLib.dll/.lib/.exp`
  - `bin\Release\kw-uni.dll/.lib/.exp`
  - `DyMazinLib.iobj/.ipdb`
  - `kw-uni.iobj/.ipdb`
- 削除はまず `Remove-Item -Force` を試し、失敗時は Git for Windows の `rm` / `bash` を試す
- その後、`maze_probe.ps1` を 32bit PowerShell で起動する
- settings 抽出元と trace 検証先は既定で `[src/kw-uni.log](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni.log)` を使う

## 自動実行入口
- 実行例は以下

```powershell
& "C:\Program Files\PowerShell\7\pwsh.exe" `
  -File "F:\Dev\CSharp\AyaoriHIME\src\tools\build_and_probe_kw_uni.ps1"
```

- 既定のスモークテスト条件は以下
  - deckey 列: `21,27,28,23`
  - `PrefixEdit`: `きみ`
  - `MazeDeckey`: `3039`
  - 期待する `out`: `莫迦なの`
  - 期待する `numBS`: `4`

## ログ確認の要点
- `bin\Release\kw-uni.log` を見ても `WARNH` しか出ていないことがある
- これは `kw-uni` の本体ログ出力先がそこではないためで、`INFO` / `INFOH` を確認する対象は `[src/kw-uni.log](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni.log)` である
- 実際に `maze_probe.ps1` 実行後、`[src/kw-uni.log](/F:/Dev/CSharp/AyaoriHIME/src/kw-uni.log)` には以下のような行が出る
  - `INFO  [DecoderImpl.setHelpOrCandidates(...)]`
  - `INFOH [DecoderImpl.HandleDeckey(...)]`
  - `INFO  [DecoderImpl.ExecCmd(...)] ENTER: paramLen=12, data=saveTraceLog`

## 既知の注意点
- `maze_probe.ps1` はビルドを行わない。単体で使う場合は先に `kw-uni.dll` を更新しておく
- `presendSettings` を省略すると、テーブルや辞書設定が不足して再現性が崩れる
- `saveTraceLog` を呼ばないと、`INFO` / `INFOH` はファイルへ反映されないことがある
- 実行中プロセスが `DyMazinLib.dll` や `kw-uni.dll` を保持していると、ビルド前削除や再リンクに失敗する
- `build_and_probe_kw_uni.ps1` はその検出と報告までは行うが、実行中プロセスの終了までは自動化しない
