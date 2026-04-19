# Shift 通常キー化メモ

## 概要
- `Ctrl/Shift/Alt/Win` を通常キーと同じように扱えるようにしたい。
- 最初の目標は `Shift` を同時打鍵シフトキーとして扱うこと。
- 要件は、`Shift -> Space` でも `Space -> Shift` でも、同時に押されている時間帯があれば同時打鍵とみなすこと。
- 方針としては `LShift/RShift` を同列に通常キー候補として扱う前提で整理する。
- Shift 単独タップ時の既存機能は維持する前提。
- 同時打鍵不成立時は通常の Shift 修飾入力へ戻す前提。

## 直前に直した不具合
- `KeyModifiers.MOD_RCTRL` 以降が 16bit を超えたのに、`KeyComboRepository.CalcSerialValue()` が `mod & 0xffff` で切っていた。
- そのため `rctrl` 定義が修飾なしと衝突していた。
- 対策として `KeyComboRepository` の組み合わせキー表現を `uint` から `ulong` に拡張済み。
- 実装内容:
  - `KeyCombo.SerialValue` を `ulong` 化
  - `KeyCombo.CalcSerialValue(uint mod, int deckey)` を `((ulong)mod << 32) | (uint)deckey` に変更
  - `DecKeyFromKeyCombo` / `ModConvertedDecKeyFromKeyCombo` を `Dictionary<ulong, int>` 化
- これで `MOD_RCTRL/LALT/RALT/LWIN/RWIN/MOD_SINGLE` のビット欠落は防止済み。

## `KeyboardEventHandler.cs` 現状把握
- 現在は `RShift` だけが特別扱いされており、`LShift` は通常 Shift に近い扱い。
- 主な該当箇所:
  - `isEffectiveVkey()` で `RSHIFT` のみ特例
  - `shiftKeyPressed()` は「RSHIFT が押されている時は shift 扱いしない」ロジック
  - `ExModiferKeyInfoManager` は `rshiftKeyInfo` だけ持つ
  - `onKeyboardDownHandler()` / `onKeyboardUpHandler()` に `RSHIFT` 専用分岐あり
  - `isSameShiftKeyAsSandS()` は `LShift` を物理押下で特別視している
- 同時打鍵エンジン側 (`Determiner`, `StrokeList`, `KeyCombinationPool`) は、`KeyDown/KeyUp` と重なり時間ベースで判定しており、押下順非依存の余地はある。
- `KeyCombinationPool._GetEntry(IEnumerable<Stroke>)` で Shift と他キーの組み合わせ定義があるかを事前確認できる。

## 今回入れた未完成の実装
- `AyaoriHIME/Handler/KeyboardEventHandler.cs`
  - `ShiftKeyRuntimeState` を追加
  - `leftShiftState` / `rightShiftState` を追加
  - `activeNonShiftVkeys` を追加
  - `handleShiftDownAsNormalCandidate()`
  - `handlePendingShiftBeforeNormalKey()`
  - `replayPendingShiftFallback()`
  - `invokeSingleShiftTap()`
  - `effectiveShiftPressed()`
- `isEffectiveVkey()` を `LShift/RShift` 両方対象に変更
- `shiftKeyPressed()` は `effectiveShiftPressed()` を使うよう変更
- `keyboardDownHandler()` は `shift` を引数で受ける形に変更
- `AyaoriHIME/Handler/SendInputHandler.cs`
  - `SendSpecificShiftModifiedVKeys(uint shiftVkey, IEnumerable<uint> vkeys)` を追加
- `Shift` 後押しで、すでに押されている通常キーとの間に同時打鍵定義が無い場合は、
  Shift を保留せずシステム側へ流す分岐を追加
- `LShift` は、同時打鍵候補として保留した場合を除き、Down/Up ともシステムへ素通しする分岐を追加
- ただし、この状態は未完成・未検証。

## 現時点の問題点
- 実装は途中段階で、挙動保証できる状態ではない。
- 特に危ない点:
  - `RShift` 既存経路との二重管理
  - `modEx` / `holdShiftPlane` / SandS との競合
  - `keyboardUpHandler()` と `Determiner.KeyUp()` の責務分離が曖昧
  - Shift 不成立時の通常 Shift フォールバックが、現状は主に `Shift -> 通常キー` の順でしか成立していない
  - `Space -> Shift` で定義なしの場合は「Shift を通常修飾としてそのまま通す」挙動に寄せたが、他キーで期待通りかは未確認
- 次回はこの中途半端な状態を前提に継ぎ足すより、`KeyboardEventHandler` の Shift 経路を整理してから組み直したほうが安全。

## ビルド確認結果
- `dotnet build AyaoriHIME/AyaoriHIME.csproj -c Debug --no-restore` を実行。
- ただし今回のコード変更ではなく、環境側の `GenerateResource` 用 x86 タスクホスト問題で停止。
- 発生したエラー:
  - `MSB4216`
  - `MSB4028`
- 補助的に `dotnet msbuild AyaoriHIME/AyaoriHIME.csproj /t:CoreCompile /p:BuildProjectReferences=false /p:DesignTimeBuild=true /p:SkipCompilerExecution=false`
  も試したが、環境側の参照解決不全で `System` すら見えない大量エラーになり、今回変更の妥当性確認には使えなかった。
- したがって、今回変更の C# コンパイル成否は未確定。

## 次セッションでやるべきこと
1. `KeyboardEventHandler.cs` の Shift 処理を再整理する。
2. `RShift` の既存 `ExModiferKeyInfo` 経路を残すのか捨てるのかを一本化する。
3. Shift を通常キー候補として保留する状態機械を明確化する。
4. `Shift -> Space` / `Space -> Shift` の両順序で、同時打鍵定義あり・なしをログで追えるようにする。
5. 実機確認手順を決める。
6. 特に `LShift` の通常入力、`RShift` の既存単打/拡張シフト面、`SandS` の相互作用を優先して確認する。
