# ADR: realtime / user Ngram の build-time 辞書統合

## 日付
2026-05-11

## 背景
- `NgramAnalyzer/work/input/wiki_hplt.all.txt` から作成した `wiki_hplt.all.score.*.txt` を、`NgramAnalyzer/tools/make_ngram_dict.sh` で `ngram-sys.dic` に変換している
- 一方で、利用者の入力から抽出した realtime Ngram は `NgramAnalyzer/work/input/realtime.ngram.*.txt` に保存され、従来は `[NgramAnalyzer/NgramCoreLib/src/analyzer/RealtimeDict.cpp](/F:/Dev/CSharp/AyaoriHIME/src/NgramAnalyzer/NgramCoreLib/src/analyzer/RealtimeDict.cpp)` の `calcRealtimeBonus(...)` により実行時ボーナスとして扱っていた
- また、利用者が明示的に定義する user Ngram は `user.ngram.*.txt` 系で管理し、runtime では同じく `RealtimeDict.cpp` の user 側処理でボーナスまたはペナルティとして扱う
- この方式では、辞書本体のコストと realtime / user 側の補正が分離しており、辞書生成実験や配布用辞書の比較がしにくい
- `RealtimeDict.cpp` では `ゲタ + 漢字` の特殊扱いがあり、先頭が `〓` で次が漢字の場合は、ゲタを飛ばした漢字始まり Ngram も検索対象としている
- realtime / user ともに入力ファイルは今後も増える見込みであり、単一ファイル固定では運用しづらい

## 決定
- realtime / user Ngram の補正は、実行時だけでなく build-time でも `wiki_hplt` ベース辞書へ反映できるようにする
- 反映は `[NgramAnalyzer/tools/merge_realtime_ngram_cost.rb](/F:/Dev/CSharp/AyaoriHIME/src/NgramAnalyzer/tools/merge_realtime_ngram_cost.rb)` を正とし、互換のために `[NgramAnalyzer/tools/merge_realtime_ngram_cost.ps1](/F:/Dev/CSharp/AyaoriHIME/src/NgramAnalyzer/tools/merge_realtime_ngram_cost.ps1)` も同仕様で維持する
- `NgramAnalyzer/tools/make_ngram_dict.sh` は、まず merged テキスト辞書 `work/build/ngram-sys-source.txt` を生成し、その staging ディレクトリだけを `make-dict` に渡す
- realtime 入力は `work/input/realtime.ngram.*.txt` を名前順に列挙し、同一キーはファイル横断でカウント加算する
- user 入力は `work/input/user.ngram.*.txt` を名前順に列挙し、複数ファイルを順に読み込めるようにする
- realtime / user の入力 glob に一致するファイルが 0 件でもエラーにせず、その種別を無視して続行する
- realtime ボーナスは `RealtimeDict.cpp` と同じ `log(1 + count) / log(1 + maxCount)` ベースで計算し、既存キーには減算、新規キーには基準コストからの減算として適用する
- user ボーナスは `RealtimeDict.cpp` の `calcUserBonus(...)` と同じ段階関数を build-time 側にも再現し、既存キーには減算、新規キーには user 用基準コストからの減算として適用する
- user 側では負のポイントを許可し、build-time 側でもペナルティとしてコスト増に反映する
- 先頭が漢字の realtime Ngram については、`key` 本体に加えて `〓 + key` にも同じボーナスを反映する
- user 側でも、元語に加えて `〓 + word` を派生登録する
- user 側で 6 文字以上の語は、runtime と同様に overlapping 5gram を派生登録する
- スクリプトは実処理に入る前に、利用する `base file` / `realtime files` / `user files` / `output file` を INFO として表示する
- これら 4 種の INFO は、4 項目の内容が確定した時点でまとめて出力する

## 反映ルール
### realtime
- 対象長は 2～4 文字に限定する
- 2～4 文字以外の realtime エントリは build-time 統合対象外とする
- ひらがなの 2gram は常に無視する
- ひらがなの 4gram 以上は、頻度 2 以下なら無視し、頻度 3 以上なら反映する
- ひらがな以外の 2～4gram は、上記ひらがな条件に該当しない限り反映する
- 新規キーの基準コストは通常 `6000`、先頭が `〓` の派生キーは `9500` を使う

### user
- 入力は `<word>\t<point>` を基本形式とし、値は省略可能とする
- point 省略時は `ngramMaxBonusPoint` 相当の既定値 `25` を用いる
- point は符号付き整数を許可し、負値は build-time 側でもペナルティとして扱う
- point の build-time 変換は `calcUserBonus(...)` と同じ段階関数を使い、`bonusFactor=100` を既定値とする
- 新規キーの基準コストは通常 `6000`、先頭が `〓` の派生キーは `9500` を使う
- 入力ファイルが複数ある場合は名前順に読み込み、後から読んだ同一キーが派生登録結果を上書きし得る点も runtime の map 更新と同様に許容する

## 実装方針
- `[NgramAnalyzer/tools/merge_realtime_ngram_cost.rb](/F:/Dev/CSharp/AyaoriHIME/src/NgramAnalyzer/tools/merge_realtime_ngram_cost.rb)` で `wiki_hplt.all.score.*.txt` を読み、realtime 群と user 群の集計後に merged 結果を書き出す
- `ひらがなのみ` と `先頭漢字` の判定をスクリプト側で行う
- `〓 + key` の派生追加は、runtime の `RealtimeDict.cpp` が持つ `ゲタ + 漢字` 特例と整合するように build-time 側でも同じ意図で入れる
- user についても Ruby / PowerShell の両スクリプトで `calcUserBonus(...)` を再実装し、point の省略・負値・複数ファイル入力を同じ仕様で扱う
- INFO 出力は、入力 glob の展開と存在確認が終わり、4 項目の表示内容が確定した時点でまとめて出力する
- `[NgramAnalyzer/tools/make_ngram_dict.sh](/F:/Dev/CSharp/AyaoriHIME/src/NgramAnalyzer/tools/make_ngram_dict.sh)` は `work/build/` を staging として使い、そこに生成した `ngram-sys-source.txt` のみを辞書コンパイラへ渡す

## 影響範囲
- `[NgramAnalyzer/tools/merge_realtime_ngram_cost.rb](/F:/Dev/CSharp/AyaoriHIME/src/NgramAnalyzer/tools/merge_realtime_ngram_cost.rb)`
- `[NgramAnalyzer/tools/merge_realtime_ngram_cost.ps1](/F:/Dev/CSharp/AyaoriHIME/src/NgramAnalyzer/tools/merge_realtime_ngram_cost.ps1)`
- `[NgramAnalyzer/tools/make_ngram_dict.sh](/F:/Dev/CSharp/AyaoriHIME/src/NgramAnalyzer/tools/make_ngram_dict.sh)`
- `[NgramAnalyzer/work/build/ngram-sys-source.txt](/F:/Dev/CSharp/AyaoriHIME/src/NgramAnalyzer/work/build/ngram-sys-source.txt)`

## 既知の注意点
- build-time で merged した辞書を使いながら、同じ realtime 入力を runtime 側でもそのまま読むと、重複キーには二重に補正がかかる
- 同様に、build-time で user の知識を折り込んだ上で runtime 側でも同じ user ファイルを読むと、重複キーには二重に補正がかかる
- この ADR は「辞書生成時に realtime / user の知識を折り込む」方針を定めるものであり、runtime 側の差分適用戦略まではまだ統合していない
- ひらがな 2gram / 4gram の扱いは性能とノイズ抑制のための暫定ルールであり、今後の評価結果で見直す可能性がある
- user の build-time 新規追加は、runtime の「user 単独エントリは辞書未登録なら standalone ノード化しない」挙動を完全再現するものではない
