#include "Logger.h"
#include "file_utils.h"

#include "settings.h"

#include "Lattice2_Common.h"
#include "Lattice2_Ngram.h"

#include "Ngram/NgramBridge.h"

namespace lattice2 {
    DECLARE_LOGGER;     // defined in Lattice2.cpp

#define SYSTEM_NGRAM_FILE           L"files/mixed_all.ngram.txt"
#define KATAKANA_COST_FILE          L"files/katakana.cost.txt"
#define REALTIME_NGRAM_MAIN_FILE    L"files/realtime.ngram.txt"
#define REALTIME_NGRAM_TEMP_FILE    L"files/realtime.ngram.tmp.txt"
//#define USER_NGRAM_FILE           L"files/user.ngram.txt"
#define USER_COST_FILE              L"files/userword.cost.txt"

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

    // cost ファイルに登録がある unigram のデフォルトのボーナスカウント
    int DEFAULT_UNIGRAM_BONUS_COUNT = 10000;

    // 2文字以上の形態素で先頭が高頻度助詞、2文字目がひらがな以外の場合のボーナス
    int HEAD_HIGH_FREQ_JOSHI_BONUS = 1000;

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

    //// システムで用意したカタカナ単語コスト(3gram以上、sysCost < uesrCost のものだけ計上)
    //std::map<MString, int> KatakanaWordCosts;

    // 利用者の選択によって抑制されるNgramに課されるペナルティを計算するためのベースとなるカウント
    std::map<MString, int> realtimeNgramDepressCounts;

    // 常用対数でNgram頻度を補正した値に掛けられる係数
    int ngramLogFactor = 50;

    // Ngram頻度がオンラインで更新されたか
    bool realtimeNgram_updated = false;

    //--------------------------------------------------------------------------------------
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

    inline bool isHeadHighFreqJoshi(mchar_t mc) {
        return mc == L'と' || mc == L'を' || mc == L'に' || mc == L'の' || mc == L'で' || mc == L'は' || mc == L'が';
    }

    inline bool isSmallKatakana(mchar_t mc) {
        return mc == L'ァ' || mc == L'ィ' || mc == L'ゥ' || mc == L'ェ' || mc == L'ォ' || mc == L'ャ' || mc == L'ュ' || mc == L'ョ';
    }

    int _calcTrigramBonus(const MString& word, int rtmCount, int sysCount = 0, int origSysCnt = 0, int usrCount = 0, int origRtmCnt = 0) {
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
        int bonusTotal = -(bonusTier1 + bonusTier2 + bonusTier3 + bonusTier4);
        if (IS_LOG_DEBUGH_ENABLED) {
            if (sysCount == 0 && origSysCnt == 0 && usrCount == 0 && origRtmCnt == 0) {
                LOG_WARN(L"REGEX-MATCH COST: word={}, cost={}, rtmCount={}", to_wstr(word), bonusTotal, rtmCount);
            } else if (bonusTotal < -(SETTINGS->realtimeTrigramTier1Num * bonumFactor * 10 + 1000)) {
                LOG_WARN(L"HIGHER COST: word={}, cost={}, sysCount={}, usrCount={}, rtmCount={}, tier1=({}:{}), tier2=({}:{}), tier3=({}:{}), tier4=({}:{})",
                    to_wstr(word), bonusTotal, origSysCnt, usrCount, origRtmCnt, numTier1, bonusTier1, numTier2, bonusTier2, numTier3, bonusTier3, numTier4, bonusTier4);
            }
        }
        return bonusTotal;
    }

    void _raiseRealtimeNgramByWord(const MString& word, bool manualSelect) {
        realtimeNgramDepressCounts[word] += manualSelect ? SETTINGS->ngramDepressCountFactor : 1;
        realtimeNgram_updated = true;
    }

    void _depressRealtimeNgramByWord(const MString& word, bool manualSelect) {
        realtimeNgramDepressCounts[word] -= manualSelect ? SETTINGS->ngramDepressCountFactor : 1;
        realtimeNgram_updated = true;
    }

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
                    //if (IS_LOG_DEBUGH_ENABLED) {
                    //    String ws = to_wstr(word);
                    //    if (ws == L"ている" || ws == L"いるな" || ws == L"ていか" || ws == L"いかな") {
                    //        LOG_WARNH(L"file={}, word={}, count={}", ngramFile, ws, count);
                    //    }
                    //}
                }
            }
        }
        LOG_INFO(_T("DONE: maxFreq={}"), maxFreq);
        return maxFreq;
    }

#define REALTIME_NGRAM_FILE (SETTINGS->useTmpRealtimeNgramFile ? REALTIME_NGRAM_TEMP_FILE : REALTIME_NGRAM_MAIN_FILE)

    void loadCostAndNgramFile(bool systemNgramFile, bool realtimeNgramFile) {
        LOG_INFO(L"ENTER: systemNgramFile={}, realtimeNgramFile={}", systemNgramFile, realtimeNgramFile);
        _loadNgramFile(REALTIME_NGRAM_FILE, realtimeNgramDepressCounts);
        maxCandidatesSize = 0;
        LOG_INFO(L"LEAVE");
    }

    void saveRealtimeNgramFile() {
        LOG_SAVE_DICT(L"ENTER: file={}, realtimeNgram_updated={}", REALTIME_NGRAM_FILE, realtimeNgram_updated);
#ifndef _DEBUG
        auto path = utils::joinPath(SETTINGS->rootDir, REALTIME_NGRAM_FILE);
        if (realtimeNgram_updated) {
            // 一旦、一時ファイルに書き込み
            auto pathTmp = path + L".tmp";
            {
                LOG_SAVE_DICT(_T("SAVE: realtime ngram file pathTmp={}"), pathTmp.c_str());
                utils::OfstreamWriter writer(pathTmp);
                if (writer.success()) {
                    for (const auto& pair : realtimeNgramDepressCounts) {
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
                LOG_SAVE_DICT(_T("DONE: entries count={}"), realtimeNgramDepressCounts.size());
            }
            // pathTmp ファイルのサイズが path ファイルのサイズよりも小さい場合は、書き込みに失敗した可能性があるので、既存ファイルを残す
            utils::compareAndMoveFileToBackDirWithRotation(pathTmp, path, SETTINGS->backFileRotationGeneration);
        }
#endif
        LOG_SAVE_DICT(L"LEAVE: file={}", REALTIME_NGRAM_FILE);
    }

    inline bool is_space_or_vbar(mchar_t ch) {
        return ch == ' ' || ch == '|';
    }

    // 編集バッファのフラッシュによるリアルタイムNgramの更新
    void updateRealtimeNgram(const MString& str) {
        raiseRealtimeNgram(str, false);
    }

    // リアルタイムNgramの蒿上げ
    void raiseRealtimeNgram(const MString& str, bool bByGUI) {
        LOG_DEBUGH(L"ENTER: str={}, byGUI={}", to_wstr(str), bByGUI);
        int strlen = (int)str.size();
        int hiraganaLen = 0;
        int kanjiLen = 0;
        for (int pos = 0; pos < strlen; ++pos) {
            int charLen = 0;
            if (utils::is_hiragana(str[pos])) {
                kanjiLen = 0;
                charLen = ++hiraganaLen;
            } else if (utils::is_kanji(str[pos])) {
                hiraganaLen = 0;
                charLen = ++kanjiLen;
            } else {
                hiraganaLen = 0;
                kanjiLen = 0;
            }
            if (charLen >= 3 && pos >= 2) {
                _raiseRealtimeNgramByWord(str.substr(pos - 2, 3), bByGUI);
            }
            if (charLen >= 2 && pos >= 1) {
                _raiseRealtimeNgramByWord(str.substr(pos - 1, 2), bByGUI);
            }
        }
        LOG_DEBUGH(L"LEAVE: str={}", to_wstr(str));
    }

    // リアルタイムNgramの抑制
    void depressRealtimeNgram(const MString& str, bool bByGUI) {
        LOG_DEBUGH(L"CALLED: str={}", to_wstr(str));
        int strlen = (int)str.size();
        int hiraganaLen = 0;
        int kanjiLen = 0;
        for (int pos = 0; pos < strlen; ++pos) {
            int charLen = 0;
            if (utils::is_hiragana(str[pos])) {
                kanjiLen = 0;
                charLen = ++hiraganaLen;
            } else if (utils::is_kanji(str[pos])) {
                hiraganaLen = 0;
                charLen = ++kanjiLen;
            } else {
                hiraganaLen = 0;
                kanjiLen = 0;
            }
            if (charLen >= 3 && pos >= 2) {
                _depressRealtimeNgramByWord(str.substr(pos - 3, 3), bByGUI);
            }
            if (charLen >= 2 && pos >= 1) {
                _depressRealtimeNgramByWord(str.substr(pos - 2, 2), bByGUI);
            }
        }
        LOG_DEBUGH(L"LEAVE: str={}", to_wstr(str));
    }

    // 候補選択による、リアルタイムNgramの嵩上げ
    void _raiseRealtimeNgramForDiffPart(const MString& oldCand, const MString& newCand) {
        LOG_WARNH(L"ENTER: oldCand={}, newCand={}", to_wstr(oldCand), to_wstr(newCand));
        int prevDiffPos = -10;
        int oldSize = (int)oldCand.size();
        int newSize = (int)newCand.size();
        for (int pos = 0; pos < newSize; ++pos) {
            bool currentDiff = pos < oldSize || oldCand[pos] != newCand[pos];
            if (currentDiff || prevDiffPos + 2 == pos) {
                if (pos >= 2) {
                    // 2文字前から3gramについて嵩上げする
                    _raiseRealtimeNgramByWord(newCand.substr(pos - 2, 3), true);
                }
            }
            if (currentDiff || prevDiffPos + 1 == pos) {
                if (pos >= 1) {
                    if (utils::is_kanji(newCand[pos - 1]) || pos + 1 == newSize) {
                        // 1文字前が漢字あるいは当位置で文字列が終了の場合は、2gramも嵩上げする
                        _raiseRealtimeNgramByWord(newCand.substr(pos - 1, 2), true);
                    }
                }
            }
            if (currentDiff) {
                prevDiffPos = pos;
            }
        }
        LOG_WARNH(L"LEAVE");
    }

    // 候補選択による、リアルタイムNgramの抑制
    void _depressRealtimeNgramForDiffPart(const MString& oldCand, const MString& newCand) {
        LOG_WARNH(L"ENTER: oldCand={}, newCand={}", to_wstr(oldCand), to_wstr(newCand));
        int prevDiffPos = -10;
        int oldSize = (int)oldCand.size();
        int newSize = (int)newCand.size();
        for (int pos = 0; pos < oldSize; ++pos) {
            bool currentDiff = pos < newSize || oldCand[pos] != newCand[pos];
            if (currentDiff || prevDiffPos + 2 == pos) {
                if (pos >= 2) {
                    // 2文字前から3gramについて抑制をする
                    _depressRealtimeNgramByWord(oldCand.substr(pos - 2, 3), true);
                }
            }
            if (currentDiff || prevDiffPos + 1 == pos) {
                if (pos >= 1) {
                    if (utils::is_kanji(oldCand[pos - 1]) || pos + 1 == oldSize) {
                        // 1文字前が漢字あるいは当位置で文字列が終了の場合は、2gramも抑制する
                        _depressRealtimeNgramByWord(oldCand.substr(pos - 1, 2), true);
                    }
                }
            }
            if (currentDiff) {
                prevDiffPos = pos;
            }
        }
        LOG_WARNH(L"LEAVE");
    }

    // 候補選択による、リアルタイムNgramの蒿上げと抑制
    void raiseAndDepressRealtimeNgramForDiffPart(const MString& oldCand, const MString& newCand) {
        LOG_WARNH(L"ENTER: oldCand={}, newCand={}", to_wstr(oldCand), to_wstr(newCand));
        _raiseRealtimeNgramForDiffPart(oldCand, newCand);
        _depressRealtimeNgramForDiffPart(oldCand, newCand);
        LOG_WARNH(L"LEAVE");
    }

    int getDepressedNgramPenalty(const MString& word) {
        auto iter = realtimeNgramDepressCounts.find(word);
        if (iter != realtimeNgramDepressCounts.end()) {
            int raiseCount = iter->second;
            if (raiseCount > 0) {
                int depressCost = (-raiseCount) * SETTINGS->ngramDepressPenaltyFactor;
                _LOG_DETAIL(L"{}: raiseCount={}, depressCost={}", to_wstr(word), raiseCount, depressCost);
                return depressCost;
            }
        }
        return 0;
    }

    //int getWordConnCost(const MString& s1, const MString& s2) {
    //    return get_base_ngram_cost(utils::last_substr(s1, 1) + utils::safe_substr(s2, 0, 1)) / 2;
    //}

    std::wregex kanjiDateTime(L"年[一二三四五六七八九十]+月?|[一二三四五六七八九十]+月[一二三四五六七八九十]?|月[一二三四五六七八九十]+日?|[一二三四五六七八九十]+日");

    // Ngramコストの取得
    int getNgramCost(const MString& str, const std::vector<MString>& morphs) {
        _LOG_DETAIL(L"ENTER: str={}", to_wstr(str));
        std::vector<MString> ngrams;
        int cost = NgramBridge::ngramCalcCost(MSTR_GETA + str, ngrams, IS_LOG_INFOH_ENABLED);
        if (IS_LOG_INFOH_ENABLED) {
            LOG_INFOH(L"ngrams:\n--------\n{}\n--------", to_wstr(utils::join(ngrams, '\n')));
        }
        if (str.size() >= 2) {
            for (size_t pos = 0 ; pos < str.size() - 1; ++pos) {
                cost += getDepressedNgramPenalty(utils::safe_substr(str, pos, 2));
                if (pos + 2 < str.size()) {
                    cost += getDepressedNgramPenalty(utils::safe_substr(str, pos, 3));
                }
            }
        }
        _LOG_DETAIL(L"LEAVE: cost={}, adjusted cost={} (* NGRAM_COST_FACTOR({}))", cost, cost* SETTINGS->ngramCostFactor, SETTINGS->ngramCostFactor);
        return cost;
    }

} // namespace lattice2

