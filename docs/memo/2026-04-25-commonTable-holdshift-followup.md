# 2026-04-25 commonTable / HoldShift 作業メモ

## 今回までに入れた変更

### 1. `commonTable.txt` の runtime 経路
- `CommonTableParser` / `CommonTableRuntime` / `InputActionResolver` を使う新経路に寄せた
- `#singleHit` / `#holdShift` は `InputActionResolver` に登録される
- `mod-conversion.txt` の runtime 読込は停止した

### 2. `KeyComboRepository` 廃止の途中段階
- `KeyComboRepository` は削除済み
- reverse mapping は `DeckeyComboMap`
- forward lookup は `InputActionResolver`

関連ファイル:
- `AyaoriHIME/Domain/DeckeyComboMap.cs`
- `AyaoriHIME/Domain/InputActionResolver.cs`

### 3. `commonTable.txt` を拡張修飾キー設定ダイアログから編集
- `DlgModConversion` の表示元を `ExtraModifiers` から `CommonTableRuntime` へ切替
- 保存先は `Settings.CommonTableFile`
- 書き出しは `CommonTableRuntime.MakeCommonTableContents()`

関連ファイル:
- `AyaoriHIME/Gui/DlgModConversion.cs`
- `AyaoriHIME/Gui/DlgSettings.cs`
- `AyaoriHIME/Domain/CommonTable.cs`

### 4. HoldShift / SystemModifier 修正
- `nfer/xfer/caps/alnum` も `Settings.GetHoldShiftKeySetting(deckey)` があれば `GenericHoldShift` として扱うようにした
- `makeExModKeyShifted()` で、commonTable HoldShift がある special key も `PRESSED -> SHIFTED` へ遷移するようにした
- `#singleHit` pending は、押下中 HoldShift がある場合は作らないようにした
- `Ctrl/Shift/Alt/Win` の SystemModifier 判定を `isEffectiveVkey()` より前に移し、`RCtrl` などが HoldShift 経路に入るようにした
- 未定義の SystemModifier HoldShift 組合せは、singleHit を消費済みにしたうえで通常の `keyboardDownHandler()` へ戻すようにした

主な修正ファイル:
- `AyaoriHIME/Handler/KeyboardEventHandler.cs`

## 直近で確認していた不具合と対応状況

### `nfer + F7`
- 症状:
  - `#holdShift nfer D both`
  - `-F7>"お願いします"`
  - なのに `F7` の `#singleHit` が先に発火していた
- 原因:
  - `F7` が `CommonTable singleHit pending` に先に入っていた
- 対応:
  - HoldShift 押下中は `tryBeginPendingCommonSingleHit()` で pending を作らないよう修正

### `RCtrl + K`
- 症状:
  - `#holdShift rctrl C both` が効かない
- 原因:
  - `RCtrl` (`0xa3`) が `isEffectiveVkey()` で落ち、SystemModifier 経路に入っていなかった
- 対応:
  - SystemModifier 判定を `isEffectiveVkey()` より前に移動

### `RCtrl + H -> BS`
- 症状:
  - 既存の Ctrl 変換が効かない
- 原因:
  - `Rctrl` が HoldShift として `SHIFTED` になったあと、未定義 HoldShift 組合せでも Ctrl 変換経路へ戻っていなかった
  - さらに `Rctrl` KeyUp で singleHit (`MultiStreamKanjiPreferredNext`) が誤発火していた
- 対応:
  - 未定義 SystemModifier HoldShift の場合は pending singleHit を consumed にしてから通常の `keyboardDownHandler()` に戻すよう修正

## まだ注意が必要な点

### 1. `DlgModConversion` は動作検証不足
- コード上は `commonTable.txt` の読み書きにつながっている
- ただし、
  - ダイアログ編集
  - 保存
  - 再読込
  - 実キー動作確認
 までの通し確認は未実施

### 2. `CommonTableRuntime` の編集用保持は暫定
- `currentDefinition` を持って UI 編集に使っている
- `UpdateSingleHit()` / `UpdateHoldShiftTarget()` は `RawTarget` 中心で持っている
- runtime 再初期化との整合は、実機での保存後再読込確認が必要

### 3. `mod-conversion.txt` の UI/設定項目はまだ残っている
- runtime 読込は止めたが、設定画面の文言や旧 UI 名称は未整理

## 次セッションでまず見るべきこと

1. `DlgModConversion` で `commonTable.txt` を編集して保存できるか
2. 保存後に再読込して実際にキー動作へ反映されるか
3. HoldShift と既存 Ctrl 変換の優先順位
   - `Rctrl + K`
   - `Rctrl + H`
   - `nfer + F7`
   - `nfer + Tab`
4. `RCtrl` / `LCtrl` / `LShift` / `RShift` の singleHit 誤発火がないか

## 直近コミット
- `71c5b3c Split key combo reverse map and document common table`
- `a556166 Wire common table editing and fix hold shift paths`
