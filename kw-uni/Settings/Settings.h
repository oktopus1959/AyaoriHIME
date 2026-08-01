#pragma once

#include "std_utils.h"

#define SYSTEM_FILES_FOLDER     SETTINGS->systemFilesFolder
#define USER_FILES_FOLDER       SETTINGS->userFilesFolder

#define JOIN_SYSTEM_FILES_FOLDER(p)     utils::joinPath(SYSTEM_FILES_FOLDER, p)
#define JOIN_USER_FILES_FOLDER(p)       utils::joinPath(USER_FILES_FOLDER, p)

struct Settings {
    bool firstUse = true;      // 最初の起動か

    bool isJPmode = true;      // キーボードがJPモードか

    String rootDir;            // ルートフォルダ
    String systemFilesFolder;  // システムファイルフォルダ
    String userFilesFolder;    // ユーザーファイルフォルダ
    String tableFile;          // ストロークテーブル
    String tableFile2;         // ストロークテーブル2
    String tableFile3;         // ストロークテーブル3
    String charsDefFile;       // Deckey から文字への変換
    String easyCharsFile;      // 簡易打鍵文字ファイル
    String kanjiYomiFile;      // 漢字読みファイル
    String bushuFile;          // 部首合成辞書
    String autoBushuFile;      // 自動部首合成辞書
    String mazegakiFile;       // 交ぜ書き辞書
    String simpleDicFile;      // 簡易辞書
    String systemRomanDicFile; // システムローマ字辞書ファイル

    int backFileRotationGeneration = 3;     // 辞書ファイル保存世代数

    size_t simpleDicMapKeyMaxLength = 16;   // 簡易辞書変換キーの最大長

    bool simpleDicSelectFirstCandByEnter = false; // 簡易辞書の第1候補をEnterキーで選択する
    bool simpleDicNewLineWhenEnter = false;        // 簡易辞書候補選択時のEnterではつねに改行する
    size_t simpleDicHorizontalCandMax = 5;         // 簡易辞書候補の横列鍵盤表示の際の最大数

    bool simpleDicMoveShortestAt2nd = false;       // 最短長候補を2番目に表示する
    bool simpleDicShowCandsFromFirst = true;       // 最初の簡易辞書候補選択から横列候補表示を行うか
    bool simpleDicUseArrowToSelectCand = true; // 矢印キーで簡易辞書候補選択を行う
    bool simpleDicSelectCandByTab = true;      // Tabキーで簡易辞書候補選択を行う

    bool simpleDicGatherAllCandidates = true; // キーの開始位置をずらして全簡易辞書候補を取得するか
    bool mazegakiSelectFirstCand = false;   // 交ぜ書き変換で先頭の候補を自動選択
    bool mazeBlockerTail = true;            // 交ぜ書き変換で、変換後のブロッカーの位置
    bool mazeRemoveHeadSpace = true;        // 交ぜ書き変換で、変換開始位置の空白を削除
    bool mazeRightShiftYomiPos = true;      // 交ぜ書き変換で、読みの開始位置を右移動する
    size_t mazeYomiMaxLen = 10;             // 交ぜ書き変換時の最長入力読み長


    int hiraToKataShiftPlane = 0;   // Shift入力された平仮名をカタカナに変換する面
    bool hiraToKataNormalPlane = false;   // 通常面の平仮名をカタカナに変換する
    bool convertJaPeriod = false;           // 「。」と「．」を相互変換する
    bool convertJaComma = false;            // 「、」と「，」を相互変換する

    bool eisuModeEnabled = false;           // 英大文字入力による英数モード移行が有効か
    wchar_t eisuSimpleDicSearchChar = '\0'; // 英数モードから簡易辞書検索を呼び出す文字
    wchar_t eisuExitAsIsChar = '\0';        // 英数モードからそのまま抜ける文字
    wchar_t eisuExitDecapitalChar = '\0';   // 英数モードから小文字化して抜ける文字
    size_t eisuExitCapitalCharNum = 3;      // 英数モードを自動的に抜けるまでの大文字数
    size_t eisuExitSpaceNum = 2;            // 英数モードを自動的に抜けるまでのSpace数

    bool removeOneByBS = false;             // BS で直前打鍵のみを取り消す

    bool yamanobeEnabled = false;           // YAMANOBEアルゴリズムを有効にするか
    //bool autoBushuComp = false;             // 自動部首合成を行う
    size_t autoBushuCompMinCount = 1;       // 自動首部合成を有効にする最小合成回数

    String romanBushuCompPrefix;           // ローマ字テーブル出力時の部首合成用プレフィックス
    String romanSecPlanePrefix;            // 裏面定義文字に対するローマ字出力時のプレフィックス

    bool kanaTrainingMode = false;          // かな入力練習モードか
    bool googleCompatible = false;          // Google日本語入力と互換な後置書き換えか(falseなら書き換えられた文字列も1文字ずつが対象になる)
    bool multiStreamMode = false;           // 漢直・かな融合モードか
    bool multiCandidateMode = false;        // 複数候補モードか
    bool isHiraganaTableOnly = false;       // かな配列だけを使用するか

    bool collectRealtimeNgram = true;         // Realtime Ngram 情報を収集する
    bool useTmpRealtimeNgramFile = false;   // 一時的なRealtime Ngram ファイルを使用する
    bool hiraganaBigramEnabled = false;     // ひらがな交じりのBigramを有効にする
    bool hiraganaQuadgramEnabled = false;   // ひらがな交じりの4-gramを有効にする
    bool morphCostWithoutEOS = true;           // EOSまで含めた形態素解析コストを使用しない
    String morphMazeFormat = L"maze1";      // 形態素解析器の出力に使用する交ぜ書きフォーマット (maze1 / maze2)
    int morphMazeEntryPenalty = 1000;       // 交ぜ書きエントリに対するペナルティ
    int morphMazeConnectionPenalty = 1000;  // 交ぜ書きエントリの接続に対するペナルティ
    int morphNonTerminalCost = 5000;        // 非終端形態素の単語コスト
    int analyzeMorphLen = 10;               // 形態素解析を行う際の最小形態素長
    int ngramCostFactor = 1;                // 形態素コストに対するNgramコストの係数
    double char3gramWeight = 1.0;           // 文字3-gram言語モデルの重み
    double char3gramTailKanjiCostDecayRate = 0.5; // 文字3-gram窓末尾が漢字の場合のコスト減衰率
    double char4gramWeight = 0.0;           // 文字4-gramボーナスの係数 (正値にすると Char3gramひらがなのコストは 0 扱いになる)
    int ngramMaxBonusPoint = 25;            // Ngramに与えるボーナスポイントの最大値
    int ngramBonusPointFactor = 100;        // 嵩上げされたNgramに与えるボーナスの係数
    int ngramManualSelectDelta = 10;        // 候補選択によるNgramカウントの変動幅
    int mazegakiBonusPointFactor = 2000;    // 交ぜ書きで選択された候補に対するボーナス
    //bool commitByPunctuation = true;        // 句読点でコミットする
    bool outputHeadSymbol = true;           // 先頭の記号類をそのまま出力する
    bool strokeBackByBS = false;            // BSで打鍵取消を行う
    int maxStrokeBackCount = 0;             // BSで打鍵取消を行う時に、何回を超えたら通常のBS動作に戻すか
    int multiStreamBeamSize = 10;           // multi-stream モードでのBeamSize
    double extraBeamSizeRate = 0.5;         // 余分に残しておく候補の割合
    int remainingStrokeSize = 5;            // 残しておく多ストロークの範囲 (stroke位置的に組み合せ不可だったものは、strokeCount が範囲内なら残しておく)
    int recentConnectionKeepStrokeCount = 2;    // 直近Nストローク分の接続候補を上位に保持する範囲
    //int variableTailLength = 10;            // 入力の末尾部分で、可変となる部分の長さ (形態素解析の対象となる長さを計算するのに使用される)
    int fixLeaderCharStrokeCount = 5;       // 指定されたストローク数以上、先頭部が同じ文字の場合にその部分を固定するための、ストローク長の閾値
    int challengeNumForSameLeader = 4;      // 解の先頭部分が同じならそれらだけを残すようにするための、チャレンジ打鍵数
    int kanjiNoKanjiBonus = 1500;           // 「漢字+の+漢字」のような場合に与えるボーナス
    int loweredContinuousKanjiNum = 0;      // 連続するN文字の漢字列にはコストを与える
    int exclusivePrefixCode = -1;           // 排他的なストローク処理を開始する文字のコード。このコードから始まるストローク列が全て完了するまでは、途中から別のストロークを始めない 
    String mergerCandidateFile;             // 解候補ログファイル
    int mergerCandidateMin = 3;             // 解候補ログに出力する最小解数
    int mergerCandidateMax = 10;            // 解候補ログに出力する最大解数
    bool useEditWindow = false;             // 編集バッファを使用するか
    String editBufferCaretChar = L"▴";      // 編集バッファのカレット文字
    bool multiStreamDetailLog = false;      // multi-stream モードでの詳細ログを有効にする

    // for Debug
    bool debughState = false;               // State モジュールで DebugH を有効にする
    bool debughMazegaki = false;            // mazegaki モジュールで DebugH を有効にする
    bool debughMazegakiDic = false;         // mazegakiDic モジュールで DebugH を有効にする
    bool debughStrokeTable = false;         // strokeTable モジュールで DebugH を有効にする
    bool debughBushu = false;               // bushuComp モジュールで DebugH を有効にする
    bool debughString = false;              // String モジュールで DebugH を有効にする
    bool debughZenkaku = false;             // Zenkaku モジュールで DebugH を有効にする
    bool debughKatakana = false;            // Katakana モジュールで DebugH を有効にする
    bool debughMyPrevChar = false;          // MyChar/PrevChar モジュールで DebugH を有効にする
    bool bushuDicLogEnabled = false;        // bushuDic で InfoH を有効にする
    bool developerSettingsEnabled = false;  // 開発者用設定を有効にする

public:
    void SetValues(const std::map<String, String>&);

    static std::unique_ptr<Settings> Singleton;
};

#define SETTINGS  (Settings::Singleton)
