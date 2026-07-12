# ADR: TSF composition と EditBuffer のライフサイクル対応

## 日付
2026-07-11

## 状態
採用

## 背景
- AyaoriHIME は低レベルキーボードフックで入力を捕捉し、`kw-uni` のデコード結果を `FrmEditBuffer` に保持している
- `FrmEditBuffer` の内容は、次の入力によって `numBS` 分を削除して別の文字列へ書き換えられる可変な文字列である
- 従来の `SendInput` 経路では、この可変な文字列を対象アプリへ逐次出力し、Backspace と文字列入力によって表示内容を同期していた
- 先行ADR `2026-07-08-tsf-output-service-boundary.md` は、TSFを「確定文字列出力sink」とし、逐次出力を対象アプリの本文へ直接挿入する方針を採った
- その実装では、各 `PutString()` を `CommitText` としてTSFへ送り、edit sessionごとに確定文字列の挿入と前方削除を行っていた
- メモ帳での検証では、edit session内では挿入後の文書範囲が確認できたが、次のedit sessionまで維持されず、前回範囲へのアクセスが `TS_E_INVALIDPOS` になった
- `FrmEditBuffer` 上で書き換え可能な文字列は未確定文字列であり、通常のIMEと同様にTSF compositionとして扱う必要がある

## 決定
- TSF text serviceは、AyaoriHIMEの未確定文字列をTSF compositionとして対象アプリへ提示する
- `FrmEditBuffer.PutString()` による逐次変更は、確定文字列の挿入ではなくcompositionの開始または更新として扱う
- AyaoriHIMEが確定を指示した時点で、TSF text serviceはcompositionを終了し、対象アプリの本文へ確定する
- AyaoriHIME側のEditBufferを引き続き未確定文字列状態の正とする
- `kw-uni` のデコード、OutputStack、Lattice、候補選択、および`editBufferData`の意味は変更しない
- TSF text serviceは対象アプリ内のcomposition rangeと、AyaoriHIMEから受信したcomposition識別子を管理する
- 先行ADR `2026-07-08-tsf-output-service-boundary.md` の「TSFを確定文字列出力sinkとする」という決定を本ADRで置き換える

## 責務境界

### AyaoriHIME / kw-uni
- キー入力の捕捉と抑止
- `deckey`生成とデコード
- 候補選択と書き換え内容の決定
- EditBufferの保持
- compositionの開始、更新、確定、キャンセルをTSF text serviceへ指示
- TSFへ送った未確定文字列とEditBufferの同期管理

### TSF text service
- 対象アプリのアクティブな`ITfContext`の管理
- `ITfContextComposition::StartComposition()`によるcomposition開始
- `ITfComposition::GetRange()`で取得したrangeの更新
- composition rangeと選択位置の管理
- `ITfComposition::EndComposition()`による確定
- compositionのキャンセル時に未確定rangeを削除してcompositionを終了
- 各操作の結果をAyaoriHIMEへ返却

## 状態モデル

TSF text serviceの各clientは、対象contextごとに次の状態を持つ。

- `Idle`
  - 管理中のcompositionがない
- `Composing`
  - `ITfComposition`とcomposition識別子を保持している
  - 更新、確定、キャンセルを受け付ける

状態遷移は次の通りとする。

| 現在状態 | 要求 | 処理 | 次状態 |
| --- | --- | --- | --- |
| Idle | UpdateComposition | compositionを開始して文字列を設定 | Composing |
| Composing | UpdateComposition | composition rangeを書き換える | Composing |
| Composing | CommitComposition | compositionを終了する | Idle |
| Composing | CancelComposition | rangeを削除してcompositionを終了する | Idle |
| Idle | CommitComposition | 冪等な成功として扱う | Idle |
| Idle | CancelComposition | 冪等な成功として扱う | Idle |

## compositionの更新単位
- AyaoriHIMEは、未確定文字列の差分として`numBS`と`outString`を引き続き生成する
- IPCでは誤差の累積を避けるため、原則として更新後のEditBuffer全文を`UpdateComposition`で送る
- TSF text serviceは保持しているcomposition range全体を受信文字列で置換する
- `numBS`はAyaoriHIME内部および従来フォールバック経路との互換性のため維持するが、TSF composition更新時のrange計算には使用しない
- 更新要求には単調増加するcomposition識別子とsequence番号を含め、古い要求を適用しない
- 文字列長とrange座標はUTF-16 code unit単位とする

## IPCプロトコル
- 既存headerの`magic/version/type/payloadLength`形式を維持する
- composition対応はprotocol versionを更新し、旧DLLとの誤接続を明示的に拒否する
- 最低限、次のmessage typeを定義する
  - `Hello`
  - `FocusChanged`
  - `UpdateComposition`
  - `CommitComposition`
  - `CancelComposition`
  - `OperationResult`
  - `Bye`
- `UpdateComposition`は次を含む
  - composition ID
  - sequence番号
  - composition先頭からのカレットoffset
  - UTF-16LE文字列のbyte長
  - 更新後の未確定文字列全文
- `CommitComposition`はcomposition ID、sequence番号、確定するprefix長を含む
- prefix長がcomposition全文より短い場合は、残りを同じ論理IDの新しいcompositionとして維持する
- `CancelComposition`はcomposition IDとsequence番号を含む
- `OperationResult`は要求種別、composition ID、sequence番号、HRESULTを含む
- 文字列本文はログへ出力せず、文字列長、ID、sequence番号、HRESULTのみ記録する

## 確定契機
- `FrmEditBuffer`が保持している未確定文字列を確定バッファへ移す時点で`CommitComposition`を送る
- decoderの無効化、確定操作、対象アプリへ制御キーを送る前など、既存の`FlushBuffer()`契機を分類して確定要求へ対応付ける
- `FlushBuffer()`の全呼び出しを無条件に同じ意味とはみなさず、現在の呼び出し理由を調査して確定とキャンセルを区別する
- composition確定後も、AyaoriHIME側のEditBufferとOutputStackの既存同期規則を維持する

## フォーカスとcontext変更
- compositionは開始時の`ITfContext`に属する
- composition中に同一client内でcontextが変わった場合、旧contextのcompositionを確定またはキャンセルしてから新contextを採用する
- フォーカス喪失時の既定動作はcompositionの確定とする
- contextが既に無効で確定操作を実行できない場合、TSF text serviceはcomposition参照を破棄し、失敗結果を返す
- 別プロセスのTSF clientから届いた`focus=False`で、現在activeな別clientを解除しない
- 最後に`focus=True`を通知した接続済みclientをactive clientとする
- focus通知が遅延または欠落した場合は、named pipeから取得したclient process IDとforeground windowのprocess IDが一致する接続を優先し、active clientを復旧する
- TSF callbackを実行するUIスレッドおよびそこから派生する通知処理ではnamed pipe I/Oを行わない
- アプリ切替時はforeground process IDの変化をAyaoriHIME側で検出し、旧論理compositionを確定済みとして終了する

## フォールバック
- composition開始前にTSFが明示失敗した場合は、既存`SendInput`経路へフォールバックできる
- `tsfFallbackClassNames`に`|`区切りで指定したwindow classでは、composition開始前から既存出力経路を使用する
- `tsfFallbackClassNames`は大文字・小文字を区別しない前方一致とし、既定値を`mintty`とする
- TSF text serviceはcomposition開始前に、selection rangeの`ITfContextView::GetTextExt()`でインライン表示位置を取得する
- レイアウト取得失敗、無効な矩形、またはcontext viewのウィンドウ外を指す矩形の場合は、本文を変更せず明示失敗を返して既存出力経路へフォールバックする
- `ITfTextInputProcessorEx::ActivateEx()`の起動フラグを記録するが、`TF_TMAE_CONSOLE`だけを理由にフォールバックしない
- minttyのIMM互換text storeは有効な`GetTextExt()`矩形を返す一方で任意のTSF serviceのcompositionをインライン表示しないため、能力検査だけではなく既定のwindow class判定を併用する
- composition開始後は、同じ未確定文字列を`SendInput`でも出力すると二重表示になるため、操作単位の安易なフォールバックを行わない
- composition更新失敗時は、そのcompositionを異常状態として扱い、再同期またはキャンセルを試みる
- timeout後はTSF側で要求が適用された可能性があるため、従来通り自動フォールバックしない
- TSFから従来経路へ切り替える場合は、compositionの終了と対象アプリ上の残存文字列を確認できる明示的な切替手順を設ける

## 実装方針
1. `ITextOutputSink`の文字列送信だけでは表現できないため、composition操作を表す境界を追加する
2. `FrmEditBuffer`の更新後全文と確定・キャンセル契機を新しい境界へ渡す
3. named pipe protocolをcomposition対応versionへ更新する
4. TSF DLLにcontext単位のcomposition状態を追加する
5. `ITfContextComposition`と`ITfComposition`を使用して開始、更新、確定、キャンセルを実装する
6. active client切替時のcomposition処理を実装する
7. 旧`CommitText`による逐次確定挿入を削除する
8. composition開始前と開始後でフォールバック方針を分離する

## 非目標
- キー入力捕捉を`ITfKeyEventSink`へ移すこと
- デコードや候補選択をTSF DLLへ移すこと
- TSF標準候補UIを導入すること
- EditBuffer、OutputStack、Latticeの責務を変更すること
- 制御キーやショートカットの送出をすべてTSF化すること

## 影響
- `FrmEditBuffer`の更新とTSF compositionが同じ未確定文字列を表すようになる
- 対象アプリは未確定文字列をcompositionとして認識できる
- composition開始後の障害処理は、単純な文字列出力sinkより複雑になる
- protocol version更新により、AyaoriHIME.exeとTSF DLLは対応する版を同時に配置する必要がある
- 対象アプリによってcomposition表示やフォーカス喪失時の挙動が異なるため、アプリ別検証が必要になる

## 検証方針
- メモ帳で「の」が未確定compositionとして表示され、続く更新で同じrangeが「愛」へ置換されること
- 更新間のedit sessionをまたいでもcomposition rangeが有効であること
- 確定操作後にcompositionが終了し、本文として「愛」が残ること
- キャンセル操作後に未確定文字列が本文へ残らないこと
- 文中選択、文頭、文末、および既存文字列の途中でcompositionを開始できること
- サロゲートペアを含む文字列をUTF-16 code unit契約で更新できること
- フォーカス移動時に旧contextのcompositionが規定通り確定されること
- timeout後に`SendInput`へフォールバックせず、二重出力しないこと
- `useTsfOutput=false`では既存出力の挙動が変わらないこと
- Visual Studio、ブラウザ、ターミナル系アプリでsmoke testを行うこと

## 未決事項
- `FrmEditBuffer.FlushBuffer()`の各呼び出しを確定とキャンセルのどちらへ対応付けるか
- EditBufferの一部だけを確定し、残りをcompositionとして維持する必要があるか
- TSF属性による未確定文字列の表示形式を初期実装で設定するか
- composition更新失敗後の再同期手順
- フォーカス喪失時に、設定や操作理由によって確定とキャンセルを切り替える必要があるか

## 参照
- `docs/adr/2026-07-08-tsf-output-service-boundary.md`
- `docs/adr/2026-01-18-outputstack-editbuffer-sync.md`
- `docs/spec/2026-01-output_stack_整合.md`
- `AyaoriHIME/Forms/FrmEditBuffer.cs`
- `AyaoriHIME/Handler/TextOutput/TsfTextOutputSink.cs`
- `AyaoriHimeTsfTextService/dllmain.cpp`
