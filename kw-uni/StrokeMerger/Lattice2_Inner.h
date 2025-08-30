#pragma once

#include "string_utils.h"

#include "Lattice.h"

namespace lattice2 {
    class CandidateString;

#define SYSTEM_NGRAM_FILE           L"files/mixed_all.ngram.txt"
#define KATAKANA_COST_FILE          L"files/katakana.cost.txt"
#define REALTIME_NGRAM_MAIN_FILE    L"files/realtime.ngram.txt"
#define REALTIME_NGRAM_TEMP_FILE    L"files/realtime.ngram.tmp.txt"
//#define USER_NGRAM_FILE           L"files/user.ngram.txt"
#define USER_COST_FILE              L"files/userword.cost.txt"
#define MAZEGAKI_PRIOR_FILE         L"files/mazegaki.prior.txt"

    // ビームサイズ
    //size_t BestKSize = 5;
    inline size_t maxCandidatesSize = 0;

    // 漢字用の余裕
    inline int EXTRA_KANJI_BEAM_SIZE = 3;

    //// 多ストロークの範囲 (stroke位置的に組み合せ不可だったものは、strokeCount が範囲内なら残しておく)
    //int AllowedStrokeRange = 5;

    //// 末尾がここで設定した長さ以上に同じ候補は、先頭だけを残して削除
    //int LastSameLen = 99;

    //// 解の先頭部分が指定の長さ以上で同じなら、それらだけを残す
    //int SAME_LEADER_LEN = 99;

    // 非優先候補に与えるペナルティ
    inline int NON_PREFERRED_PENALTY = 1000000;

    // 末尾が同じ候補と見なす末尾長
    inline size_t SAME_TAIL_LEN = 3;

    // 形態素解析の結果、交ぜ書きエントリであった場合のペナルティ
    //int MAZE_PENALTY_2 = 5000;      // 見出し2文字以下
    //int MAZE_PENALTY_3 = 3000;      // 見出し3文字
    //int MAZE_PENALTY_4 = 1000;      // 見出し4文字以上

    //// 漢字と長音が連続する場合のペナルティ
    //int KANJI_CONSECUTIVE_PENALTY = 1000;

    //// 漢字が連続する場合のボーナス
    //int KANJI_CONSECUTIVE_BONUS = 0;

    //// カタカナが連続する場合のボーナス
    //int KATAKANA_CONSECUTIVE_BONUS = 1000;

    //// 末尾がひらがなの連続にボーナスを与える場合のひらがな長
    //int TAIL_HIRAGANA_LEN = 0;  // 4

    //// 末尾がひらがなの連続場合のボーナス
    //int TAIL_HIRAGANA_BONUS = 0; //1000;

    //// 「漢字+の+漢字」の場合のボーナス
    //int KANJI_NO_KANJI_BONUS = 1500;

    // cost ファイルに登録がある場合のデフォルトのボーナス
    inline int DEFAULT_WORD_BONUS = 1000;

    // cost ファイルに登録がある unigram のデフォルトのボーナスカウント
    inline int DEFAULT_UNIGRAM_BONUS_COUNT = 10000;

    // 1文字のひらがな形態素で、前後もひらがなの場合のコスト
    inline int MORPH_ISOLATED_HIRAGANA_COST = 3000;

    // 連続する単漢字の場合のコスト
    inline int MORPH_CONTINUOUS_ISOLATED_KANJI_COST = 3000;

    // 2文字以上の形態素で漢字を含む場合のボーナス
    //int MORPH_ANY_KANJI_BONUS = 5000;
    //int MORPH_ANY_KANJI_BONUS = 3000;
    inline int MORPH_ANY_KANJI_BONUS = 1000;

    // 3文字以上の形態素ですべてひらがなの場合のボーナス
    inline int MORPH_ALL_HIRAGANA_BONUS = 1000;

    // 2文字以上の形態素ですべてカタカナの場合のボーナス
    inline int MORPH_ALL_KATAKANA_BONUS = 3000;

    // 2文字以上の形態素で先頭が高頻度助詞、2文字目がひらがな以外の場合のボーナス
    inline int HEAD_HIGH_FREQ_JOSHI_BONUS = 1000;

    // SingleHitの高頻度助詞が、マルチストロークに使われているケースのコスト
    inline int SINGLE_HIT_HIGH_FREQ_JOSHI_KANJI_COST = 3000;

    // 先頭の1文字だけの場合に、複数の候補があれば、2つ目以降には初期コストを与える
    inline int HEAD_SINGLE_CHAR_ORDER_COST = 5000;

    // 孤立した小書きカタカナのコスト
    inline int ISOLATED_SMALL_KATAKANA_COST = 3000;

    // 孤立したカタカナのコスト
    inline int ISOLATED_KATAKANA_COST = 3000;

    // 孤立した漢字のコスト
    inline int ONE_KANJI_COST = 1000;

    // 1スペースのコスト
    inline int ONE_SPACE_COST = 20000;

    // デフォルトの最大コスト
    inline int DEFAULT_MAX_COST = 1000;

    //// 形態素コストに対するNgramコストの係数
    //int NGRAM_COST_FACTOR = 5;

    // Realtime Ngram のカウントを水増しする係数
    inline int REALTIME_FREQ_BOOST_FACTOR = 10;

    // Realtime Ngram のカウントを水増ししたときの、カウント値をスムージングする際の閾値
    inline int SYSTEM_NGRAM_COUNT_THRESHOLD = 3000;

    inline const MString NonTerminalMarker = to_mstr(L"非終端,*");

    bool isNonTerminalMorph(const MString& morph);

    //// Realtime 3gram のカウントからボーナス値を算出する際の係数
    //int REALTIME_TRIGRAM_BONUS_FACTOR = 100;

    //int TIER1_NUM = 5;
    //int TIER2_NUM = 10;

    // int STROKE_COST = 150;

    // Ngram統計によるコスト
    // 1-2gramはポジティブコスト(そのまま計上)、3gram以上はネガティブコスト(DEFAULT_MAX_COST を引いて計上)として計算される
    inline std::map<MString, int> ngramCosts;

    // 利用者が手作業で作成した単語コスト(3gram以上、そのまま計上)
    inline std::map<MString, int> userWordCosts;

    // 利用者が手作業で作成したNgram統計
    inline std::map<MString, int> userNgram;
    inline int userMaxFreq = 0;

    // システムで用意したNgram統計
    inline std::map<MString, int> systemNgram;
    inline int systemMaxFreq = 0;

    // システムで用意したカタカナ単語コスト(3gram以上、sysCost < uesrCost のものだけ計上)
    inline std::map<MString, int> KatakanaWordCosts;

    // 利用者が入力した文字列から抽出したNgram統計
    inline std::map<MString, int> realtimeNgram;
    inline int realtimeMaxFreq = 0;

    // 常用対数でNgram頻度を補正した値に掛けられる係数
    inline int ngramLogFactor = 50;

    // Ngram頻度がオンラインで更新されたか
    inline bool realtimeNgram_updated = false;

    // グローバルな後置書き換えマップ
    inline std::map<MString, MString> globalPostRewriteMap;

    // 交ぜ書き優先度
    inline std::map<MString, int> mazegakiPriorDict;

    // 交ぜ書き優先度がオンラインで更新されたか
    inline bool mazegakiPrior_updated = false;

    inline bool isDecimalString(StringRef item) {
        return utils::reMatch(item, L"[+\\-]?[0-9]+");
    }

    inline bool all_hiragana(const MString& word) {
        return std::all_of(word.begin(), word.end(), [](mchar_t x) { return utils::is_hiragana(x);});
    }

    inline bool all_pure_katakana(const MString& word) {
        return std::all_of(word.begin(), word.end(), [](mchar_t x) { return utils::is_pure_katakana(x);});
    }

    inline bool all_kanji(const MString& word) {
        return std::all_of(word.begin(), word.end(), [](mchar_t x) { return utils::is_kanji(x);});
    }

    inline int count_kanji(const MString& word) {
        return (int)std::count_if(word.begin(), word.end(), [](mchar_t c) { return utils::is_kanji(c); });
    }

    inline bool contains_half_or_more_kanji(const MString& word) {
        int len = (int)word.length();
        int nHalf = len / 2 + (len % 2);
        return count_kanji(word) >= nHalf;
    }

    inline bool isHighFreqJoshi(mchar_t mc) {
        return mc == L'が' || mc == L'を' || mc == L'に' || mc == L'の' || mc == L'で' || mc == L'は';
    }

    inline bool isHeadHighFreqJoshi(mchar_t mc) {
        return mc == L'と' || mc == L'を' || mc == L'に' || mc == L'の' || mc == L'で' || mc == L'は' || mc == L'が';
    }

    inline bool isSmallKatakana(mchar_t mc) {
        return mc == L'ァ' || mc == L'ィ' || mc == L'ゥ' || mc == L'ェ' || mc == L'ォ' || mc == L'ャ' || mc == L'ュ' || mc == L'ョ';
    }

    inline bool isCommitChar(mchar_t mch) {
        //return utils::is_punct_or_commit_char(mch) || utils::is_paren(mch);
        return !utils::is_japanese_char_except_nakaguro(mch) && !is_numeral((wchar_t)mch) && mch != L'　' && mch != ' ';
    }

    //-------------------------------------------------------------------------------------------
    // ストローク長の同じ候補の数を返す
    size_t getNumOfSameStrokeLen(const std::vector<CandidateString>& candidates);

    // 候補選択による、リアルタイムNgramの蒿上げと抑制
    void _raiseAndDepressRealtimeNgramForDiffPart(const MString& oldCand, const MString& newCand);

    // リアルタイムNgramの更新
    void updateRealtimeNgram(const MString& str);

    // Ngramコストの取得
    int getNgramCost(const MString& str, const std::vector<MString>& morphs);

    // 交ぜ書き優先度の取得
    int getMazegakiPriorityCost(const MString& mazeFeat);

    // 交ぜ書き優先度の更新
    void updateMazegakiPriority(const CandidateString& raised, const CandidateString& depressed);

    MString substringBetweenNonJapaneseChars(const MString& str);

    // 先頭候補以外に、非優先候補ペナルティを与える (先頭候補のペナルティは 0 にする)
    void arrangePenalties(std::vector<CandidateString>& candidates, size_t nSameLen);

    // 先頭候補を最優先候補にする
    void selectFirst(std::vector<CandidateString>& candidates);

} // namespace lattice2

