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

    // 利用者の選択によって嵩上げされるNgramに課されるボーナスを計算するためのベースとなるカウント
    std::map<MString, int> realtimeNgramBonusCounts;

    // Ngram頻度がオンラインで更新されたか
    bool realtimeNgram_updated = false;

    //--------------------------------------------------------------------------------------
#if 0
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
#endif

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

    void loadCostAndNgramFile(bool bReload) {
        LOG_INFO(L"ENTER: bReload={}", bReload);
        if (bReload) {
            realtimeNgramBonusCounts.clear();
        }
        _loadNgramFile(REALTIME_NGRAM_FILE, realtimeNgramBonusCounts);
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
                    for (const auto& pair : realtimeNgramBonusCounts) {
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
                LOG_SAVE_DICT(_T("DONE: entries count={}"), realtimeNgramBonusCounts.size());
            }
            // pathTmp ファイルのサイズが path ファイルのサイズよりも小さい場合は、書き込みに失敗した可能性があるので、既存ファイルを残す
            utils::compareAndMoveFileToBackDirWithRotation(pathTmp, path, SETTINGS->backFileRotationGeneration);
        }
#endif
        LOG_SAVE_DICT(L"LEAVE: file={}", REALTIME_NGRAM_FILE);
    }

    //inline bool is_space_or_vbar(mchar_t ch) {
    //    return ch == ' ' || ch == '|';
    //}

    void _updateRealtimeNgramCountByWord(bool bIncrease, const MString& word, bool manualSelect) {
        int delta = manualSelect ? SETTINGS->ngramManualSelectDelta : 1;
        if (!bIncrease) {
            delta = -delta;
        }
        realtimeNgramBonusCounts[word] += delta;
        realtimeNgram_updated = true;
    }

    // リアルタイムNgramの更新
    void _updateRealtimeNgram(bool bIncrease, const MString& str, bool bManual) {
        LOG_DEBUGH(L"ENTER: bIncrease={}, str={}, byGUI={}", bIncrease, to_wstr(str), bManual);
        int strlen = (int)str.size();
        int hirakanLen = 0;
        int kanjiLen = 0;
        for (int pos = 0; pos < strlen; ++pos) {
            int charLen = 0;
            if (utils::is_hiragana(str[pos])) {
                kanjiLen = 0;
                ++hirakanLen;
            } else if (utils::is_kanji(str[pos])) {
                ++kanjiLen;
                ++hirakanLen;
            } else {
                hirakanLen = 0;
                kanjiLen = 0;
            }
            if (hirakanLen >= 3 && pos >= 2) {
                // ひらがなor漢字が3文字以上連続している場合は、3gramを更新する
                _updateRealtimeNgramCountByWord(bIncrease, str.substr(pos - 2, 3), bManual);
            }
            if (kanjiLen >= 2 && pos >= 1) {
                // 漢字が2文字以上連続している場合は、2gramを更新する
                _updateRealtimeNgramCountByWord(bIncrease, str.substr(pos - 1, 2), bManual);
            }
        }
        LOG_DEBUGH(L"LEAVE: str={}", to_wstr(str));
    }

    // 編集バッファのフラッシュによるリアルタイムNgramの更新
    void updateRealtimeNgram(const MString& str) {
        _updateRealtimeNgram(true, str, false);
    }

    // リアルタイムNgramの蒿上げ
    void increaseRealtimeNgram(const MString& str, bool bManual) {
        _updateRealtimeNgram(true, str, bManual);
    }

    // リアルタイムNgramの抑制
    void decreaseRealtimeNgram(const MString& str, bool bManual) {
        _updateRealtimeNgram(false, str, bManual);
    }

    // 候補選択による、リアルタイムNgramの増加・減少
    void _updateRealtimeNgramForDiffPart(bool bIncrease, const MString& baseCand, const MString& diffCand) {
        LOG_WARNH(L"ENTER: bIncrease={}, baseCand={}, diffCand={}", bIncrease, to_wstr(baseCand), to_wstr(diffCand));
        int prevDiffPos = -10;
        int baseSize = (int)baseCand.size();
        int diffSize = (int)diffCand.size();
        for (int pos = 0; pos < diffSize; ++pos) {
            bool currentDiff = pos < baseSize || baseCand[pos] != diffCand[pos];
            if (currentDiff || prevDiffPos + 2 == pos) {
                if (pos >= 2) {
                    // 2文字前から3gramについて更新する
                    _updateRealtimeNgramCountByWord(bIncrease, diffCand.substr(pos - 2, 3), true);
                }
            }
            if (currentDiff || prevDiffPos + 1 == pos) {
                if (pos >= 1) {
                    if (utils::is_kanji(diffCand[pos - 1]) || pos + 1 == diffSize) {
                        // 1文字前が漢字あるいは当位置で文字列が終了の場合は、2gramも更新する
                        _updateRealtimeNgramCountByWord(bIncrease, diffCand.substr(pos - 1, 2), true);
                    }
                }
            }
            if (currentDiff) {
                prevDiffPos = pos;
            }
        }
        LOG_WARNH(L"LEAVE");
    }

    //// 候補選択による、リアルタイムNgramの嵩上げ
    //void _increaseRealtimeNgramForDiffPart(const MString& oldCand, const MString& newCand) {
    //    _updateRealtimeNgramForDiffPart(true, oldCand, newCand);
    //}

    //// 候補選択による、リアルタイムNgramの抑制
    //void _decreaseRealtimeNgramForDiffPart(const MString& oldCand, const MString& newCand) {
    //    _updateRealtimeNgramForDiffPart(false, newCand, oldCand);
    //}

    // 候補選択による、リアルタイムNgramの蒿上げと抑制
    void updateRealtimeNgramForDiffPart(const MString& oldCand, const MString& newCand) {
        LOG_WARNH(L"ENTER: oldCand={}, newCand={}", to_wstr(oldCand), to_wstr(newCand));
        //_increaseRealtimeNgramForDiffPart(oldCand, newCand);
        _updateRealtimeNgramForDiffPart(true, oldCand, newCand);
        //_decreaseRealtimeNgramForDiffPart(oldCand, newCand);
        _updateRealtimeNgramForDiffPart(false, newCand, oldCand);
        LOG_WARNH(L"LEAVE");
    }

    int getRealtimeNgramBonus(const MString& word) {
        auto iter = realtimeNgramBonusCounts.find(word);
        if (iter != realtimeNgramBonusCounts.end()) {
            int bonusCount = iter->second;
            if (bonusCount > 0) {
                int bonusPoint = bonusCount * SETTINGS->ngramBonusPointFactor;
                _LOG_DETAIL(L"{}: bonusCount={}, bonusPoint={}", to_wstr(word), bonusCount, bonusPoint);
                return bonusPoint;
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
                cost -= getRealtimeNgramBonus(utils::safe_substr(str, pos, 2));
                if (pos + 2 < str.size()) {
                    cost -= getRealtimeNgramBonus(utils::safe_substr(str, pos, 3));
                }
            }
        }
        _LOG_DETAIL(L"LEAVE: cost={}, adjusted cost={} (* NGRAM_COST_FACTOR({}))", cost, cost* SETTINGS->ngramCostFactor, SETTINGS->ngramCostFactor);
        return cost;
    }

} // namespace lattice2

