# ADR: OutputStack と EditBuffer 同期の整理

## 日付
2026-01-18

## 背景
- フロントは常に EditBuffer をバックエンドへ送信する
- OutputStack に候補文字列が混在し、EditBuffer との不整合が発生していた
- `Decoder::HandleDeckey()` 側で OutputStack を更新していたが、K-Best/Lattice の差分処理と整合しないケースがあった

## 決定
- EditBuffer は常に正とし、`HandleDeckey()` 毎に OutputStack との同期要否を判定する
- Lattice に `syncBaseString()` を追加し、EditBuffer 変更時は K-Best をクリアして基底文字列を同期する
- OutputStack の更新は `MergerHistoryResidentState` 側で行い、末尾差分のみを更新する
  - `numBS` 分だけ `pop` して新しい出力を `push`
  - 全文置換はしない

## 実装方針
- `DecoderImpl` に `needSyncEditBuffer()` を追加し、EditBuffer と OutputStack 末尾の一致で同期要否を判断
- `Lattice2::syncBaseString()` と `KBestList::setBaseString()` を追加し、基底文字列を初期化可能にする
- `MergerHistoryState` に `syncOutputStackTail()` を追加し、`SetTranslatedOutString()` 後に OutputStack を更新
- `Decoder::HandleDeckey()` から OutputStack の pop/push を削除

## 影響範囲
- `kw-uni/Decoder.cpp`
- `kw-uni/StateCommonInfo.h`
- `kw-uni/OutputStack.h`
- `kw-uni/StrokeMerger/Lattice.h`
- `kw-uni/StrokeMerger/Lattice2.cpp`
- `kw-uni/StrokeMerger/Lattice2_Kbest.h`
- `kw-uni/StrokeMerger/Lattice2_Kbest.cpp`
- `kw-uni/StrokeMerger/MergerHistoryState.cpp`

## 既知の注意点
- EditBuffer が毎回変化する運用では、K-Best が毎回クリアされる
- OutputStack 末尾比較の判定条件は、今後の不整合ログに応じて調整する
