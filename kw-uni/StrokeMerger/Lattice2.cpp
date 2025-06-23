#include "Logger.h"
#include "string_utils.h"
#include "misc_utils.h"
#include "path_utils.h"
#include "file_utils.h"
#include "transform_utils.h"
#include "Constants.h"

#include "settings.h"
#include "StateCommonInfo.h"
#include "Lattice.h"
#include "MStringResult.h"
#include "BushuComp/BushuComp.h"
#include "BushuComp/BushuDic.h"

#include "MorphBridge.h"
#include "Llama/LlamaBridge.h"

#define _LOG_INFOH LOG_INFO
#define _LOG_DETAIL LOG_DEBUG
#if 1
#undef IS_LOG_DEBUGH_ENABLED
#define IS_LOG_DEBUGH_ENABLED true
#undef _LOG_INFOH
#undef _LOG_DETAIL
#undef LOG_INFO
#undef LOG_DEBUGH
#undef LOG_DEBUG
#define _LOG_INFOH LOG_WARN
#define _LOG_DETAIL LOG_WARN
#define LOG_INFO LOG_INFOH
#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFOH
#endif

namespace lattice2 {
    DEFINE_LOCAL_LOGGER(lattice);

    // ビームサイズ
    //size_t BestKSize = 5;
    size_t maxCandidatesSize = 0;

    // 漢字用の余裕
    int EXTRA_KANJI_BEAM_SIZE = 3;

    //// 多ストロークの範囲 (stroke位置的に組み合せ不可だったものは、strokeCount が範囲内なら残しておく)
    //int AllowedStrokeRange = 5;

    //// 末尾がここで設定した長さ以上に同じ候補は、先頭だけを残して削除
    //int LastSameLen = 99;

    //// 解の先頭部分が指定の長さ以上で同じなら、それらだけを残す
    //int SAME_LEADER_LEN = 99;

    // 非優先候補に与えるペナルティ
    int NON_PREFERRED_PENALTY = 1000000;

    // 末尾が同じ候補と見なす末尾長
    size_t SAME_TAIL_LEN = 3;

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
    int DEFAULT_WORD_BONUS = 1000;

    // cost ファイルに登録がある unigram のデフォルトのボーナスカウント
    int DEFAULT_UNIGRAM_BONUS_COUNT = 10000;

    // 1文字のひらがな形態素で、前後もひらがなの場合のコスト
    int MORPH_ISOLATED_HIRAGANA_COST = 3000;

    // 連続する単漢字の場合のコスト
    int MORPH_CONTINUOUS_ISOLATED_KANJI_COST = 3000;

    // 2文字以上の形態素で漢字を含む場合のボーナス
    //int MORPH_ANY_KANJI_BONUS = 5000;
    //int MORPH_ANY_KANJI_BONUS = 3000;
    int MORPH_ANY_KANJI_BONUS = 1000;

    // 3文字以上の形態素ですべてひらがなの場合のボーナス
    int MORPH_ALL_HIRAGANA_BONUS = 1000;

    // 2文字以上の形態素ですべてカタカナの場合のボーナス
    int MORPH_ALL_KATAKANA_BONUS = 3000;

    // 2文字以上の形態素で先頭が高頻度助詞、2文字目がひらがな以外の場合のボーナス
    int HEAD_HIGH_FREQ_JOSHI_BONUS = 1000;

    // SingleHitの高頻度助詞が、マルチストロークに使われているケースのコスト
    int SINGLE_HIT_HIGH_FREQ_JOSHI_KANJI_COST = 3000;

    // 先頭の1文字だけの場合に、複数の候補があれば、2つ目以降には初期コストを与える
    int HEAD_SINGLE_CHAR_ORDER_COST = 5000;

    // 孤立した小書きカタカナのコスト
    int ISOLATED_SMALL_KATAKANA_COST = 3000;

    // 孤立したカタカナのコスト
    int ISOLATED_KATAKANA_COST = 3000;

    // 孤立した漢字のコスト
    int ONE_KANJI_COST = 1000;

    // 1スペースのコスト
    int ONE_SPACE_COST = 20000;

    // デフォルトの最大コスト
    int DEFAULT_MAX_COST = 1000;

    //// 形態素コストに対するNgramコストの係数
    //int NGRAM_COST_FACTOR = 5;

    // Realtime Ngram のカウントを水増しする係数
    int REALTIME_FREQ_BOOST_FACTOR = 10;

    // Realtime Ngram のカウントを水増ししたときの、カウント値をスムージングする際の閾値
    int SYSTEM_NGRAM_COUNT_THRESHOLD = 3000;

    const MString NonTerminalMarker = to_mstr(L"非終端,*");

    bool isNonTerminalMorph(const MString& morph) {
        auto items = utils::split(morph, '\t');
        return items.size() > 2 && utils::startsWith(items[2], NonTerminalMarker);
    }

    //// Realtime 3gram のカウントからボーナス値を算出する際の係数
    //int REALTIME_TRIGRAM_BONUS_FACTOR = 100;

    //int TIER1_NUM = 5;
    //int TIER2_NUM = 10;

    // int STROKE_COST = 150;

    // Ngram統計によるコスト
    // 1-2gramはポジティブコスト(そのまま計上)、3gram以上はネガティブコスト(DEFAULT_MAX_COST を引いて計上)として計算される
    std::map<MString, int> ngramCosts;

    // 利用者が手作業で作成した単語コスト(3gram以上、そのまま計上)
    std::map<MString, int> userWordCosts;

    // 利用者が手作業で作成したNgram統計
    std::map<MString, int> userNgram;
    int userMaxFreq = 0;

    // システムで用意したNgram統計
    std::map<MString, int> systemNgram;
    int systemMaxFreq = 0;

    // システムで用意したカタカナ単語コスト(3gram以上、sysCost < uesrCost のものだけ計上)
    std::map<MString, int> KatakanaWordCosts;

    // 利用者が入力した文字列から抽出したNgram統計
    std::map<MString, int> realtimeNgram;
    int realtimeMaxFreq = 0;

    // 常用対数でNgram頻度を補正した値に掛けられる係数
    int ngramLogFactor = 50;

    // Ngram頻度がオンラインで更新されたか
    bool realtimeNgram_updated = false;

    // グローバルな後置書き換えマップ
    std::map<MString, MString> globalPostRewriteMap;

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

    inline void _updateNgramCost(const MString& word, int sysCount, int usrCount, int rtmCount) {
        int origSysCnt = sysCount;
        int origRtmCnt = rtmCount;
        if (word.size() <= 2) {
            // 1-2 gram は正のコスト
            int total = sysCount + usrCount + rtmCount * REALTIME_FREQ_BOOST_FACTOR;
            int delta = (int)((total >= -3 && total <= 3 ? 0 : total > 3 ? std::log(total) : -std::log(-total)) * ngramLogFactor);
            ngramCosts[word] = DEFAULT_MAX_COST - delta;
        } else if (word.size() == 3) {
            // 3gramは負のコスト
            if (std::all_of(word.begin(), word.end(), [](mchar_t x) { return utils::is_hiragana(x);})) {
                // すべてひらがななら、system ngram として扱う。(「ってい」とかが高い realtime ngram になって、「調節す」⇒「ってい老」になってしまうことを防ぐ)
                //sysCount += rtmCount * REALTIME_FREQ_BOOST_FACTOR;
                // SYSTEM_NGRAM_COUNT_THRESHOLD によってスムージングする(「ている」の突出した頻度によって、「していかない」より「しているない」が優先されないように)
                int tmpCnt = sysCount + rtmCount * REALTIME_FREQ_BOOST_FACTOR;
                sysCount = tmpCnt > SYSTEM_NGRAM_COUNT_THRESHOLD ? SYSTEM_NGRAM_COUNT_THRESHOLD + (int)(std::sqrt(tmpCnt - 1000) * REALTIME_FREQ_BOOST_FACTOR) : tmpCnt;
                rtmCount = 0;
            }
            int bonumFactor = SETTINGS->realtimeTrigramBonusFactor;
            int numTier1 = SETTINGS->realtimeTrigramTier1Num;
            if (numTier1 > rtmCount) numTier1 = rtmCount;
            int numTier2 = SETTINGS->realtimeTrigramTier2Num;
            if (numTier1 + numTier2 > rtmCount) numTier2 = rtmCount - numTier1;
            int maxTier3 = 100 - (numTier1 + numTier2);
            if (maxTier3 < 0) maxTier3 = 0;
            int numTier3 = rtmCount - (numTier1 + numTier2);
            if (numTier3 < 0) numTier3 = 0;
            if (numTier3 > maxTier3) numTier3 = maxTier3;
            int numTier4 = rtmCount - (numTier1 + numTier2 + numTier3);
            if (numTier4 < 0) numTier4 = 0;

            int bonusTier1 = numTier1 * (bonumFactor * 10);
            int bonusTier2 = numTier2 * bonumFactor;
            int bonusTier3 = (maxTier3 > 0 && numTier3 > 0 ? (int)(numTier3 * bonumFactor * (1.0 - ((float)numTier3 / (maxTier3 * 2)))) : 0);
            int bonusTier4 = numTier4 + sysCount;
            int bonusTotal = bonusTier1 + bonusTier2 + bonusTier3 + bonusTier4;
            ngramCosts[word] = -bonusTotal;

            if (IS_LOG_DEBUGH_ENABLED) {
                String ws = to_wstr(word);
                if (ws == L"ている" || ws == L"いるな" || ws == L"ていか" || ws == L"いかな") {
                    LOG_WARNH(L"word={}: sysCount adjusted: sysCnt={}, totalCost={}", ws, sysCount, bonusTotal);
                }
                if (ngramCosts[word] < -(SETTINGS->realtimeTrigramTier1Num * bonumFactor * 10 + 1000)) {
                    LOG_WARN(L"HIGHER COST: word={}, cost={}, sysCount={}, usrCount={}, rtmCount={}, tier1=({}:{}), tier2=({}:{}), tier3=({}:{}), tier4=({}:{})",
                        to_wstr(word), ngramCosts[word], origSysCnt, usrCount, origRtmCnt, numTier1, bonusTier1, numTier2, bonusTier2, numTier3, bonusTier3, numTier4, bonusTier4);
                }
            }
            //// 上のやり方は、間違いの方の影響も拡大してしまうので、結局、あまり意味が無いと思われる
            //ngramCosts[word] = -(rtmCount * bonumFactor + (sysCount + usrCount) * (bonumFactor/10));
        //} else if (word.size() >= 4 && rtmCount < 0) {
        //    ngramCosts[word] = (-rtmCount) * 100;
        }
    }

    // オンラインでのNgram更新
    void _updateRealtimeNgramByWord(const MString& word) {
        int count = realtimeNgram[word];
        if (count == 0 && word.length() >= 2 &&
            userNgram.find(word) == userNgram.end() && systemNgram.find(word) == systemNgram.end() &&
            (all_pure_katakana(word) || contains_half_or_more_kanji(word))) {
            // 未だどこにも登録されていない2文字以上の素片で、全部カタカナか半分以上漢字を含むものは、ティア1として登録
            count = SETTINGS->realtimeTrigramTier1Num;
            LOG_WARNH(L"TIER1 Ngram: {}, count={}", to_wstr(word), count);
        } else {
            ++count;
        }
        realtimeNgram[word] = count;
        _updateNgramCost(word, 0, 0, count);
        realtimeNgram_updated = true;
    }

    void _raiseRealtimeNgramByWord(const MString& word, bool bJust2ByGUI = false) {
        LOG_WARNH(L"CALLED: word={}, just2ByGUI={}", to_wstr(word), bJust2ByGUI);
        int count = SETTINGS->realtimeTrigramTier1Num;
        if (word.length() >= 4) {
            ////LOG_WARNH(L"CALLED: word={}", to_wstr(word));
            //auto iter = realtimeNgram.find(word);
            //if (iter != realtimeNgram.end()) {
            //    if (iter->second < 0) iter->second = 0;
            //    else ++(iter->second);
            //    LOG_WARNH(L"RAISE: word={} count={}", to_wstr(word), iter->second);
            //}
        } else if (word.length() == 3) {
            int c = realtimeNgram[word] + 10;
            if (c > count) count = c;
            realtimeNgram[word] = count;
        } else if (word.length() == 2) {
            count = realtimeNgram[word];
            if (count < 0) count = 0;
            else count += bJust2ByGUI ? 1000 : 10;
            realtimeNgram[word] = count;
        }
        _updateNgramCost(word, 0, 0, count);
        realtimeNgram_updated = true;
    }

    void _depressRealtimeNgramByWord(const MString& word, bool bJust2ByGUI = false) {
        int count = 0;
        if (word.length() >= 4) {
            // 4gramの抑制をやってみたが、いまいちなので採用しない(2025/2/4)
            //LOG_WARNH(L"CALLED: word={}", to_wstr(word));
            //count = -SETTINGS->realtimeTrigramTier1Num;
            //if (realtimeNgram[word] > count) {
            //    realtimeNgram[word] = count;
            //}
        } else if (word.length() == 3) {
            count = realtimeNgram[word] - 10;
            if (count < 0) count = 0;
            realtimeNgram[word] = count;
        } else if (word.length() == 2) {
            count = realtimeNgram[word];
            if (count > 0) count = 0;
            else count -= bJust2ByGUI ? 1000 : 10;
            realtimeNgram[word] = count;
        }
        LOG_WARNH(L"CALLED: _updateNgramCost(word={}, rtmCnt={}), just2ByGUI", to_wstr(word), count, bJust2ByGUI);
        _updateNgramCost(word, 0, 0, count);
        realtimeNgram_updated = true;
    }

    // ngramCosts の初期作成
    void makeInitialNgramCostMap() {
        ngramLogFactor = (int)((DEFAULT_MAX_COST - 50) / std::log(systemMaxFreq + userMaxFreq + realtimeMaxFreq * REALTIME_FREQ_BOOST_FACTOR));
        _LOG_DETAIL(L"ENTER: systemMaxFreq={}, realtimeMaxFreq={}, userMaxFreq={}, ngramLogFactor={}", systemMaxFreq, realtimeMaxFreq, userMaxFreq, ngramLogFactor);
        ngramCosts.clear();
        for (auto iter = systemNgram.begin(); iter != systemNgram.end(); ++iter) {
            const MString& word = iter->first;
            int sysCount = iter->second;
            auto iterOnl = realtimeNgram.find(word);
            int rtmCount = iterOnl != realtimeNgram.end() ? iterOnl->second : 0;
            auto iterUsr = userNgram.find(word);
            int usrCount = iterUsr != userNgram.end() ? iterUsr->second : 0;
            _updateNgramCost(word, sysCount, usrCount, rtmCount);
        }
        for (auto iter = userNgram.begin(); iter != userNgram.end(); ++iter) {
            const MString& word = iter->first;
            int usrCount = iter->second;
            if (usrCount == 0 || (usrCount < 0 && word.size() == 1)) {
                // カウント(orコスト)が 0 のもの、1文字で負のカウント(orコスト)のものは無視
                continue;
            }
            if (ngramCosts.find(word) == ngramCosts.end()) {
                // 未登録
                auto iterOnl = realtimeNgram.find(word);
                int rtmCount = iterOnl != realtimeNgram.end() ? iterOnl->second : 0;
                _updateNgramCost(word, 0, usrCount, rtmCount);
            }
        }
        // 下記(SystemNgramに含まれないNgramを優先する)は悪影響が大き過ぎるので、止めておく(「い朝です」の「い朝」のボーナスが大きくなりすぎて、「ないからです」が出なくなる)
        // ⇒と思ったが、問題なさそうなので復活
        for (auto iter = realtimeNgram.begin(); iter != realtimeNgram.end(); ++iter) {
            const MString& word = iter->first;
            if (word.size() < 4) {
                int rtmCount = iter->second;
                if (ngramCosts.find(word) == ngramCosts.end()) {
                    // 未登録
                    _updateNgramCost(word, 0, 0, rtmCount);
                }
            }
        }
        for (auto iter = KatakanaWordCosts.begin(); iter != KatakanaWordCosts.end(); ++iter) {
            const MString& word = iter->first;
            int cost = iter->second;
            if (cost < ngramCosts[word]) {
                // update
                ngramCosts[word] = cost;
            }
        }
        _LOG_DETAIL(L"LEAVE: ngramCosts.size={}", ngramCosts.size());
    }

#define SYSTEM_NGRAM_FILE L"files/mixed_all.ngram.txt"
#define KATAKANA_COST_FILE  L"files/katakana.cost.txt"
#define REALTIME_NGRAM_MAIN_FILE L"files/realtime.ngram.txt"
#define REALTIME_NGRAM_TEMP_FILE L"files/realtime.ngram.tmp.txt"
//#define USER_NGRAM_FILE    L"files/user.ngram.txt"
#define USER_COST_FILE    L"files/userword.cost.txt"

    int _loadNgramFile(StringRef ngramFile, std::map<MString, int>& ngramMap) {
        auto path = utils::joinPath(SETTINGS->rootDir, ngramFile);
        LOG_INFO(_T("LOAD: {}"), path.c_str());
        int maxFreq = 0;
        utils::IfstreamReader reader(path);
        if (reader.success()) {
            ngramMap.clear();
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(utils::replace_all(utils::strip(line), L" +", L"\t"), '\t');
                if (items.size() == 2 &&
                    items[0].size() >= 1 && items[0].size() <= 3 &&         // 1-3gramに限定
                    items[0][0] != L'#' && isDecimalString(items[1])) {

                    int count = std::stoi(items[1]);
                    MString word = to_mstr(items[0]);
                    ngramMap[word] = count;
                    if (maxFreq < count) maxFreq = count;
                    if (IS_LOG_DEBUGH_ENABLED) {
                        String ws = to_wstr(word);
                        if (ws == L"ている" || ws == L"いるな" || ws == L"ていか" || ws == L"いかな") {
                            LOG_WARNH(L"file={}, word={}, count={}", ngramFile, ws, count);
                        }
                    }
                }
            }
        }
        LOG_INFO(_T("DONE: maxFreq={}"), maxFreq);
        return maxFreq;
    }

    void _loadKatakanaCostFile() {
        auto path = utils::joinPath(SETTINGS->rootDir, KATAKANA_COST_FILE);
        LOG_INFO(_T("LOAD: {}"), path.c_str());
        utils::IfstreamReader reader(path);
        if (reader.success()) {
            KatakanaWordCosts.clear();
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(utils::replace_all(utils::strip(utils::reReplace(line, L"#.*$", L"")), L"[ \t]+", L"\t"), '\t');
                if (!items.empty() && !items[0].empty() && items[0][0] != L'#') {
                    MString word = to_mstr(items[0]);
                    if (word.size() >= 2) {
                        // bigram以上のみ
                        if (items.size() >= 2 && isDecimalString(items[1])) {
                            KatakanaWordCosts[word] = std::stoi(items[1]) - DEFAULT_WORD_BONUS;
                        }
                    }
                }
            }
        }
    }

    void __loadUserCostFile(StringRef file) {
        auto path = utils::joinPath(SETTINGS->rootDir, file);
        LOG_INFO(_T("LOAD: {}"), path.c_str());
        utils::IfstreamReader reader(path);
        if (reader.success()) {
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(utils::replace_all(utils::strip(utils::reReplace(line, L"#.*$", L"")), L"[ \t]+", L"\t"), '\t');
                if (!items.empty() && !items[0].empty() && items[0][0] != L'#') {
                    MString word = to_mstr(items[0]);
                    if (word.size() == 1) {
                        // unigram
                        int count = DEFAULT_UNIGRAM_BONUS_COUNT;
                        if (items.size() >= 2 && isDecimalString(items[1])) {
                            count = std::stoi(items[1]);
                        }
                        if (count >= 0) {
                            userNgram[word] = count;
                            if (userMaxFreq < count) userMaxFreq = count;
                        } else {
                            userWordCosts[word] = count;    // 負数はコストとして扱う
                        }
                    } else {
                        if (items.size() >= 2 && isDecimalString(items[1])) {
                            userWordCosts[word] = std::stoi(items[1]);
                        } else {
                            userWordCosts[word] = -DEFAULT_WORD_BONUS;       // userword.cost のデフォルトは -DEFAULT_WORD_BONUS
                        }
                    }
                }
            }
        }
    }

    void _loadUserCostFile() {
        userWordCosts.clear();
        userMaxFreq = 0;
        __loadUserCostFile(USER_COST_FILE);
        //__loadUserCostFile(USER_NGRAM_FILE);
    }

#define REALTIME_NGRAM_FILE (SETTINGS->useTmpRealtimeNgramFile ? REALTIME_NGRAM_TEMP_FILE : REALTIME_NGRAM_MAIN_FILE)

    void loadCostAndNgramFile(bool systemNgramFile = true, bool realtimeNgramFile = true) {
        LOG_INFO(L"ENTER: systemNgramFile={}, realtimeNgramFile={}", systemNgramFile, realtimeNgramFile);
#ifndef _DEBUG
        if (systemNgramFile) {
            systemMaxFreq = _loadNgramFile(SYSTEM_NGRAM_FILE, systemNgram);
            //int userMaxFreq = _loadNgramFile(USER_NGRAM_FILE, systemNgram);
            //if (systemMaxFreq < userMaxFreq) systemMaxFreq = userMaxFreq;
        }
        if (realtimeNgramFile) {
            LOG_INFO(L"LOAD: realtime ngram file={}", REALTIME_NGRAM_FILE);
            realtimeMaxFreq = _loadNgramFile(REALTIME_NGRAM_FILE, realtimeNgram);
            if (realtimeMaxFreq <= 0 && SETTINGS->useTmpRealtimeNgramFile) {
                realtimeMaxFreq = _loadNgramFile(REALTIME_NGRAM_MAIN_FILE, realtimeNgram);
            }
            _loadKatakanaCostFile();
        }
#endif
        _loadUserCostFile();
        makeInitialNgramCostMap();

        maxCandidatesSize = 0;
        LOG_INFO(L"LEAVE");
    }

    void saveRealtimeNgramFile() {
        LOG_INFO(L"CALLED: file={}, realtimeNgram_updated={}", REALTIME_NGRAM_FILE, realtimeNgram_updated);
#ifndef _DEBUG
        auto path = utils::joinPath(SETTINGS->rootDir, REALTIME_NGRAM_FILE);
        if (realtimeNgram_updated) {
            if (utils::moveFileToBackDirWithRotation(path, SETTINGS->backFileRotationGeneration)) {
                LOG_INFO(_T("SAVE: realtime ngram file path={}"), path.c_str());
                utils::OfstreamWriter writer(path);
                if (writer.success()) {
                    for (const auto& pair : realtimeNgram) {
                        String line;
                        int count = pair.second;
                        if (count < 0 || count > 1 || (count == 1 && Reporting::Logger::IsWarnEnabled())) {
                            // count が 0 または 1 の N-gramは無視する
                            line.append(to_wstr(pair.first));           // 単語
                            line.append(_T("\t"));
                            line.append(std::to_wstring(pair.second));  // カウント
                            writer.writeLine(utils::utf8_encode(line));
                        }
                    }
                    realtimeNgram_updated = false;
                }
                LOG_INFO(_T("DONE: entries count={}"), realtimeNgram.size());
            }
        }
#endif
    }

    inline bool is_space_or_vbar(mchar_t ch) {
        return ch == ' ' || ch == '|';
    }

    void updateRealtimeNgram(const MString& str) {
        _LOG_DETAIL(L"CALLED: str={}, collectRealtimeNgram={}", to_wstr(str), SETTINGS->collectRealtimeNgram);
        if (!SETTINGS->collectRealtimeNgram) return;

        int strlen = (int)str.size();
        for (int pos = 0; pos < strlen; ++pos) {
            //// 1gramなら漢字以外は無視
            //if (utils::is_kanji(str[pos])) {
            //    _updateRealtimeNgramByWord(str.substr(pos, 1));
            //}

            if (!utils::is_japanese_char_except_nakaguro(str[pos])) continue;
            // 1-gram
            _updateRealtimeNgramByWord(str.substr(pos, 1));

            if (pos + 1 >= strlen || !utils::is_japanese_char_except_nakaguro(str[pos + 1])) continue;
            // 2-gram
            _updateRealtimeNgramByWord(str.substr(pos, 2));

            if (pos + 2 >= strlen || !utils::is_japanese_char_except_nakaguro(str[pos + 2])) continue;
            // 3-gram
            _updateRealtimeNgramByWord(str.substr(pos, 3));

            //if (pos + 3 >= strlen || !utils::is_japanese_char_except_nakaguro(str[pos + 3])) continue;
            //// 4-gram
            //_raiseRealtimeNgramByWord(str.substr(pos, 4));
        }
    }

    // リアルタイムNgramの蒿上げ
    void raiseRealtimeNgram(const MString& str, bool bByGUI = false) {
        LOG_DEBUGH(L"CALLED: str={}", to_wstr(str));
        int strlen = (int)str.size();
        for (int pos = 0; pos < strlen; ++pos) {
            if (!utils::is_japanese_char_except_nakaguro(str[pos])) continue;

            if (pos + 1 >= strlen || !utils::is_japanese_char_except_nakaguro(str[pos + 1])) continue;
            // 2-gram
            _raiseRealtimeNgramByWord(str.substr(pos, 2), bByGUI && strlen == 2);

            if (pos + 2 >= strlen || !utils::is_japanese_char_except_nakaguro(str[pos + 2])) continue;
            // 3-gram
            _raiseRealtimeNgramByWord(str.substr(pos, 3));

            //if (pos + 3 >= strlen || !utils::is_japanese_char_except_nakaguro(str[pos + 3])) continue;
            //// 4-gram
            //_raiseRealtimeNgramByWord(str.substr(pos, 4));
        }
    }

    // リアルタイムNgramの抑制
    void depressRealtimeNgram(const MString& str, bool bByGUI = false) {
        LOG_DEBUGH(L"CALLED: str={}", to_wstr(str));
        int strlen = (int)str.size();
        for (int pos = 0; pos < strlen; ++pos) {
            if (!utils::is_japanese_char_except_nakaguro(str[pos])) continue;

            if (pos + 1 >= strlen || !utils::is_japanese_char_except_nakaguro(str[pos + 1])) continue;
            // 2-gram
            _depressRealtimeNgramByWord(str.substr(pos, 2), bByGUI && strlen == 2);

            if (pos + 2 >= strlen || !utils::is_japanese_char_except_nakaguro(str[pos + 2])) continue;
            // 3-gram
            _depressRealtimeNgramByWord(str.substr(pos, 3));

            //if (pos + 3 >= strlen || !utils::is_japanese_char_except_nakaguro(str[pos + 3])) continue;
            //// 4-gram
            //_depressRealtimeNgramByWord(str.substr(pos, 4));
        }
    }

    // 候補選択による、リアルタイムNgramの蒿上げと抑制
    void _raiseAndDepressRealtimeNgramForDiffPart(const MString& oldCand, const MString& newCand) {
        LOG_WARNH(L"CALLED: oldCand={}, newCand={}", to_wstr(oldCand), to_wstr(newCand));
        size_t prefixLen = utils::commonPrefixLength(oldCand, newCand);
        if (prefixLen < newCand.size()) {
            for (int delta = 2; delta > 0; --delta) {
                // 異なっている部分の2文字前のところから、3gramについて蒿上げをする
                if (prefixLen >= (size_t)delta) {
                    size_t pos = prefixLen - delta;
                    if (pos + 2 < newCand.size()) _raiseRealtimeNgramByWord(newCand.substr(pos, 3));
                    //if (pos + 3 < newCand.size()) _depressRealtimeNgramByWord(newCand.substr(pos, 4));
                }
            }
            // 異なっている部分について蒿上げする
            raiseRealtimeNgram(newCand.substr(prefixLen));
        }
        if (prefixLen < oldCand.size()) {
            for (int delta = 2; delta > 0; --delta) {
                // 異なっている部分の2文字前のところから、3gramについて抑制をする
                if (prefixLen >= (size_t)delta) {
                    size_t pos = prefixLen - delta;
                    if (pos + 2 < oldCand.size()) _depressRealtimeNgramByWord(oldCand.substr(pos, 3));
                    //if (pos + 3 < oldCand.size()) _depressRealtimeNgramByWord(oldCand.substr(pos, 4));
                }
            }
            // 異なっている部分について抑制する
            depressRealtimeNgram(oldCand.substr(prefixLen));
        }
    }

    //void updateRealtimeNgram() {
    //    updateRealtimeNgram(OUTPUT_STACK->backStringUptoPunctWithFlag());
    //}

    // 2～4gramに対する利用者定義コストを計算
    int getUserWordCost(const MString& word) {
        auto iter = userWordCosts.find(word);
        if (iter != userWordCosts.end()) {
            int xCost = iter->second;
            // 利用者が定義したコストはそのまま返す
            _LOG_DETAIL(L"{}: userWordCost={}", to_wstr(word), xCost);
            return xCost;
        }
        return 0;
    }

    // 1～2gramに対する(ポジティブ)コストを計算
    int get_base_ngram_cost(const MString& word) {
        if (word.empty()) return 0;
        if (word.size() == 1 && word == MSTR_SPACE) return ONE_SPACE_COST;          // 1 space の場合のコスト
        //int cost = word.size() == 2 ? getUserWordCost(word) : 0;
        int cost = getUserWordCost(word);
        if (cost != 0) return cost;
        auto iter = ngramCosts.find(word);
        cost = iter != ngramCosts.end() ? iter->second : DEFAULT_MAX_COST;
        if (cost < -100000) {
            _LOG_DETAIL(L"ABNORMAL COST: word={}, cost={}", to_wstr(word), cost);
        }
        return cost;
    }

    // 2～4gramに対する追加(ネガティブ)コストを計算
    int getExtraNgramCost(const MString& word) {
        int xCost = getUserWordCost(word);
        if (xCost == 0 && word.size() > 2) {
            auto iter = ngramCosts.find(word);
            if (iter != ngramCosts.end()) {
                // システムによるNgramのコストはネガティブコスト
                xCost = iter->second;
                // 正のコストが設定されている場合(wikipedia.costなど)は、 DEFAULT BONUS を引いたコストにする; つまり負のコストになる
                if (xCost > 0 && xCost < DEFAULT_WORD_BONUS) xCost -= DEFAULT_WORD_BONUS;
                _LOG_DETAIL(L"{}: ngramCosts={}", to_wstr(word), xCost);
                //if (to_wstr(word) == L"れた々") { LOG_WARNH(L"word={}: ngramCosts={}", to_wstr(word), xCost); }
            }
        } else if (xCost < 0) {
            _LOG_DETAIL(L"{}: userWordCost={}", to_wstr(word), xCost);
        }
        return xCost;
    }

    //int getWordConnCost(const MString& s1, const MString& s2) {
    //    return get_base_ngram_cost(utils::last_substr(s1, 1) + utils::safe_substr(s2, 0, 1)) / 2;
    //}

    int findKatakanaLen(const MString& s, size_t pos) {
        int len = 0;
        for (; (size_t)(pos + len) < s.size(); ++len) {
            if (!utils::is_katakana(s[pos + len])) break;
        }
        return len;
    }

    int getKatakanaStringCost(const MString& str, size_t pos, int katakanaLen) {
        int restlen = katakanaLen;
        auto katakanaWord = to_wstr(utils::safe_substr(str, pos, katakanaLen));
        int xCost = 0;
        int totalCost = 0;
        while (restlen > 5) {
            // 6->4:3, 7->4:4, 8->4:3:3, 9->4:4:3, 10->4:4:4, ...
            auto word = utils::safe_substr(str, pos, 4);
            xCost = getExtraNgramCost(word);
            _LOG_DETAIL(L"KATAKANA: extraWord={}, xCost={}", to_wstr(word), xCost);
            if (xCost == 0) {
                word = utils::safe_substr(str, pos, 3);
                xCost = getExtraNgramCost(word);
                _LOG_DETAIL(L"KATAKANA: extraWord={}, xCost={}", to_wstr(word), xCost);
            }
            if (xCost == 0) {
                totalCost += xCost;
                _LOG_DETAIL(L"FOUND: katakana={}, totalCost={}", katakanaWord, totalCost);
            }
            pos += word.size() - 1;
            restlen -= word.size() - 1;
        }
        if (restlen == 5) {
            // 5->3:3
            auto word = utils::safe_substr(str, pos, 3);
            xCost = getExtraNgramCost(word);
            _LOG_DETAIL(L"KATAKANA: extraWord={}, xCost={}", to_wstr(word), xCost);
            totalCost += xCost;
            _LOG_DETAIL(L"FOUND: katakana={}, totalCost={}", katakanaWord, totalCost);
            pos += 2;
            restlen -= 2;
        }
        if (restlen == 4) {
            auto word = utils::safe_substr(str, pos, restlen);
            xCost = getExtraNgramCost(word);
            _LOG_DETAIL(L"KATAKANA: extraWord={}, xCost={}", to_wstr(word), xCost);
            if (xCost != 0) {
                totalCost += xCost;
                _LOG_DETAIL(L"FOUND: katakana={}, totalCost={}", katakanaWord, totalCost);
                pos += 3;
                restlen -= 3;
            } else {
                word = utils::safe_substr(str, pos, 3);
                xCost = getExtraNgramCost(word);
                _LOG_DETAIL(L"KATAKANA: extraWord={}, xCost={}", to_wstr(word), xCost);
                if (xCost != 0) {
                    totalCost += xCost;
                    _LOG_DETAIL(L"FOUND: katakana={}, totalCost={}", katakanaWord, totalCost);
                    pos += 1;
                    restlen -= 1;
                }
            }
        }
        if (restlen == 3) {
            auto word = utils::safe_substr(str, pos, restlen);
            xCost = getExtraNgramCost(word);
            _LOG_DETAIL(L"KATAKANA: extraWord={}, xCost={}", to_wstr(word), xCost);
            if (xCost != 0) {
                totalCost += xCost;
                _LOG_DETAIL(L"FOUND: katakana={}, totalCost={}", katakanaWord, totalCost);
            }
        }
        return totalCost;
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

    MString substringBetweenPunctuations(const MString& str) {
        int endPos = (int)(str.size());
        // まず末尾の句読点をスキップ
        for (; endPos > 0; --endPos) {
            if (!utils::is_punct_or_commit_char(str[endPos - 1])) break;
        }
        // 前方に向かって句読点を検索
        int startPos = endPos - 1;
        for (; startPos > 0; --startPos) {
            if (utils::is_punct_or_commit_char(str[startPos - 1])) break;
        }
        return utils::safe_substr(str, startPos, endPos - startPos);
    }

    MString substringBetweenNonJapaneseChars(const MString& str) {
        int endPos = (int)(str.size());
        // まず末尾の非日本語文字をスキップ
        for (; endPos > 0; --endPos) {
            if (!isCommitChar(str[endPos - 1])) break;
        }
        // 前方に向かって非日本語文字を検索
        int startPos = endPos - 1;
        for (; startPos > 0; --startPos) {
            if (isCommitChar(str[startPos - 1])) break;
        }
        return utils::safe_substr(str, startPos, endPos - startPos);
    }

#if 1
    // Ngramコストの取得
    int getNgramCost(const MString& str, const std::vector<MString>& morphs) {
        _LOG_DETAIL(L"ENTER: str={}", to_wstr(str));
        int cost = 0;
        size_t strLen = str.size();

        if (strLen <= 0) return 0;

        std::vector<size_t> wordBoundaries;
        wordBoundaries.push_back(0);
        for (const auto& m : morphs) {
            size_t p = m.find('\t');
            if (p > m.size()) p = m.size();
            p += wordBoundaries.back();
            wordBoundaries.push_back(p);
        }

        // unigram
        for (size_t i = 0; i < strLen; ++i) {
            if (isSmallKatakana(str[i])) {
                if ((i == 0 || !utils::is_katakana(str[i - 1]))) {
                    // 小書きカタカナの直前にカタカナが無ければ、ペナルティを与える
                    cost += ISOLATED_SMALL_KATAKANA_COST;
                    continue;
                }
            } else if (utils::is_pure_katakana(str[i])) {
                if ((i == 0 || !utils::is_katakana(str[i - 1])) && (i == strLen - 1 || !utils::is_katakana(str[i + 1]))) {
                    // 孤立したカタカナなら、ペナルティを与える
                    cost += ISOLATED_KATAKANA_COST;
                    continue;
                }
            //} else if ((i == 0 || !utils::is_hiragana(str[i - 1])) && strLen == i + 2 && isHighFreqJoshi(str[i]) && utils::is_hiragana(str[i + 1])) {
                //// 先頭または漢字・カタカナの直後の2文字のひらがなで、1文字目が高頻度の助詞(が、を、に、の、で、は)なら、ボーナスを与付して、ひらがな2文字になるようにする
                // こちらはいろいろと害が多い(からです⇒朝です、食べさせると青⇒食べ森書がの、など)
            //} else if (i == 0 && strLen >= 2 && isHighFreqJoshi(str[0]) && !utils::is_hiragana(str[1])) {
            //    // 1文字目が高頻度の助詞(と、を、に、の、で、は、が)で、2文字目がひらがな以外なら、ボーナスを付与
            //    cost -= HEAD_HIGH_FREQ_JOSHI_BONUS;
                // 「がよい」より「が関」が優先されてしまう
            }
            // 通常の unigram コストの計上
            cost += get_base_ngram_cost(utils::safe_substr(str, i, 1));
        }
        _LOG_DETAIL(L"Unigram cost={}", cost);

        if (strLen == 1) {
            if (utils::is_kanji(str[0])) {
                cost += ONE_KANJI_COST;    // 文字列が漢字１文字の場合のコスト
                _LOG_DETAIL(L"Just one Kanji (+{}): cost={}", ONE_KANJI_COST, cost);
            }
        } else if (strLen > 1) {
            // bigram
            //int lastKanjiPos = -1;
            auto wbIter = wordBoundaries.begin();
            size_t wbBegin = 0;
            size_t wbEnd = 0;
            int biCost = 0;
            for (size_t i = 0; i < strLen - 1; ++i) {
                if (i >= wbEnd) {
                    wbBegin = wbEnd;
                    ++wbIter;
                    wbEnd = wbIter != wordBoundaries.end() ? *wbIter : strLen;
                }

                // 通常の bigram コストの計上
                int divider = 1;    // コスト減少のためのディバイダー
                if (wbEnd - wbBegin > 2 && (i >= wbBegin && i + 2 <= wbEnd)) {
                    // 形態素内に収まる場合は、bigramコストを減少させる
                    divider = wbEnd - wbBegin - 1;
                }
                biCost += get_base_ngram_cost(utils::safe_substr(str, i, 2)) / divider;

                //// 漢字が2文字連続する場合のボーナス
                //if (i > lastKanjiPos && utils::is_kanji(str[i]) && utils::is_kanji(str[i + 1])) {
                //    cost -= KANJI_CONSECUTIVE_BONUS;
                //    lastKanjiPos = i + 1;   // 漢字列が3文字以上続くときは、重複計上しない
                //}

                //if (utils::is_katakana(str[i]) && utils::is_katakana(str[i + 1])) {
                //    // カタカナが2文字連続する場合のボーナス
                //    cost -= KATAKANA_CONSECUTIVE_BONUS;
                //    int katakanaLen = findKatakanaLen(str, i);
                //    if (katakanaLen > 0) {
                //        // カタカナ連なら、次の文字種までスキップ
                //        i += katakanaLen - 1;
                //        continue;
                //    }
                //}

                //if ((utils::is_kanji(str[i]) && str[i + 1] == CHOON) || (str[i] == CHOON && utils::is_kanji(str[i + 1]))) {
                //    // 漢字と「ー」の隣接にはペナルティ
                //    cost += KANJI_CONSECUTIVE_PENALTY;
                //}
            }
            cost += biCost;
            _LOG_DETAIL(L"Bigram cost={}, total={}", biCost, cost);

            if (strLen > 2) {
                // trigram
                size_t i = 0;
                while (i < strLen - 2) {
                    // 末尾に3文字以上残っている
                    //bool found = false;
                    int katakanaLen = findKatakanaLen(str, i);
                    if (katakanaLen >= 3) {
                        // カタカナの3文字以上の連続
                        cost += getKatakanaStringCost(str, i, katakanaLen);
                        // カタカナ連の末尾に飛ばす
                        i += katakanaLen - 1;
                        continue;
                        //found = true;
                    }
                    if (i < strLen - 3) {
                        // 4文字連
                        //if (TAIL_HIRAGANA_LEN >= 4 && (i == strLen - TAIL_HIRAGANA_LEN) && utils::is_hiragana_str(utils::safe_substr(str, i, TAIL_HIRAGANA_LEN))) {
                        //    // 末尾がひらがな4文字連続の場合のボーナス
                        //    cost -= TAIL_HIRAGANA_BONUS;
                        //    _LOG_DETAIL(L"TAIL HIRAGANA:{}, cost={}", to_wstr(utils::safe_substr(str, i, TAIL_HIRAGANA_LEN)), cost);
                        //    break;
                        //}
                        int len = 4;
                        auto word = utils::safe_substr(str, i, len);
                        _LOG_DETAIL(L"len={}, extraWord={}", len, to_wstr(word));
                        int xCost = getExtraNgramCost(word);
                        if (xCost != 0) {
                            // コスト定義がある
                            cost += xCost;
                            _LOG_DETAIL(L"FOUND: extraWord={}, xCost={}, total={}", to_wstr(word), xCost, cost);
                            //i += len - 1;
                            //continue;
                            //found = true;
                        }
                    }
                    {
                        // 3文字連
                        int len = 3;
                        auto word = utils::safe_substr(str, i, len);
                        _LOG_DETAIL(L"len={}, extraWord={}", len, to_wstr(word));
                        int xCost = getExtraNgramCost(word);
                        if (xCost != 0) {
                            // コスト定義がある
                            cost += xCost;
                            _LOG_DETAIL(L"FOUND: extraWord={}, xCost={}, total={}", to_wstr(word), xCost, cost);
                            // 次の位置へ
                            //i += len;
                            //found = true;
                        }
                    }
#if 0
                    {
                        // 3文字連
                        // 「漢字+の+漢字」のような場合はボーナス
                        if (SETTINGS->kanjiNoKanjiBonus > 0) {
                            if ((str[i + 1] == L'が' || str[i + 1] == L'の' /* || str[i + 1] == L'で' */ || str[i + 1] == L'を') && !utils::is_hiragana(str[i]) && !utils::is_hiragana(str[i + 2])) {
                                cost -= KANJI_NO_KANJI_BONUS;
                                _LOG_DETAIL(L"KANJI-NO-KANJI:{}, cost={}", to_wstr(utils::safe_substr(str, i, 3)), cost);
                            }
                        }
                    }
#endif
                    ++i;
                }
            }
        }
        _LOG_DETAIL(L"LEAVE: cost={}, adjusted cost={} (* NGRAM_COST_FACTOR({}))", cost, cost* SETTINGS->ngramCostFactor, SETTINGS->ngramCostFactor);
        return cost;
    }
#else
    int getNgramCost(const MString& str) {
        int cost = 0;
        for (size_t i = 0; i < str.size() - 1; ++i) {
            int bigramCost = get_base_ngram_cost(utils::safe_substr(str, i, 2));
            if (bigramCost >= DEFAULT_MAX_COST) {
                bigramCost = get_base_ngram_cost(utils::safe_substr(str, i, 1)) + get_base_ngram_cost(utils::safe_substr(str, i + 1, 1));
            }
            cost += bigramCost;
        }
        return cost;
    }
#endif

#define GLOBAL_POST_REWRITE_FILE    L"files/global-post-rewrite-map.txt"

    // グローバルな後置書き換えマップファイルの読み込み
    void readGlobalPostRewriteMapFile() {
        globalPostRewriteMap.clear();
        auto path = utils::joinPath(SETTINGS->rootDir, GLOBAL_POST_REWRITE_FILE);
        LOG_INFO(_T("LOAD: {}"), path.c_str());
        utils::IfstreamReader reader(path);
        int count = 0;
        if (reader.success()) {
            LOG_INFO(_T("FOUND: {}"), path.c_str());
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(utils::replace_all(utils::strip(line), L"[>: ]+", L"\t"), '\t');
                if (items.size() == 2 &&
                    items[0].size() >= 1 && items[1].size() >= 1 &&
                    items[0][0] != L'#' && items[0][0] != L';') {

                   globalPostRewriteMap[to_mstr(items[0])] = to_mstr(items[1]);
                   ++count;
                }
            }
        }
        LOG_INFO(_T("LEAVE: count={}"), count);
    }

    // 候補文字列
    class CandidateString {
        MString _str;
        int _strokeLen;
        int _cost;
        int _penalty;
        bool _isNonTerminal;
        //float _llama_loss = 0.0f;

        // 末尾文字列にマッチする RewriteInfo を取得する
        std::tuple<const RewriteInfo*, int> matchWithTailString(const PostRewriteOneShotNode* rewriteNode) const {
            size_t maxlen = SETTINGS->kanaTrainingMode && ROOT_STROKE_NODE->hasOnlyUsualRewriteNdoe() ? 0 : 8;     // かな入力練習モードで濁点のみなら書き換えをやらない
            bool bRollOverStroke = STATE_COMMON->IsRollOverStroke();
            _LOG_DETAIL(_T("bRollOverStroke={}"), bRollOverStroke);
            while (maxlen > 0) {
                LOG_DEBUGH(_T("maxlen={}"), maxlen);
                const MString targetStr = utils::safe_tailstr(_str, maxlen);
                LOG_DEBUGH(_T("targetStr={}"), to_wstr(targetStr));
                if (targetStr.empty()) break;

                const RewriteInfo* rewInfo = 0;
                if (bRollOverStroke) rewInfo = rewriteNode->getRewriteInfo(targetStr + MSTR_PLUS);        // ロールオーバー打ちのときは"+"を付加したエントリを検索
                if (!rewInfo) rewInfo = rewriteNode->getRewriteInfo(targetStr);
                if (rewInfo) {
                    LOG_DEBUGH(_T("REWRITE_INFO found: outStr={}, rewritableLen={}, subTable={:p}"), to_wstr(rewInfo->rewriteStr), rewInfo->rewritableLen, (void*)rewInfo->subTable);
                    return { rewInfo, (int)targetStr.size() };
                }

                maxlen = targetStr.size() - 1;
            }
            return { 0, 0 };
        }

        // 複数文字が設定されているストロークを1文字ずつに分解
        std::vector<MString> split_piece_str(const MString& s) const {
            if (s.size() == 1 && s.front() == '|') {
                return std::vector<MString>(1, s);
            } else {
                return utils::split(s, '|');
            }
        }

        MString convertoToKatakanaIfAny(const MString& str, bool bKatakana) const {
            MString result = str;
            if (bKatakana) {
                for (size_t i = 0; i < result.size(); ++i) {
                    mchar_t ch = (wchar_t)result[i];
                    if (utils::is_hiragana(ch)) { result[i] = utils::hiragana_to_katakana(ch); }
                }
            }
            return result;
        }

        // グローバルな後置書き換えを適用する
        MString applyGlobalPostRewrite(const MString& adder) const {
            size_t maxlen = SETTINGS->kanaTrainingMode && ROOT_STROKE_NODE->hasOnlyUsualRewriteNdoe() ? 0 : 8;     // かな入力練習モードで濁点のみなら書き換えをやらない
            if (maxlen > _str.size()) maxlen = _str.size();
            for (size_t len = maxlen; len > 0; --len) {
                MString key = utils::safe_tailstr(_str, len) + adder;
                LOG_DEBUGH(_T("key={}"), to_wstr(key));
                auto iter = globalPostRewriteMap.find(key);
                if (iter != globalPostRewriteMap.end()) {
                    // 書き換え文字列が見つかった
                    LOG_DEBUGH(_T("FOUND: rewrite={}"), to_wstr(iter->second));
                    return len < _str.size() ? utils::safe_substr(_str, 0, _str.size() - len) + iter->second : iter->second;
                }
            }
            return _str + adder;
        }

    public:
        CandidateString()
            : _strokeLen(0), _cost(0), _penalty(0), _isNonTerminal(false) {
        }

        CandidateString(const MString& s, int len, int cost, int penalty)
            : _str(s), _strokeLen(len), _cost(cost), _penalty(penalty), _isNonTerminal(false) {
        }

        CandidateString(const CandidateString& cand, int strokeDelta)
            : _str(cand._str), _strokeLen(cand._strokeLen+strokeDelta), _cost(cand._cost), _penalty(cand._penalty), _isNonTerminal(false) {
        }

        // 自動部首合成の実行
        std::tuple<MString, int> applyAutoBushu(const WordPiece& piece, int strokeCount) const {
            LOG_DEBUGH(_T("CALLED: _str={}, _strokeLen={}, piece={}, strokeCount={}"), to_wstr(_str), piece.debugString(), _strokeLen, strokeCount);
            MStringResult resultOut;
            if (_strokeLen + piece.strokeLen() == strokeCount) {
                if (SETTINGS->autoBushuCompMinCount > 0 && BUSHU_DIC) {
                    // 自動部首合成が有効である
                    const MString& pieceStr = piece.getString();
                    if (_str.size() > 0 && (pieceStr.size() == 1 || (pieceStr.size() > 1 && pieceStr[1] == '|'))) {
                        // 自動部首合成の実行 (複数文字がある場合は先頭の文字だけを対象)
                        LOG_DEBUGH(L"CALL ReduceByAutoBushu({}, {})", (wchar_t)_str.back(), (wchar_t)pieceStr.front());
                        BUSHU_COMP_NODE->ReduceByAutoBushu(_str.back(), pieceStr.front(), resultOut);
                        if (!resultOut.resultStr().empty()) {
                            MString s(_str);
                            s.back() = resultOut.resultStr().front();
                            return { s, 1 };
                        }
                        //mchar_t m = BUSHU_DIC->FindAutoComposite(_str.back(), piece.getString().front());
                        ////if (m == 0) m = BUSHU_DIC->FindComposite(_str.back(), piece.getString().front(), 0);
                        //LOG_DEBUGH(_T("BUSHU_DIC->FindAutoComposite({}, {}) -> {}"),
                        //    to_wstr(utils::safe_tailstr(_str, 1)), to_wstr(utils::safe_substr(piece.getString(), 0, 1)), to_wstr(m));
                        //if (m != 0) {
                        //    MString s(_str);
                        //    s.back() = m;
                        //    return { s, 1 };
                        //}
                    }
                }
            }
            // 空を返す
            return { resultOut.resultStr(), resultOut.numBS()};
        }

        // 部首合成の実行
        MString applyBushuComp() const {
            if (BUSHU_DIC) {
                if (_str.size() >= 2) {
                    // 部首合成の実行
                    MString ms = BUSHU_COMP_NODE->ReduceByBushu(_str[_str.size() - 2], _str[_str.size() - 1]);
                    LOG_DEBUGH(_T("BUSHU_COMP_NODE->ReduceByBushu({}, {}) -> {}"),
                        to_wstr(_str[_str.size() - 2]), to_wstr(_str[_str.size() - 1]), to_wstr(ms));
                    if (!ms.empty()) {
                        MString s(_str.substr(0, _str.size() - 2));
                        s.append(1, ms[0]);
                        return s;
                    }
                    //mchar_t m = BUSHU_DIC->FindComposite(_str[_str.size() - 2], _str[_str.size() - 1], '\0');
                    //LOG_DEBUGH(_T("BUSHU_DIC->FindComposite({}, {}) -> {}"),
                    //    to_wstr(_str[_str.size() - 2]), to_wstr(_str[_str.size() - 1]), to_wstr(m));
                    //if (m != 0) {
                    //    MString s(_str.substr(0, _str.size() - 2));
                    //    s.append(1, m);
                    //    return s;
                    //}
                }
            }
            return EMPTY_MSTR;
        }

        // 交ぜ書き変換の実行
        MString applyMazegaki() const {
            _LOG_DETAIL(L"ENTER: {}", to_wstr(_str));
            MString result;
            std::vector<MString> words;
            MorphBridge::morphCalcCost(_str, words, -SETTINGS->morphMazeEntryPenalty, false);
            //for (auto iter = words.begin(); iter != words.end(); ++iter) {
            //    _LOG_DETAIL(L"morph: {}", to_wstr(*iter));
            //    auto items = utils::split(*iter, '\t');
            //    if (items.size() == 1) {
            //        result.append(items[0]);
            //    }  else if (items.size() >= 2) {
            //        result.append(items[1] == MSTR_MINUS ? items[0] : items[1]);
            //    }
            //}
            bool tailMaze = false;          // 末尾の交ぜ書きのみが置換される
            for (auto iter = words.rbegin(); iter != words.rend(); ++iter) {
                _LOG_DETAIL(L"morph: {}", to_wstr(*iter));
                auto items = utils::split(*iter, '\t');
                if (!items.empty()) {
                    if (tailMaze || items.size() == 1 || items[1] == MSTR_MINUS) {
                        result.insert(0, items[0]);
                    } else {
                        result.insert(0, items[1]);
                        // 末尾の交ぜ書きを実行したら、残りは元の表層形を出力
                        tailMaze = true;
                    }
                }
            }
            _LOG_DETAIL(L"LEAVE: {}", to_wstr(result));
            return result;
        }

        // 単語素片を末尾に適用してみる
        std::vector<MString> applyPiece(const WordPiece& piece, int strokeCount, int paddingLen, bool isStrokeBS, bool bKatakanaConversion) const {
            LOG_DEBUGH(_T("ENTER: pieces={}, strokeCount={}, paddingLen={}, isStrokeBS={}, _strokeLen={}"), piece.debugString(), strokeCount, paddingLen, isStrokeBS, _strokeLen);
            std::vector<MString> ss;
            if (isStrokeBS || (piece.isPadding() && _strokeLen + paddingLen == strokeCount) || (_strokeLen + piece.strokeLen() == strokeCount)) {
                LOG_DEBUGH(_T("isStrokeBS({}) || _strokeLen({}) + piece.strokeLen({})|paddingLen({}) == strokeCount({})"),
                    isStrokeBS, _strokeLen, piece.strokeLen(), paddingLen, strokeCount);
                // 素片のストローク数が適合した
                if (piece.rewriteNode()) {
                    // 書き換えノード
                    const RewriteInfo* rewInfo;
                    int numBS = 0;
                    std::tie(rewInfo, numBS) = matchWithTailString(piece.rewriteNode());

                    if (rewInfo) {
                        // 末尾にマッチする書き換え情報があった
                        for (MString s : split_piece_str(rewInfo->rewriteStr)) {
                            ss.push_back(utils::safe_substr(_str, 0, -numBS) + s);
                        }
                    }
                    // 書き換えない候補も追加
                    // 複数文字が設定されたストロークの扱い
                    LOG_DEBUGH(_T("rewriteNode: {}"), to_wstr(piece.rewriteNode()->getString()));
                    for (MString s : split_piece_str(piece.rewriteNode()->getString())) {
                        ss.push_back(_str + convertoToKatakanaIfAny(s, bKatakanaConversion));
                    }
                } else {
                    int numBS = piece.numBS();
                    if (numBS > 0) {
                        if ((size_t)numBS < _str.size()) {
                            ss.push_back(utils::safe_substr(_str, 0, (int)(_str.size() - numBS)));
                        } else {
                            ss.push_back(EMPTY_MSTR);
                        }
                    } else {
                        // 複数文字が設定されたストロークの扱い
                        LOG_DEBUGH(_T("normalNode: {}"), to_wstr(piece.getString()));
                        for (MString s : split_piece_str(piece.getString())) {
                            if (bKatakanaConversion) {
                                ss.push_back(_str + convertoToKatakanaIfAny(s, bKatakanaConversion));
                            } else {
                                ss.push_back(applyGlobalPostRewrite(s));
                            }
                        }
                    }
                }
            } else {
                // ここで空文字列を push_back してはいけない。_candidates の stroke が進むことによって余計な候補が追加されてしまう。
                LOG_DEBUGH(_T("return NO result: _strokeLen({}) + piece.strokeLen({}) != strokeCount({})"), _strokeLen, piece.strokeLen(), strokeCount);
            }

            LOG_DEBUG(L"LEAVE: ss=\"{}\"", to_wstr(utils::join(ss, '|')));
            return ss;
        }

        const MString& string() const {
            return _str;
        }

        const mchar_t tailChar() const {
            return _str.empty() ? 0 : _str.back();
        }

        const bool isKanjiTailChar() const {
            return utils::is_kanji(tailChar());
        }

        const int strokeLen() const {
            return _strokeLen;
        }

        void addStrokeCount(int delta) {
            _strokeLen += delta;
        }

        int totalCost() const {
            return _cost + _penalty;
        }

        void addCost(int cost) {
            _cost += cost;
        }

        //void zeroCost() {
        //    _penalty = -_cost;
        //}

        int penalty() const {
            return _penalty;
        }

        void penalty(int penalty) {
            _penalty = penalty;
        }

        void zeroPenalty() {
            _penalty = 0;
        }

        bool isNonTerminal() const {
            return _isNonTerminal;
        }

        void setNonTerminal(bool flag = true) {
            _isNonTerminal = flag;
        }

        //float llama_loss() const {
        //    return _llama_loss;
        //}

        //void llama_loss(float loss) {
        //    _llama_loss = loss;
        //}

        String debugString() const {
            return to_wstr(_str)
                + _T(" (totalCost=") + std::to_wstring(totalCost())
                + _T("(_cost=") + std::to_wstring(_cost)
                + _T(",_penalty=") + std::to_wstring(_penalty)
                //+ _T(",_llama_loss=") + std::to_wstring(_llama_loss)
                + _T("), strokeLen=") + std::to_wstring(_strokeLen) + _T(")");
        }
    };

    // 先頭候補以外に、非優先候補ペナルティを与える (先頭候補のペナルティは 0 にする)
    void arrangePenalties(std::vector<CandidateString>& candidates, size_t nSameLen) {
        _LOG_DETAIL(_T("CALLED"));
        candidates.front().zeroPenalty();
        for (size_t i = 1; i < nSameLen; ++i) {
            candidates[i].penalty(NON_PREFERRED_PENALTY * (int)i);
        }
    }

    // ストローク長の同じ候補の数を返す
    size_t getNumOfSameStrokeLen(const std::vector<CandidateString>& candidates) {
        size_t nSameLen = 0;
        if (candidates.size() > 1) {
            int strokeLen = candidates.front().strokeLen();
            ++nSameLen;
            for (auto iter = candidates.begin() + 1; iter != candidates.end() && iter->strokeLen() == strokeLen; ++iter) {
                ++nSameLen;
            }
        }
        return nSameLen;
    }

    // 先頭候補を最優先候補にする
    void selectFirst(std::vector<CandidateString>& candidates) {
        size_t nSameLen = getNumOfSameStrokeLen(candidates);
        if (nSameLen > 1) {
            arrangePenalties(candidates, nSameLen);
            LOG_INFO(_T("CALLED: First candidate preferred."));
        }
    }

    // 先頭候補だけを残す
    void pickupFirst(std::vector<CandidateString>& candidates) {
        size_t nSameLen = getNumOfSameStrokeLen(candidates);
        if (nSameLen > 1) {
            arrangePenalties(candidates, nSameLen);
            LOG_INFO(_T("CALLED: First candidate preferred."));
        }
    }

    // 表層形と交ぜ書き原形などの形態素情報
    class MorphInfo {
        MString surface;
        MString mazeBase;

    };

    // K-best な文字列を格納する
    class KBestList {

        std::vector<CandidateString> _candidates;

        std::vector<MString> _bestStack;

        bool _prevBS = false;

        // 次候補/前候補選択された時の元の先頭候補位置 (-1なら候補選択されていないことを示す)
        int _origFirstCand = -1;

        // 次候補/前候補選択された時の選択候補位置
        int _selectedCandPos = 0;

        // 末尾漢字などの追加で一時的に拡大された候補数
        //int _extendedCandNum = 0;

        // 次のストロークをスキップする候補文字列
        std::set<MString> _kanjiPreferredNextCands;

        static bool _isEmpty(const std::vector<CandidateString> cands) {
            //return cands.empty() || cands.size() == 1 && cands.front().string().empty();
            return cands.empty() || cands.front().string().empty();
        }

        String _debugLog;

    private:
        std::vector<bool> _highFreqJoshiStroke;
        std::vector<bool> _rollOverStroke;

        void setHighFreqJoshiStroke(int count, mchar_t ch) {
            if (count >= 0 && count < 1024) {
                if (count >= (int)_highFreqJoshiStroke.size()) {
                    _highFreqJoshiStroke.resize(count + 1);
                }
                if (isHighFreqJoshi(ch)) _highFreqJoshiStroke[count] = true;
            }
        }
        void setRollOverStroke(int count, bool flag) {
            if (count >= 0 && count < 1024) {
                if (count >= (int)_rollOverStroke.size()) {
                    _rollOverStroke.resize(count + 1);
                }
                _rollOverStroke[count] = flag;
            }
        }

        bool isSingleHitHighFreqJoshi(int count) const {
            return (size_t)count < _highFreqJoshiStroke.size() && (size_t)count < _rollOverStroke.size() ? _highFreqJoshiStroke[count] &&  !_rollOverStroke[count] : false;
        }

    public:
        void setPrevBS(bool flag) {
            _prevBS = flag;
        }

        bool isPrevBS() const {
            return _prevBS;
        }

        // 候補選択による、リアルタイムNgramの蒿上げと抑制
        void raiseAndDepressRealtimeNgramForDiffPart() {
            if (_origFirstCand > 0 && (size_t)_origFirstCand < _candidates.size()) {
                // 候補選択がなされていて、元の先頭候補以外が選択された
                _raiseAndDepressRealtimeNgramForDiffPart(_candidates[_origFirstCand].string(), _candidates[0].string());
                removeOtherThanFirst();
            }
        }

        void clearKbests(bool clearAll) {
            //LOG_INFO(L"CALLED: clearAll={}", clearAll);
            _candidates.clear();
            _bestStack.clear();
            _highFreqJoshiStroke.clear();
            _rollOverStroke.clear();
            _origFirstCand = -1;
            //_extendedCandNum = 0;
            if (clearAll) _kanjiPreferredNextCands.clear();
        }

        void removeOtherThanKBest() {
            if ((int)_candidates.size() > SETTINGS->multiStreamBeamSize) {
                _candidates.erase(_candidates.begin() + SETTINGS->multiStreamBeamSize, _candidates.end());
            }
        }

        void removeOtherThanFirst() {
            if (_candidates.size() > 0) {
                _candidates.erase(_candidates.begin() + 1, _candidates.end());
                _candidates.front().zeroPenalty();
            }
        }

        bool isEmpty() const {
            return _isEmpty(_candidates);
        }

        int size() const {
            return _candidates.size();
        }

        MString getTopString() {
            _LOG_DETAIL(L"_candidates.size={}, topString=\"{}\"", _candidates.size(), to_wstr(_candidates.empty() ? MString() : _candidates[0].string()));
            return _candidates.empty() ? MString() : _candidates[0].string();
        }

        int origFirstCand() const {
            return _origFirstCand;
        }

        int selectedCandPos() const {
            return _origFirstCand < 0 ? _origFirstCand : _selectedCandPos;
        }

        void setSelectedCandPos(int nSameLen) {
            _selectedCandPos = _origFirstCand == 0 ? _origFirstCand : nSameLen - _origFirstCand;
        }

        int cookedOrigFirstCand() const {
            return _origFirstCand >= 0 ? _origFirstCand : 0;
        }

        void resetOrigFirstCand() {
            _origFirstCand = -1;
            //_extendedCandNum = 0;
        }

        void incrementOrigFirstCand(int nSameLen) {
            if (_origFirstCand < 0) {
                _origFirstCand = 1;
            } else {
                ++_origFirstCand;
                if (_origFirstCand >= (int)nSameLen) _origFirstCand = 0;
            }
            setSelectedCandPos(nSameLen);
            //_extendedCandNum = nSameLen;
        }

        void decrementOrigFirstCand(int nSameLen) {
            --_origFirstCand;
            if (_origFirstCand < 0) _origFirstCand = nSameLen - 1;
            setSelectedCandPos(nSameLen);
            //_extendedCandNum = nSameLen;
        }

        // ストローク長の同じ候補の数を返す
        size_t getNumOfSameStrokeLen() const {
            return lattice2::getNumOfSameStrokeLen(_candidates);
        }

        std::vector<MString> getTopCandStrings() const {
            _LOG_DETAIL(L"ENTER: _candidates.size={}, _origFirstCand={}, _selectedCandPos={}", _candidates.size(), _origFirstCand, _selectedCandPos);
            std::vector<MString> result;
            int nSameLen = getNumOfSameStrokeLen();
            int first = cookedOrigFirstCand();
            if (first > nSameLen) first = nSameLen;
            _LOG_DETAIL(L"nSameLen={}, first={}", nSameLen, first);
            for (int i = first; i < nSameLen; ++i) {
                result.push_back(_candidates[i].string());
            }
            for (int i = 0; i < first; ++i) {
                result.push_back(_candidates[i].string());
            }

            size_t maxLen = 0;
            for (const auto& s : result) {
                if (maxLen < s.size()) maxLen = s.size();
            }
            size_t dispLen = 12;
            size_t pos = maxLen > dispLen ? maxLen - dispLen : 0;
            if (pos > 0) {
                for (auto& s : result) {
                    s = s.substr(pos);
                }
            }
            _LOG_DETAIL(L"LEAVE: result.size()={}, _origFirstCand={}, _selectedCandPos={}", result.size(), _origFirstCand, _selectedCandPos);
            return result;
        }

        String debugKBestString(size_t maxLn = 100000) const {
            String result = L"kanjiPreferredNextCands=" + kanjiPreferredNextCandsDebug() + L"\n\n";
            result.append(_debugLog);
            result.append(std::format(L"\nTotal candidates={}\n", _candidates.size()));
            result.append(L"\nKBest:\n");
            for (size_t i = 0; i < _candidates.size() && i < maxLn; ++i) {
                result.append(std::to_wstring(i));
                result.append(_T(": "));
                result.append(_candidates[i].debugString());
                result.append(_T("\n"));
            }
            return result;
        }

    private:
        MString MS_MAZE = to_mstr(L"MAZE");

        // 形態素解析コストの計算
        int calcMorphCost(const MString& s, std::vector<MString>& words) {
            int cost = 0;
            if (!s.empty()) {
                // 形態素解析器の呼び出し
                // mazePenalty = 0 なので、形態素解析器の中で、デフォルトの mazePenalty が加算される。
                cost = MorphBridge::morphCalcCost(s, words, 0, true);

                _LOG_DETAIL(L"ENTER: {}: orig morph cost={}, morph={}", to_wstr(s), cost, to_wstr(utils::join(words, '/')));
                std::vector<std::vector<MString>> wordItemsList;
                for (auto iter = words.begin(); iter != words.end(); ++iter) {
                    wordItemsList.push_back(utils::split(*iter, '\t'));
                    //// MAZEコストの追加
                    //if (utils::endsWith(wordItemsList.back().back(), MS_MAZE)) {
                    //    size_t len = wordItemsList.back().front().size();
                    //    int penalty = len <= 2 ? MAZE_PENALTY_2 : len == 3 ? MAZE_PENALTY_3 : MAZE_PENALTY_4;
                    //    cost += penalty;
                    //    _LOG_DETAIL(L"add MAZE penalty={}, new cost={}", penalty, cost);
                    //}
                }
                for (auto iter = wordItemsList.begin(); iter != wordItemsList.end(); ++iter) {
                    const auto& items = *iter;
                    const MString& w = items[0];
                    const MString& feat = items[2];
                    if (w.size() == 1 && utils::is_hiragana(w[0])) {
                        // 1文字ひらがなが2つ続いて、その前後もひらがなのケース
                        // 「開発させる」⇒「さ きれ て さ せる」
                        if (iter != wordItemsList.begin() && iter + 1 != wordItemsList.end() && iter + 2 != wordItemsList.end()) {
                            auto iter0 = iter - 1;
                            auto iter1 = iter + 1;
                            auto iter2 = iter + 2;
                            const MString& w1 = iter1->front();
                            if (w1.size() == 1 && utils::is_hiragana(w1[0])) {
                                // 1文字ひらがなが2つ続いた
                                if (utils::is_hiragana(iter0->front().back()) && utils::is_hiragana(iter2->front().front())) {
                                    // その前後もひらがながだった
                                    cost += MORPH_ISOLATED_HIRAGANA_COST;
                                    _LOG_DETAIL(L"{} {}: ADD ISOLATED_HIRAGANA_COST({}): morphCost={}", to_wstr(w), to_wstr(w1), MORPH_ISOLATED_HIRAGANA_COST, cost);
                                }
                            }
                        }
                    }
                    if (SETTINGS->depressedContinuousKanjiNum > 0) {
                        if (w.size() == 1 && utils::is_pure_kanji(w[0])) {
                            if (SETTINGS->depressedContinuousKanjiNum == 2) {
                                // 単漢字が2つ続くケース (「耳調量序高い」（これってけっこう高い）)
                                if (iter + 1 != wordItemsList.end()) {
                                    auto iter1 = iter + 1;
                                    const MString& w1 = iter1->front();
                                    if (w1.size() == 1 && utils::is_pure_kanji(w1[0])) {
                                        cost += MORPH_CONTINUOUS_ISOLATED_KANJI_COST;
                                        _LOG_DETAIL(L"{} {}: ADD MORPH_CONTINUOUS_ISOLATED_KANJI_COST({}): morphCost={}",
                                            to_wstr(w), to_wstr(w1), MORPH_CONTINUOUS_ISOLATED_KANJI_COST, cost);
                                    }
                                }
                            } else if (SETTINGS->depressedContinuousKanjiNum > 2) {
                                // 単漢字が3つ続くケース (「耳調量序高い」（これってけっこう高い）)
                                if (iter + 1 != wordItemsList.end() && iter + 2 != wordItemsList.end()) {
                                    auto iter1 = iter + 1;
                                    auto iter2 = iter + 2;
                                    const MString& w1 = iter1->front();
                                    const MString& w2 = iter2->front();
                                    if (w1.size() == 1 && utils::is_pure_kanji(w1[0]) && w2.size() == 1 && utils::is_pure_kanji(w2[0])) {
                                        cost += MORPH_CONTINUOUS_ISOLATED_KANJI_COST;
                                        _LOG_DETAIL(L"{} {} {}: ADD MORPH_CONTINUOUS_ISOLATED_KANJI_COST({}): morphCost={}",
                                            to_wstr(w), to_wstr(w1), to_wstr(w2), MORPH_CONTINUOUS_ISOLATED_KANJI_COST, cost);
                                    }
                                }
                            }
                        }
                    }
                    //if (w.size() >= 2 && std::any_of(w.begin(), w.end(), [](mchar_t c) { return utils::is_kanji(c); })) {
                    //    cost -= MORPH_ANY_KANJI_BONUS * (int)(w.size() - 1);
                    //}
                    if (w.size() >= 2) {
                        int kCnt = (int)std::count_if(w.begin(), w.end(), [](mchar_t c) { return utils::is_kanji(c); });
                        if (kCnt > 0) {
                            cost -= MORPH_ANY_KANJI_BONUS * kCnt;
                            _LOG_DETAIL(L"{}: SUB ANY_KANJI_BONUS({}): morphCost={}", to_wstr(w), MORPH_ANY_KANJI_BONUS * kCnt, cost);
                        }
                    }
                    if (w.size() >= 3 && !utils::endsWith(feat, MS_MAZE) && std::all_of(w.begin(), w.end(), [](mchar_t c) { return utils::is_hiragana(c); })) {
                        cost -= MORPH_ALL_HIRAGANA_BONUS;
                        _LOG_DETAIL(L"{}: SUB ALL_HIRAGANA_BONUS({}): morphCost={}", to_wstr(w), MORPH_ALL_HIRAGANA_BONUS, cost);
                    }
                    if (w.size() >= 2 && std::all_of(w.begin(), w.end(), [](mchar_t c) { return utils::is_katakana(c); })) {
                        auto iter1 = iter + 1;
                        bool flag = iter1 == wordItemsList.end();
                        if (!flag) {
                            const MString& w2 = iter1->front();
                            // 次がカタカナ連でないか、合計で6文以上
                            flag = !std::all_of(w2.begin(), w2.end(), [](mchar_t c) { return utils::is_katakana(c); }) || w.size() + w2.size() >= 6;
                        }
                        if (flag) {
                            // 次がカタカナ連でないか、合計で6文以上なら
                            cost -= MORPH_ALL_KATAKANA_BONUS;
                            _LOG_DETAIL(L"{}: SUB ALL_KATAKANA_BONUS({}): morphCost={}", to_wstr(w), MORPH_ALL_KATAKANA_BONUS, cost);
                        }
                    }
                }
                _LOG_DETAIL(L"LEAVE: {}: total morphCost={}", to_wstr(s), cost);
            }
            return cost;
        }
#if 0
        int totalCostWithMorph(const MString& candStr) {
            std::vector<MString> words;
            int morphCost = calcMorphCost(candStr, words);
            return morphCost;
        }
#endif

        float calcLlamaLoss(const MString& s) {
            std::vector<float> logits;
            auto loss = LlamaBridge::llamaCalcCost(s, logits);
            LOG_INFO(L"{}: loss={}, logits={}", to_wstr(s), loss, utils::join_primitive(logits, L","));
            return loss;
        }

        int calcLlamaCost(const MString& s) {
            std::vector<float> logits;
            auto loss = s.size() > 1 ? LlamaBridge::llamaCalcCost(s, logits) : 2.0f;
            int cost = (int)((loss / s.size()) * 1000);
            LOG_INFO(L"{}: loss={}, cost={}, logits={}", to_wstr(s), loss, cost, utils::join_primitive(logits, L","));
            return cost;
        }

        // 2つの文字列の末尾文字列の共通部分が指定の長さより長いか、または全く同じ文字列か
        bool hasLongerCommonSuffixThanOrSameStr(const MString& str1, const MString& str2, int len) {
            LOG_DEBUGH(_T("ENTER: str1={}, str2={}, len={}"), to_wstr(str1), to_wstr(str2), len);
            int n1 = (int)str1.size();
            int n2 = (int)str2.size();
            bool result = false;
            if (len <= n1 && len <= n2) {
                while (n1 > 0 && n2 > 0 && len > 0) {
                    if (str1[n1 - 1] != str2[n2 - 1]) break;
                    --n1;
                    --n2;
                    --len;
                }
                result = len == 0 || (n1 == 0 && n2 == 0);
            } else {
                result = (n1 == n2 && str1 == str2);
            }
            LOG_DEBUGH(_T("LEAVE: remainingLen: {}: str1={}, str2={}, common={}"), utils::boolToString(result), n1, n2, len);
            return result;
        }

        // 新しい候補を追加
        bool addCandidate(std::vector<CandidateString>& newCandidates, CandidateString& newCandStr, const MString& subStr, bool isStrokeBS) {
            _LOG_DETAIL(_T("\nENTER: newCandStr={}, isStrokeBS={}"), newCandStr.debugString(), isStrokeBS);
            bool bAdded = false;
            //bool bIgnored = false;
            const MString& candStr = newCandStr.string();
            //MString subStr = substringBetweenPunctuations(candStr);
            //MString subStr = substringBetweenNonJapaneseChars(candStr);
            _LOG_DETAIL(_T("subStr={}"), to_wstr(subStr));

            std::vector<MString> morphs;

            // 形態素解析コスト
            // 1文字以下なら、形態素解析しない(過|禍」で「禍」のほうが優先されて出力されることがあるため（「禍」のほうが単語コストが低いため）)
            //int morphCost = !SETTINGS->useMorphAnalyzer || subStr.size() <= 1 ? 5000 : calcMorphCost(subStr, morphs);
            int morphCost = !SETTINGS->useMorphAnalyzer || subStr.empty() ? 0 : calcMorphCost(subStr, morphs);
            if (subStr.size() == 1) {
                if (utils::is_katakana(subStr[0])) morphCost += 5000; // 1文字カタカナならさらに上乗せ
            }
            if (!morphs.empty() && isNonTerminalMorph(morphs.back())) {
                _LOG_DETAIL(_T("NON TERMINAL morph={}"), to_wstr(morphs.back()));
                newCandStr.setNonTerminal();
            }

            // Ngramコスト
            int ngramCost = subStr.empty() ? 0 : getNgramCost(subStr, morphs) * SETTINGS->ngramCostFactor;
            //int morphCost = 0;
            //int ngramCost = candStr.empty() ? 0 : getNgramCost(candStr);
            //int llamaCost = candStr.empty() ? 0 : calcLlamaCost(candStr) * SETTINGS->ngramCostFactor;

            // llamaコスト
            int llamaCost = 0;

            // 総コスト
            int candCost = morphCost + ngramCost + llamaCost;
            newCandStr.addCost(candCost);

            //// 「漢字+の+漢字」のような場合はペナルティを解除
            //size_t len = candStr.size();
            //if (len >= 3 && candStr[len - 2] == L'の' && !utils::is_hiragana(candStr[len - 3]) && !utils::is_hiragana(candStr[len - 1])) {
            //    newCandStr.zeroPenalty();
            //}

            int totalCost = newCandStr.totalCost();

            _LOG_DETAIL(_T("CALC: candStr={}, totalCost={}, candCost={} (morph={}[{}], ngram={})"),
                to_wstr(candStr), totalCost, candCost, morphCost, utils::reReplace(to_wstr(utils::join(morphs, ' ')), L"\t", L"|"), ngramCost);

            if (IS_LOG_DEBUGH_ENABLED) {
                if (!isStrokeBS) _debugLog.append(std::format(L"candStr={}, totalCost={}, candCost={} (morph={} [{}] , ngram = {})\n",
                    to_wstr(candStr), totalCost, candCost, morphCost, utils::reReplace(to_wstr(utils::join(morphs, ' ')), L"\t", L"|"), ngramCost));
            }

            if (!newCandidates.empty()) {
                for (auto iter = newCandidates.begin(); iter != newCandidates.end(); ++iter) {
                    int otherCost = iter->totalCost();
                    LOG_DEBUGH(_T("    otherStr={}, otherCost={}"), to_wstr(iter->string()), otherCost);
                    if (totalCost < otherCost) {
                        iter = newCandidates.insert(iter, newCandStr);    // iter は挿入したノードを指す
                        bAdded = true;
                        // TODO LastSameLen は使わない
                        //// 下位のノードで末尾文字列の共通部分が指定の長さより長いものを探し、あればそれを削除
                        //for (++iter; iter != newCandidates.end(); ++iter) {
                        //    if (hasLongerCommonSuffixThanOrSameStr(candStr, iter->string(), LastSameLen)) {
                        //        // 末尾文字列の共通部分が指定の長さより長いか、同じ文字列
                        //        newCandidates.erase(iter);
                        //        LOG_DEBUGH(_T("    REMOVE second best or lesser candidate"));
                        //        break;
                        //    }
                        //}
                        break;
                    //} else if (hasLongerCommonSuffixThanOrSameStr(candStr, iter->string(), LastSameLen)) {
                    //    // 末尾文字列の共通部分が指定の長さより長いか、同じ文字列
                    //    bIgnored = true;
                    //    break;
                    }
                }
            }
#if 0
            int beamSize = SETTINGS->multiStreamBeamSize + (newCandStr.isKanjiTailChar() ? EXTRA_KANJI_BEAM_SIZE : 0);
            if (beamSize < _extendedCandNum) beamSize = _extendedCandNum;
            if (!bAdded && /*!bIgnored &&*/ (int)newCandidates.size() < beamSize) {
                // 余裕があれば末尾に追加(複数漢字候補の場合はkBestを超えても追加)
                newCandidates.push_back(newCandStr);
                bAdded = true;
            }
            if ((int)newCandidates.size() > SETTINGS->multiStreamBeamSize) {
                // kBestサイズを超えたら末尾を削除
#if 0
                newCandidates.resize(SETTINGS->multiStreamBeamSize);
                _LOG_DETAIL(_T("    REMOVE OVERFLOW ENTRY"));
#else
                size_t pos = SETTINGS->multiStreamBeamSize;
                while (pos < newCandidates.size() && pos < (size_t)(SETTINGS->multiStreamBeamSize + EXTRA_KANJI_BEAM_SIZE)) {
                    auto iter = newCandidates.begin() + pos;
                    if (pos >= (size_t)_extendedCandNum && !iter->isKanjiTailChar()) {
                        // 末尾が漢字以外のものだけを削除対象とする
                        newCandidates.erase(iter);
                        _LOG_DETAIL(_T("    REMOVE OVERFLOW ENTRY"));
                    } else {
                        ++pos;
                    }
                }
                if (pos < newCandidates.size()) {
                    newCandidates.resize(pos);
                }
#endif
            }
#else
            if (!bAdded) {
                // 末尾に追加
                newCandidates.push_back(newCandStr);
                bAdded = true;
            }
#endif
            if (bAdded) {
                _LOG_DETAIL(_T("    ADD candidate: {}, totalCost={}"), to_wstr(candStr), totalCost);
            } else {
                _LOG_DETAIL(_T("    ABANDON candidate: {}, totalCost={}"), to_wstr(candStr), totalCost);
            }
            _LOG_DETAIL(_T("LEAVE: {}, newCandidates.size={}"), bAdded ? L"ADD" : L"ABANDON", newCandidates.size());
            return bAdded;
        }

        // 同じ末尾部分を持つ候補が連続しないように移動する
        void rotateSameTailCandidates(std::vector<CandidateString>& newCandidates) {
            std::set<MString> candStrs;
            if (newCandidates.empty() || newCandidates.front().string().size() < SAME_TAIL_LEN) return;

            candStrs.insert(utils::safe_tailstr(newCandidates.front().string(), SAME_TAIL_LEN));
            for (size_t i = 1; i < newCandidates.size() && i < 3; ++i) {
                for (auto iter = newCandidates.begin() + i; iter != newCandidates.end(); ++iter) {
                    auto tail = utils::safe_tailstr(iter->string(), SAME_TAIL_LEN);
                    if (!candStrs.contains(tail)) {
                        if (iter != newCandidates.begin() + i) {
                            std::rotate(newCandidates.begin() + i, iter, iter + 1);
                        }
                        candStrs.insert(tail);
                        break;
                    }
                }
            }
        }

        // beamSize を超えた部分を削除する
        void truncateTailCandidates(std::vector<CandidateString>& newCandidates) {
            _LOG_DETAIL(_T("ENTER: newCandidates.size={}"), newCandidates.size());
            if (IS_LOG_DEBUGH_ENABLED) {
                _debugLog.append(std::format(L"\ntruncateTailCandidates: ENTER: newCandidates.size={}\n", newCandidates.size()));
            }
            std::set<MString> triGrams;
            std::set<MString> biGrams;
            std::set<MString> uniGrams;
            const size_t beamSize = SETTINGS->multiStreamBeamSize;
            const size_t beamSize2 = beamSize + (int)(beamSize * SETTINGS->extraBeamSizeRate);
            size_t pos = 0;
            while (pos < newCandidates.size()) {
                auto iter = newCandidates.begin() + pos;
                const MString& str = iter->string();
                const MString uni = str.size() >= 1 ? utils::safe_tailstr(str, 1) : EMPTY_MSTR;
                const MString bi = str.size() >= 2 ? utils::safe_tailstr(str, 2) : EMPTY_MSTR;
                const MString tri = str.size() >= 3 ? utils::safe_tailstr(str, 3) : EMPTY_MSTR;
                if (pos < beamSize) {
                    if (!uni.empty()) uniGrams.insert(uni);
                    if (!bi.empty()) biGrams.insert(bi);
                    if (!tri.empty()) triGrams.insert(tri);
                    _LOG_DETAIL(_T("[{}] string={}: OK"), pos, to_wstr(str));
                    ++pos;
                } else {
                    if (IS_LOG_DEBUGH_ENABLED && pos == beamSize) {
                        _LOG_DETAIL(_T("uniGrams={}"), to_wstr(utils::join(uniGrams, ',')));
                        _LOG_DETAIL(_T("biGrams={}"), to_wstr(utils::join(biGrams, ',')));
                        _LOG_DETAIL(_T("triGrams={}"), to_wstr(utils::join(triGrams, ',')));
                    }
                    if (iter->isNonTerminal()) {
                        // 非終端は残す
                        _LOG_DETAIL(_T("REMAIN: non terminal: {}"), to_wstr(str));
                        ++pos;
                        continue;
                    }
                    if (!tri.empty()) {
                        if (triGrams.size() < beamSize2 && triGrams.find(tri) == triGrams.end()) {
                            // 未見のtrigramであり、まだ余裕がある
                            triGrams.insert(tri);
                            _LOG_DETAIL(_T("[{}] string={}: triGram OK, triGrams.size={}"), pos, to_wstr(str), triGrams.size());
                            ++pos;
                            continue;
                        }
                    }
                    if (!bi.empty()) {
                        if (biGrams.size() < beamSize2 && biGrams.find(bi) == biGrams.end()) {
                            // 未見のbigramであり、まだ余裕がある
                            biGrams.insert(bi);
                            _LOG_DETAIL(_T("[{}] string={}: biGram OK, biGrams.size={}"), pos, to_wstr(str), biGrams.size());
                            ++pos;
                            continue;
                        }
                    }
                    if (!uni.empty()) {
                        if (uniGrams.size() < beamSize2 && uniGrams.find(uni) == uniGrams.end()) {
                            // 未見のunigramであり、まだ余裕がある
                            uniGrams.insert(uni);
                            _LOG_DETAIL(_T("[{}] string={}: uniGram OK, uniGrams.size={}"), pos, to_wstr(str), uniGrams.size());
                            ++pos;
                            continue;
                        }
                    }
                    _LOG_DETAIL(_T("[{}] string={}: erased"), pos, to_wstr(str));
                    newCandidates.erase(iter);
                }
            }
            if (pos < newCandidates.size()) {
                newCandidates.resize(pos);
            }
            _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
            if (IS_LOG_DEBUGH_ENABLED) {
                _debugLog.append(std::format(L"truncateTailCandidates: LEAVE: newCandidates.size={}\n\n", newCandidates.size()));
            }
            if (newCandidates.size() > maxCandidatesSize) {
                maxCandidatesSize = newCandidates.size();
                LOG_WARNH(_T("max newCandidates.size updated: {}"), maxCandidatesSize);
            }
        }

    private:
        // 素片のストロークと適合する候補だけを追加
        void addOnePiece(std::vector<CandidateString>& newCandidates, const WordPiece& piece, int strokeCount, int paddingLen, bool bKatakanaConversion) {
            _LOG_DETAIL(_T("ENTER: _candidates.size={}, piece={}"), _candidates.size(), piece.debugString());
            bool bAutoBushuFound = false;           // 自動部首合成は一回だけ実行する
            bool isStrokeBS = piece.numBS() > 0;
            const MString& pieceStr = piece.getString();
            //int topStrokeLen = -1;

            if (pieceStr.size() == 1) setHighFreqJoshiStroke(strokeCount, pieceStr[0]);

            int singleHitHighFreqJoshiCost = piece.strokeLen() > 1 && isSingleHitHighFreqJoshi(strokeCount - (piece.strokeLen() - 1)) ? SINGLE_HIT_HIGH_FREQ_JOSHI_KANJI_COST : 0;

            for (const auto& cand : _candidates) {
                //if (topStrokeLen < 0) topStrokeLen = cand.strokeLen();
                bool bStrokeCountMatched = (piece.isPadding() && cand.strokeLen() + paddingLen == strokeCount) || (cand.strokeLen() + piece.strokeLen() == strokeCount);
                _LOG_DETAIL(_T("cand.strokeLen={}, piece.strokeLen()={}, strokeCount={}, paddingLen={}: {}"),
                    cand.strokeLen(), piece.strokeLen(), strokeCount, paddingLen, (bStrokeCountMatched ? L"MATCH" : L"UNMATCH"));
                if (isStrokeBS || bStrokeCountMatched) {
                    // 素片のストロークと適合する候補
                    int penalty = cand.penalty();
                    _LOG_DETAIL(L"cand.string()=\"{}\", contained in kanjiPreferred={}", to_wstr(cand.string()), _kanjiPreferredNextCands.contains(cand.string()));
                    if (singleHitHighFreqJoshiCost > 0) {
                        // 複数ストロークによる入力で、2打鍵目がロールオーバーでなかったらペナルティ
                        penalty += singleHitHighFreqJoshiCost;
                        _LOG_DETAIL(L"Non rollover multi stroke penalty, total penalty={}", penalty);
                    }
                    if (!isStrokeBS && /*cand.strokeLen() == topStrokeLen && */ !pieceStr.empty()
                        && (piece.strokeLen() == 1 || std::all_of(pieceStr.begin(), pieceStr.end(), [](mchar_t c) { return utils::is_hiragana(c);}))
                        && _kanjiPreferredNextCands.contains(cand.string())) {
                        // 漢字優先
                        _LOG_DETAIL(_T("add NON_PREFERRED_PENALTY"));
                        penalty += NON_PREFERRED_PENALTY;
                    }
                    if (!bAutoBushuFound) {
                        MString s;
                        int numBS;
                        std::tie(s, numBS) = cand.applyAutoBushu(piece, strokeCount);  // 自動部首合成
                        if (!s.empty()) {
                            _LOG_DETAIL(_T("AutoBush FOUND"));
                            CandidateString newCandStr(s, strokeCount, 0, penalty);
                            addCandidate(newCandidates, newCandStr, s, isStrokeBS);
                            bAutoBushuFound = true;
                        }
                    }
                    std::vector<MString> ss = cand.applyPiece(piece, strokeCount, paddingLen, isStrokeBS, bKatakanaConversion);
                    int idx = 0;
                    for (MString s : ss) {
                        int _iniCost = 0;
                        MString subStr = substringBetweenNonJapaneseChars(s);
                        if (subStr.length() == 1) {
                            // 1文字以下なら、出現順で初期コストを加算する(「過|禍」で「禍」のほうが優先されて出力されることがあるため)
                            _iniCost = HEAD_SINGLE_CHAR_ORDER_COST * idx;
                            ++idx;
                        }
                        CandidateString newCandStr(s, strokeCount, _iniCost, penalty);
                        addCandidate(newCandidates, newCandStr, subStr, isStrokeBS);
                    }
                    // pieceが確定文字の場合
                    if (pieceStr.size() == 1 && lattice2::isCommitChar(pieceStr[0])) {
                        // 先頭候補だけを残す
                        break;
                    }

                }
            }
            _LOG_DETAIL(_T("LEAVE"));
        }

        // 指定の打鍵回数分、解の先頭部分が同じなら、それらだけを残す
        void commitOnlyWithSameLeaderString() {
            int challengeNum = SETTINGS->challengeNumForSameLeader;
            if (challengeNum > 0 && !_candidates.empty()) {
                MString firstStr = _candidates.front().string();
                if (_bestStack.empty()) {
                    _bestStack.push_back(firstStr);
                } else if (_bestStack.back() != firstStr) {
                    _bestStack.push_back(firstStr);
                    if ((int)_bestStack.size() > challengeNum) {
                        int basePos = (int)_bestStack.size() - challengeNum - 1;
                        auto baseStr = _bestStack[basePos];
                        //if ((int)baseStr.size() >= SAME_LEADER_LEN) {
                        //    bool sameFlag = true;
                        //    for (int i = basePos + 1; sameFlag && i < (int)_bestStack.size(); ++i) {
                        //        sameFlag = utils::startsWith(_bestStack[i], baseStr);
                        //    }
                        //    if (sameFlag) {
                        //        _LOG_DETAIL(L"_bestStack.size={}, challengeNum={}, baseStr={}", _bestStack.size(), challengeNum, to_wstr(baseStr));
                        //        std::vector<CandidateString> tempCands;
                        //        auto iter = _candidates.begin();
                        //        tempCands.push_back(*iter++);
                        //        for (; iter != _candidates.end(); ++iter) {
                        //            if (utils::startsWith(iter->string(), baseStr)) {
                        //                // 先頭部分が一致する候補だけを残す
                        //                tempCands.push_back(*iter);
                        //            }
                        //        }
                        //        _candidates = std::move(tempCands);
                        //    }
                        //}
                    }
                }
            }
        }

        // CurrentStroke の候補を削除する
        void removeCurrentStrokeCandidates(std::vector<CandidateString>& newCandidates, int strokeCount) {
            _LOG_DETAIL(L"ENTER: _candidates.size={}, prevBS={}, strokeCount={}", _candidates.size(), _prevBS, strokeCount);
            if (!_prevBS) {
                if (!_candidates.empty()) {
                    const auto& firstCand = _candidates.front();
                    int delta = 2;
                    for (const auto& cand : _candidates) {
                        if (cand.strokeLen() + delta <= strokeCount) {
                            if (cand.string() == firstCand.string()) {
                                // 先頭と同じ文字列の候補だったら、そのストローク数の他の候補も残さない
                                ++delta;
                                continue;
                            }
                            // 1ストローク以上前の候補を残す
                            CandidateString newCand(cand, delta);
                            _LOG_DETAIL(L"add cand={}", newCand.debugString());
                            newCandidates.push_back(newCand);
                        }
                    }
                    if (!newCandidates.empty() && newCandidates.front().strokeLen() != strokeCount) {
                        // 一つ前のストローク候補が無くなっていたら、それ以下のものを繰り上げる
                        delta = strokeCount - newCandidates.front().strokeLen();
                        _LOG_DETAIL(L"raise candidates: frontCand.strokeLen={}, delta={}", newCandidates.front().strokeLen(), delta);
                        for (auto& cand : newCandidates) {
                            cand.addStrokeCount(delta);
                        }
                    }
                }
            }
            _LOG_DETAIL(L"LEAVE: {} candidates", newCandidates.size());
        }

        //// Strokeを一つ進める
        //void advanceStroke(std::vector<CandidateString>& newCandidates) {
        //    LOG_INFO(L"ENTER: _candidates.size={}", _candidates.size());
        //    for (const auto& cand : _candidates) {
        //        CandidateString newCand(cand, 1);
        //        _LOG_DETAIL(L"add cand={}", newCand.debugString());
        //        newCandidates.push_back(newCand);
        //    }
        //    LOG_INFO(L"LEAVE: {} candidates", newCandidates.size());
        //}

        // llama-loss の小さい順に候補を並べ直す
        //void sortByLlamaLoss(std::vector<CandidateString>& newCandidates) {
        //    std::for_each(newCandidates.begin(), newCandidates.end(), [this](CandidateString& c) {
        //        c.llama_loss(calcLlamaLoss(c.string()));
        //    });
        //    std::sort(newCandidates.begin(), newCandidates.end(), [](const CandidateString& a, const CandidateString& b) {
        //        return a.llama_loss() < b.llama_loss();
        //    });
        //}

        bool isKanjiKatakanaConsecutive(const CandidateString& cand) {
            MString str = cand.string();
            size_t len = str.size();
            return len >= 2 && (utils::is_kanji(str[len - 1]) && utils::is_kanji(str[len - 2]) || utils::is_katakana(str[len - 1]) && utils::is_katakana(str[len - 2]));
        }

        // return: strokeBack による戻しがあったら、先頭を優先する
        std::vector<CandidateString> _updateKBestList_sub(const std::vector<WordPiece>& pieces, int strokeCount, int paddingLen, bool strokeBack, bool bKatakanaConversion) {
            _LOG_DETAIL(_T("ENTER: pieces.size={}, strokeCount={}, strokeBack={}, paddingLen={}"), pieces.size(), strokeCount, strokeBack, paddingLen);
            std::vector<CandidateString> newCandidates;
            if (strokeBack) {
                _LOG_DETAIL(_T("strokeBack"));
                // strokeBackの場合
                //_LOG_DETAIL(L"strokeBack");
                removeCurrentStrokeCandidates(newCandidates, strokeCount);
                if (!newCandidates.empty()) {
                    _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
                    return newCandidates;
                }
                // 以前のストロークの候補が無ければ、通常のBSの動作とする
                removeOtherThanFirst();
            }
            bool isPaddingPiece = pieces.size() == 1 && pieces.front().isPadding();
            bool isBSpiece = pieces.size() == 1 && pieces.front().isBS();
            _LOG_DETAIL(_T("isPaddingPiece={}, isBSpiece={}"), isPaddingPiece, isBSpiece);
            _prevBS = isBSpiece;

            if (!isPaddingPiece && !isBSpiece) {
                raiseAndDepressRealtimeNgramForDiffPart();
            }

            // BS でないか、以前の候補が無くなっていた
            for (const auto& piece : pieces) {
                // 素片のストロークと適合する候補だけを追加
                addOnePiece(newCandidates, piece, strokeCount, paddingLen, bKatakanaConversion);
            }

            if (!isPaddingPiece) {
                rotateSameTailCandidates(newCandidates);
                truncateTailCandidates(newCandidates);
            }

            //sortByLlamaLoss(newCandidates);

            // stroke位置的に組み合せ不可だったものは、strokeCount が範囲内なら残しておく
            if (!isBSpiece /* && !_isPadding(newCandidates)*/) {     // isPadding()だったら、BSなどで先頭のものだけが残されたということ
                for (const auto& cand : _candidates) {
                    if (cand.strokeLen() + SETTINGS->remainingStrokeSize > strokeCount) {
                        newCandidates.push_back(cand);
                    }
                }
            }
            if (!isPaddingPiece || isBSpiece) {
                // 新しく候補が作成された
                resetOrigFirstCand();
            }
            _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
            return newCandidates;
        }

        // 先頭の1ピースだけの挿入(複数文字の場合、順序を変更しない)
        std::vector<CandidateString> _updateKBestList_initial(const CandidateString& dummyCand, const WordPiece& piece, int strokeCount, int paddingLen, bool bKatakanaConversion) {
            _LOG_DETAIL(_T("ENTER: dummyCand.string()=\"{}\", piece.string()={}, strokeCount={}, paddingLen={}"),
                to_wstr(dummyCand.string()), to_wstr(piece.getString()), strokeCount, paddingLen);
            std::vector<CandidateString> newCandidates;
            std::vector<MString> ss = dummyCand.applyPiece(piece, strokeCount, paddingLen, false, bKatakanaConversion);
            for (MString s : ss) {
                CandidateString newCandStr(s, strokeCount, 0, 0);
                newCandidates.push_back(newCandStr);
            }
            newCandidates.push_back(dummyCand);
            _LOG_DETAIL(_T("LEAVE: newCandidates.size()={}"), newCandidates.size());
            return newCandidates;
        }

    public:
        // strokeCount: lattice に最初に addPieces() した時からの相対的なストローク数
        void updateKBestList(const std::vector<WordPiece>& pieces, int strokeCount, bool strokeBack, bool bKatakanaConversion) {
            _LOG_DETAIL(_T("ENTER: _candidates.size()={}, pieces.size()={}, strokeCount={}, strokeBack={}"), _candidates.size(), pieces.size(), strokeCount, strokeBack);
            _debugLog.clear();

            setRollOverStroke(strokeCount - 1, STATE_COMMON->IsRollOverStroke());

            // 候補リストが空の場合は、追加される piece と組み合わせるための、先頭を表すダミーを用意しておく
            addDummyCandidate();

            int paddingLen = 0;
            if (pieces.front().isPadding()) {
                paddingLen = strokeCount - _candidates.front().strokeLen();
                _LOG_DETAIL(_T("paddingLen={}"), paddingLen);
            }
            if (!strokeBack && _candidates.size() == 1 && _candidates.front().string().empty() && pieces.size() == 1) {
                // 先頭の1ピースだけの挿入(複数文字の場合、順序を変更しない)
                _candidates = std::move(_updateKBestList_initial(_candidates.front(), pieces.front(), strokeCount, paddingLen, bKatakanaConversion));
            } else {
                _candidates = std::move(_updateKBestList_sub(pieces, strokeCount, paddingLen, strokeBack, bKatakanaConversion));
            }

            //// 漢字またはカタカナが2文字以上連続したら、その候補を優先する
            //if (!_candidates.empty()) {
            //    if (isKanjiKatakanaConsecutive(_candidates.front())) selectFirst();
            //}
            // 指定の打鍵回数分、解の先頭部分が同じなら、それらだけを残す (ただし、候補の選択状態でない場合)
            if (_origFirstCand < 0) commitOnlyWithSameLeaderString();
            _LOG_DETAIL(_T("LEAVE: _candidates.size()={}"), _candidates.size());
        }

    private:
        // 先頭候補以外に、非優先候補ペナルティを与える (先頭候補のペナルティは 0 にする)
        void arrangePenalties(size_t nSameLen) {
            //_candidates.front().zeroPenalty();
            //for (size_t i = 1; i < nSameLen; ++i) {
            //    _candidates[i].penalty(NON_PREFERRED_PENALTY * (int)i);
            //}
            return lattice2::arrangePenalties(_candidates, nSameLen);
        }

    public:
        void setKanjiPreferredNextCands() {
            //LOG_INFO(L"ENTER");
            _kanjiPreferredNextCands.clear();
            if (!_candidates.empty()) {
                int topStrokeCnt = _candidates.front().strokeLen();
                for (const auto& c : _candidates) {
                    if (c.strokeLen() != topStrokeCnt) break;
                    _kanjiPreferredNextCands.insert(c.string());
                }
            }
            if (_kanjiPreferredNextCands.empty()) _kanjiPreferredNextCands.insert(EMPTY_MSTR);
            //LOG_INFO(L"LEAVE: kanjiPreferredNextCands={}", kanjiPreferredNextCandsDebug());
        }

        String kanjiPreferredNextCandsDebug() const {
            return std::to_wstring(_kanjiPreferredNextCands.size()) + L":[" + to_wstr(utils::join(_kanjiPreferredNextCands, L',')) + L"]";
        }

        // 先頭を表すダミーを用意しておく
        void addDummyCandidate() {
            if (_candidates.empty()) {
                _candidates.push_back(CandidateString());
            }
        }

        // 先頭候補を取得する
        MString getFirst() const {
            _LOG_DETAIL(_T("CALLED"));
            if (_candidates.empty()) return MString();
            return _candidates.front().string();
        }

        // 先頭候補を最優先候補にする
        void selectFirst() {
            _LOG_DETAIL(_T("ENTER: _origFirstCand={}"), _origFirstCand);
            //size_t nSameLen = getNumOfSameStrokeLen();
            //if (nSameLen > 1) {
            //    arrangePenalties(nSameLen);
            //    LOG_INFO(_T("CALLED: First candidate preferred."));
            //}
            lattice2::selectFirst(_candidates);
            resetOrigFirstCand();
            _LOG_DETAIL(_T("LEAVE: _origFirstCand={}"), _origFirstCand);
        }

        // 次候補を選択する (一時的に優先するだけなので、他の候補を削除したりはしない)
        void selectNext() {
            _LOG_DETAIL(_T("ENTER: _origFirstCand={}"), _origFirstCand);
            size_t nSameLen = getNumOfSameStrokeLen();
            _LOG_DETAIL(_T("candidates.size={}, nSameLen={}"), _candidates.size(), nSameLen);
            if (nSameLen > 1) {
                auto begin = _candidates.begin();
                std::rotate(begin, begin + 1, begin + nSameLen);
                arrangePenalties(nSameLen);
                decrementOrigFirstCand((int)nSameLen);
            }
            _LOG_DETAIL(_T("LEAVE: _origFirstCand={}, _selectedCandPos={}"), _origFirstCand, _selectedCandPos);
        }

        // 前候補を選択する (一時的に優先するだけなので、他の候補を削除したりはしない)
        void selectPrev() {
            _LOG_DETAIL(_T("ENTER: _origFirstCand={}"), _origFirstCand);
            size_t nSameLen = getNumOfSameStrokeLen();
            _LOG_DETAIL(_T("candidates.size={}, nSameLen={}"), _candidates.size(), nSameLen);
            if (nSameLen > 1) {
                auto begin = _candidates.begin();
                std::rotate(begin, begin + nSameLen - 1, begin + nSameLen);
                arrangePenalties(nSameLen);
                incrementOrigFirstCand((int)nSameLen);
            }
            _LOG_DETAIL(_T("LEAVE: _origFirstCand={}, _selectedCandPos={}"), _origFirstCand, _selectedCandPos);
        }

    private:
        void updateByConversion(const MString& s) {
            if (!s.empty()) {
                CandidateString newCandStr(s, _candidates.front().strokeLen(), 0, 0);
                _candidates.insert(_candidates.begin(), newCandStr);
                size_t nSameLen = getNumOfSameStrokeLen();
                arrangePenalties(nSameLen);
            }
        }

    public:
        // 部首合成
        void updateByBushuComp() {
            _LOG_DETAIL(_T("CALLED"));
            if (!_candidates.empty()) {
                updateByConversion(_candidates.front().applyBushuComp());
            }
        }

        // 交ぜ書き変換
        void updateByMazegaki() {
            _LOG_DETAIL(_T("CALLED"));
            if (!_candidates.empty()) {
                updateByConversion(_candidates.front().applyMazegaki());
            }
        }

    };

    // Lattice
    class LatticeImpl : public Lattice2 {
        DECLARE_CLASS_LOGGER;
    private:
        // 打鍵開始位置
        int _startStrokeCount = 0;

        // K-best な文字列を格納する
        KBestList _kBestList;

        // 前回生成された文字列
        MString _prevOutputStr;

        // 漢字優先設定時刻
        time_t _kanjiPreferredSettingDt;

        // 前回生成された文字列との共通する先頭部分の長さ
        size_t calcCommonPrefixLenWithPrevStr(const MString& outStr) {
            LOG_DEBUGH(_T("ENTER: outStr={}, prevStr={}"), to_wstr(outStr), to_wstr(_prevOutputStr));
            size_t n = 0;
            while (n < outStr.size() && n < _prevOutputStr.size()) {
                if (outStr[n] != _prevOutputStr[n]) break;
                ++n;
            }
            LOG_DEBUGH(_T("LEAVE: commonLen={}"), n);
            return n;
        }

        Deque<String> _debugLogQueue;

        String formatStringOfWordPieces(const std::vector<WordPiece>& pieces) {
            return utils::join(utils::select<String>(pieces, [](WordPiece p){return p.debugString();}), _T("|"));
        }

        // すべての単語素片が1文字で、それが漢字・ひらがな・カタカナ以外か
        bool areAllPiecesNonJaChar(const std::vector<WordPiece>& pieces) {
            for (const auto iter : pieces) {
                MString s = pieces.front().getString();
                //LOG_INFO(_T("s: len={}, str={}"), s.size(), to_wstr(s));
                if (s.size() != 1 || utils::is_japanese_char_except_nakaguro(s.front())) {
                    //LOG_INFO(_T("FALSE"));
                    return false;
                }
            }
            return true;
        }

    public:
        // コンストラクタ
        LatticeImpl() : _kanjiPreferredSettingDt(0) {
            _LOG_DETAIL(_T("CALLED: Constructor"));
        }

        // デストラクタ
        ~LatticeImpl() override {
            _LOG_DETAIL(_T("CALLED: Destructor"));
            clear();
        }

        void clearAll() override {
            _LOG_DETAIL(_T("CALLED"));
            _startStrokeCount = 0;
            _prevOutputStr.clear();
            _kBestList.clearKbests(true);
        }

        void clear() override {
            _LOG_DETAIL(_T("CALLED"));
            _startStrokeCount = 0;
            _prevOutputStr.clear();
            _kBestList.clearKbests(utils::diffTime(_kanjiPreferredSettingDt) >= 3.0);     // 前回の設定時刻から３秒以上経過していたらクリアできる
        }

        void removeOtherThanKBest() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList.removeOtherThanKBest();
        }

        void removeOtherThanFirst() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList.removeOtherThanFirst();
        }

        bool isEmpty() override {
            //_LOG_DETAIL(_T("CALLED: isEmpty={}"), _kBestList.isEmpty());
            return _kBestList.isEmpty();
        }

        // 先頭候補を取得する
        MString getFirst() const override {
            _LOG_DETAIL(_T("CALLED"));
            return _kBestList.getFirst();
        }

        // 先頭候補を最優先候補にする
        void selectFirst() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList.selectFirst();
        }

        // 次候補を選択する
        void selectNext() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList.selectNext();
        }

        // 前候補を選択する
        void selectPrev() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList.selectPrev();
        }

        // 候補選択による、リアルタイムNgramの蒿上げと抑制
        void raiseAndDepressRealtimeNgramForDiffPart() override {
            _kBestList.raiseAndDepressRealtimeNgramForDiffPart();
        }

        // 部首合成
        void updateByBushuComp() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList.updateByBushuComp();
        }

        // 交ぜ書き変換
        void updateByMazegaki() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList.updateByMazegaki();
        }

    public:
        // 単語素片リストの追加(単語素片が得られなかった場合も含め、各打鍵ごとに呼び出すこと)
        // 単語素片(WordPiece): 打鍵後に得られた出力文字列と、それにかかった打鍵数
        LatticeResult addPieces(const std::vector<WordPiece>& pieces, bool kanjiPreferredNext, bool strokeBack, bool bKatakanaConversion) override {
            _LOG_DETAIL(_T("ENTER: pieces: {}"), formatStringOfWordPieces(pieces));
            int totalStrokeCount = (int)(STATE_COMMON->GetTotalDecKeyCount());
            if (_startStrokeCount == 0) _startStrokeCount = totalStrokeCount;
            int currentStrokeCount = totalStrokeCount - _startStrokeCount + 1;

            //LOG_DEBUGH(_T("ENTER: currentStrokeCount={}, pieces: {}\nkBest:\n{}"), currentStrokeCount, formatStringOfWordPieces(pieces), _kBestList.debugString());
            _LOG_DETAIL(_T("INPUT: _kBestList.size={}, _origFirstCand={}, totalStroke={}, currentStroke={}, kanjiPref={}, strokeBack={}, rollOver={}, pieces: {}"),
                _kBestList.size(), _kBestList.origFirstCand(), totalStrokeCount, currentStrokeCount, kanjiPreferredNext, strokeBack, STATE_COMMON->IsRollOverStroke(), formatStringOfWordPieces(pieces));

            if (pieces.empty()) {
                // pieces が空になるのは、同時打鍵の途中の状態などで、文字が確定していない場合
                _LOG_DETAIL(L"LEAVE: emptyResult");
                return LatticeResult::emptyResult();
            }

            //LOG_DEBUGH(L"A:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            // endPos における空の k-best path リストを取得

            //if (pieces.size() == 1) {
            //    auto s = pieces.front().getString();
            //    if (s.size() == 1 && utils::is_punct(s[0])) {
            //        // 前回の句読点から末尾までの出力文字列に対して Ngram解析を行う
            //        _LOG_DETAIL(L"CALL lattice2::updateRealtimeNgram()");
            //        lattice2::updateRealtimeNgram();
            //    }
            //}
            // フロントエンドから updateRealtimeNgram() を呼び出すので、ここではやる必要がない

            if (kanjiPreferredNext) {
                _LOG_DETAIL(L"KANJI PREFERRED NEXT");
                // 現在の先頭候補を最優先に設定し、
                selectFirst();
                //LOG_DEBUGH(L"C:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
                // 次のストロークを漢字優先とする
                _kBestList.setKanjiPreferredNextCands();
                if (_startStrokeCount == 1) _startStrokeCount = 0;  // 先頭での漢字優先なら、0 クリアしておく(この後、clear()が呼ばれるので、それと状態を合わせるため)
                //LOG_DEBUGH(L"D:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
                _kanjiPreferredSettingDt = utils::getSecondsFromEpochTime();
                //LOG_DEBUGH(L"E:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            }

            //LOG_DEBUGH(L"F:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            _LOG_DETAIL(L"_kBestList.size={}", _kBestList.size());

            //// すべての単語素片が1文字で、それが漢字・ひらがな・カタカナ以外だったら、現在の先頭候補を優先させる
            //if (!pieces.empty() && areAllPiecesNonJaChar(pieces)) {
            //    _LOG_DETAIL(L"ALL PIECES NON JA CHAR");
            //    selectFirst();
            //}
            //_LOG_DETAIL(L"_kBestList.size={}", _kBestList.size());

            // 候補リストの更新
            _kBestList.updateKBestList(pieces, currentStrokeCount, strokeBack, bKatakanaConversion);

            //LOG_DEBUGH(L"G:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            //LOG_DEBUGH(_T(".\nresult kBest:\n{}"), pKBestList->debugString());
            size_t numBS = 0;
            MString outStr = _kBestList.getTopString();
            size_t commonLen = calcCommonPrefixLenWithPrevStr(outStr);
            numBS = _prevOutputStr.size() - commonLen;
            _prevOutputStr = outStr;
            outStr = utils::safe_substr(outStr, commonLen);

            _LOG_DETAIL(_T("OUTPUT: {}, numBS={}\n\n{}"), to_wstr(outStr), numBS, _kBestList.debugKBestString());
            if (IS_LOG_DEBUGH_ENABLED) {
                while (_debugLogQueue.size() >= 10) _debugLogQueue.pop_front();
                _debugLogQueue.push_back(std::format(L"========================================\nENTER: currentStrokeCount={}, pieces: {}\n",
                    currentStrokeCount, formatStringOfWordPieces(pieces)));
                if (pieces.back().numBS() <= 0) {
                    _debugLogQueue.push_back(std::format(L"\n{}\nOUTPUT: {}, numBS={}\n\n", _kBestList.debugKBestString(SETTINGS->multiStreamBeamSize), to_wstr(outStr), numBS));
                }
            }

            //LOG_DEBUGH(L"H:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            // 解候補を仮想鍵盤に表示する
            std::vector<MString> candStrings = _kBestList.getTopCandStrings();
            if (candStrings.size() > 1) {
                STATE_COMMON->SetVirtualKeyboardStrings(VkbLayout::MultiStreamCandidates, EMPTY_MSTR, candStrings);
                STATE_COMMON->SetWaitingCandSelect(_kBestList.selectedCandPos());
            }
            //LOG_DEBUGH(L"I:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));

            _LOG_DETAIL(_T("LEAVE"));
            return LatticeResult(outStr, numBS);
        }

        // 融合候補の表示
        void saveCandidateLog() override {
            LOG_INFO(_T("ENTER"));
            String result;
            while (!_debugLogQueue.empty()) {
                result.append(_debugLogQueue.front());
                _debugLogQueue.pop_front();
            }
            LOG_INFO(L"result: {}", result);
            utils::OfstreamWriter writer(utils::joinPath(SETTINGS->rootDir, SETTINGS->mergerCandidateFile));
            if (writer.success()) {
                writer.writeLine(utils::utf8_encode(result));
                LOG_INFO(_T("result written"));
            }
            LOG_INFO(_T("LEAVE"));
        }

    };
    DEFINE_CLASS_LOGGER(LatticeImpl);

} // namespace lattice2


std::unique_ptr<Lattice2> Lattice2::Singleton;

void Lattice2::createLattice() {
    lattice2::loadCostAndNgramFile();
    Singleton.reset(new lattice2::LatticeImpl());
}

void Lattice2::reloadCostAndNgramFile() {
    lattice2::loadCostAndNgramFile(false);
}

void Lattice2::reloadUserCostFile() {
    lattice2::loadCostAndNgramFile(false, false);
}

void Lattice2::updateRealtimeNgram() {
    //lattice2::updateRealtimeNgram();
}

void Lattice2::updateRealtimeNgram(const MString& str) {
    lattice2::updateRealtimeNgram(str);
}

void Lattice2::raiseRealtimeNgram(const MString& str) {
    lattice2::raiseRealtimeNgram(str, true);
}

void Lattice2::depressRealtimeNgram(const MString& str) {
    lattice2::depressRealtimeNgram(str, true);
}

void Lattice2::saveRealtimeNgramFile() {
    lattice2::saveRealtimeNgramFile();
}

void Lattice2::reloadGlobalPostRewriteMapFile() {
    lattice2::readGlobalPostRewriteMapFile();
}

