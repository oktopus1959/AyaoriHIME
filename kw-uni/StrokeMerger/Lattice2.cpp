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
#include "KeysAndChars/EasyChars.h"

#include "MorphBridge.h"
#include "Llama/LlamaBridge.h"

#include "Lattice2_Inner.h"
#include "Lattice2_CandidateString.h"
#include "Lattice2_Kbest.h"

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

    //// システムで用意したカタカナ単語コスト(3gram以上、sysCost < uesrCost のものだけ計上)
    //inline std::map<MString, int> KatakanaWordCosts;

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

    void _updateNgramCost(const MString& word, int sysCount, int usrCount, int rtmCount) {
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
            ngramCosts[word] = _calcTrigramBonus(word, rtmCount, sysCount, origSysCnt, usrCount, origRtmCnt);

            if (IS_LOG_DEBUGH_ENABLED) {
                String ws = to_wstr(word);
                if (ws == L"ている" || ws == L"いるな" || ws == L"ていか" || ws == L"いかな") {
                    LOG_WARNH(L"word={}: sysCount adjusted: sysCnt={}, totalCost={}", ws, sysCount, ngramCosts[word]);
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
#if 0
        for (auto iter = KatakanaWordCosts.begin(); iter != KatakanaWordCosts.end(); ++iter) {
            const MString& word = iter->first;
            int cost = iter->second;
            if (cost < ngramCosts[word]) {
                // update
                ngramCosts[word] = cost;
            }
        }
#endif
        _LOG_DETAIL(L"LEAVE: ngramCosts.size={}", ngramCosts.size());
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

#if 0
    void _loadKatakanaCostFile() {
        auto path = utils::joinPath(SETTINGS->rootDir, KATAKANA_COST_FILE);
        LOG_INFOH(_T("LOAD: {}"), path.c_str());
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
        LOG_INFOH(_T("DONE"));
    }
#endif

    void __loadUserCostFile(StringRef file) {
        auto path = utils::joinPath(SETTINGS->rootDir, file);
        LOG_INFOH(_T("LOAD: {}"), path.c_str());
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
        LOG_INFOH(_T("DONE"));
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
            //_loadKatakanaCostFile();
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

    // リアルタイムNgramの更新
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
    std::wregex kanjiDateTime(L"年[一二三四五六七八九十]+月?|[一二三四五六七八九十]+月[一二三四五六七八九十]?|月[一二三四五六七八九十]+日?|[一二三四五六七八九十]+日");

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
                        if (xCost == 0) {
                            // 「M月N日」のパターンか
                            if (utils::reMatch(to_wstr(word), kanjiDateTime)) {
                                xCost = _calcTrigramBonus(word, SETTINGS->realtimeTrigramTier1Num / 2);
                            }
                        }
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

    // 交ぜ書き優先辞書の読み込み
    void loadMazegakiPriorFile() {
        auto path = utils::joinPath(SETTINGS->rootDir, MAZEGAKI_PRIOR_FILE);
        LOG_INFO(_T("LOAD: {}"), path.c_str());
        utils::IfstreamReader reader(path);
        if (reader.success()) {
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(utils::replace_all(utils::strip(utils::reReplace(line, L"#.*$", L"")), L"[ \t]+", L"\t"), '\t');
                if (!items.empty() && !items[0].empty() && items[0][0] != L'#') {
                    MString mazeFeat = to_mstr(items[0]);
                    if (items.size() >= 2 && isDecimalString(items[1])) {
                        mazegakiPriorDict[mazeFeat] = std::stoi(items[1]);
                    } else {
                        mazegakiPriorDict[mazeFeat] = -DEFAULT_WORD_BONUS;       // mazegaki優先度のデフォルトは -DEFAULT_WORD_BONUS
                    }
                }
            }
            LOG_INFO(_T("DONE: entries count={}"), mazegakiPriorDict.size());
        }
    }

    // 交ぜ書き優先度の取得
    int getMazegakiPriorityCost(const MString& mazeFeat) {
        auto iter = mazegakiPriorDict.find(mazeFeat);
        if (iter == mazegakiPriorDict.end()) {
            return 0;
        } else {
            return iter->second;
        }
    }

    // 交ぜ書き優先辞書の書き込み
    void saveMazegakiPriorFile() {
        LOG_INFO(L"CALLED: file={}, mazegakiPrior_updated={}", MAZEGAKI_PRIOR_FILE, mazegakiPrior_updated);
        auto path = utils::joinPath(SETTINGS->rootDir, MAZEGAKI_PRIOR_FILE);
        if (mazegakiPrior_updated) {
            if (utils::moveFileToBackDirWithRotation(path, SETTINGS->backFileRotationGeneration)) {
                LOG_INFO(_T("SAVE: mazegaki prior file path={}"), path.c_str());
                utils::OfstreamWriter writer(path);
                if (writer.success()) {
                    for (const auto& pair : mazegakiPriorDict) {
                        String line;
                        int count = pair.second;
                        if (count < 0 || count > 1 || (count == 1 && Reporting::Logger::IsWarnEnabled())) {
                            // count が 0 または 1 の N-gramは無視する
                            line.append(to_wstr(pair.first));           // mazeFeat
                            line.append(_T("\t"));
                            line.append(std::to_wstring(pair.second));  // ボーナス
                            writer.writeLine(utils::utf8_encode(line));
                        }
                    }
                    mazegakiPrior_updated = false;
                }
                LOG_INFO(_T("DONE: entries count={}"), mazegakiPriorDict.size());
            }
        }
    }

    // 交ぜ書き優先度の更新
    void updateMazegakiPriority(const CandidateString& raised, const CandidateString& depressed) {
        _LOG_DETAIL(L"ENTER: raised={}, depressed={}", raised.debugString(), depressed.debugString());
        if (!raised.mazeFeat().empty()) {
            mazegakiPriorDict[raised.mazeFeat()] += -DEFAULT_WORD_BONUS;
            _LOG_DETAIL(L"raised: mazegakiPriorDict[{}]={}", to_wstr(raised.mazeFeat()), mazegakiPriorDict[raised.mazeFeat()]);
            mazegakiPrior_updated = true;
        }
        if (!depressed.mazeFeat().empty()) {
            mazegakiPriorDict[depressed.mazeFeat()] += DEFAULT_WORD_BONUS;
            _LOG_DETAIL(L"depressed: mazegakiPriorDict[{}]={}", to_wstr(depressed.mazeFeat()), mazegakiPriorDict[depressed.mazeFeat()]);
            mazegakiPrior_updated = true;
        }
    }

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

    // 表層形と交ぜ書き原形などの形態素情報
    class MorphInfo {
        MString surface;
        MString mazeBase;

    };

    // Lattice
    class LatticeImpl : public Lattice2 {
        DECLARE_CLASS_LOGGER;
    private:
        // 打鍵開始位置
        int _startStrokeCount = 0;

        // K-best な文字列を格納する
        std::unique_ptr<KBestList> _kBestList;

        // 前回生成された文字列
        MString _prevOutputStr;

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
        LatticeImpl() : _kBestList(KBestList::createKBestList()) {
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
            _kBestList->clearKbests(true);
        }

        void clear() override {
            _LOG_DETAIL(_T("CALLED"));
            _startStrokeCount = 0;
            _prevOutputStr.clear();
            _kBestList->clearKbests(false);
        }

        void removeOtherThanKBest() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->removeOtherThanKBest();
        }

        void removeOtherThanFirst() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->removeOtherThanFirst();
        }

        void setKanjiPreferredNextCands() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->setKanjiPreferredNextCands();
            if (_startStrokeCount == 1) _startStrokeCount = 0;  // 先頭での漢字優先なら、0 クリアしておく(この後、clear()が呼ばれるので、それと状態を合わせるため)
        }
        void clearKanjiPreferredNextCands() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->clearKanjiPreferredNextCands();
        }

        bool isEmpty() override {
            //_LOG_DETAIL(_T("CALLED: isEmpty={}"), _kBestList->isEmpty());
            return _kBestList->isEmpty();
        }

        // 先頭候補を取得する
        MString getFirst() const override {
            _LOG_DETAIL(_T("CALLED"));
            return _kBestList->getFirst();
        }

        // 先頭候補を最優先候補にする
        void selectFirst() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->selectFirst();
        }

        // 次候補を選択する
        void selectNext() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->selectNext();
        }

        // 前候補を選択する
        void selectPrev() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->selectPrev();
        }

        // 候補選択による、リアルタイムNgramの蒿上げと抑制
        void raiseAndDepressByCandSelection() override {
            _kBestList->raiseAndDepressByCandSelection();
        }

        // 部首合成
        void updateByBushuComp() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->updateByBushuComp();
        }

        // 交ぜ書き変換
        void updateByMazegaki() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->updateByMazegaki();
        }

    public:
        // 単語素片リストの追加(単語素片が得られなかった場合も含め、各打鍵ごとに呼び出すこと)
        // 単語素片(WordPiece): 打鍵後に得られた出力文字列と、それにかかった打鍵数
        LatticeResult addPieces(const std::vector<WordPiece>& pieces, bool strokeBack, bool bKatakanaConversion) override {
            _LOG_DETAIL(_T("ENTER: pieces: {}"), formatStringOfWordPieces(pieces));
            int totalStrokeCount = (int)(STATE_COMMON->GetTotalDecKeyCount());
            if (_startStrokeCount == 0) _startStrokeCount = totalStrokeCount;
            int currentStrokeCount = totalStrokeCount - _startStrokeCount + 1;

            //LOG_DEBUGH(_T("ENTER: currentStrokeCount={}, pieces: {}\nkBest:\n{}"), currentStrokeCount, formatStringOfWordPieces(pieces), _kBestList->debugString());
            _LOG_DETAIL(_T("INPUT: _kBestList.size={}, _origFirstCand={}, totalStroke={}, currentStroke={}, strokeBack={}, rollOver={}, pieces: {}"),
                _kBestList->size(), _kBestList->origFirstCand(), totalStrokeCount, currentStrokeCount, strokeBack, STATE_COMMON->IsRollOverStroke(), formatStringOfWordPieces(pieces));

            _LOG_DETAIL(L"kBest:\n{}", _kBestList->debugCandidates(10));

            if (pieces.empty()) {
                // pieces が空になるのは、同時打鍵の途中の状態などで、文字が確定していない場合
                _LOG_DETAIL(L"LEAVE: emptyResult");
                return LatticeResult::emptyResult();
            }

            //if (pieces.size() == 1) {
            //    auto s = pieces.front().getString();
            //    if (s.size() == 1 && utils::is_punct(s[0])) {
            //        // 前回の句読点から末尾までの出力文字列に対して Ngram解析を行う
            //        _LOG_DETAIL(L"CALL lattice2::updateRealtimeNgram()");
            //        lattice2::updateRealtimeNgram();
            //    }
            //}
            // フロントエンドから updateRealtimeNgram() を呼び出すので、ここではやる必要がない

            //if (kanjiPreferredNext) {
            //    _LOG_DETAIL(L"KANJI PREFERRED NEXT");
            //    // 先頭候補だけを残す
            //    _kBestList->removeOtherThanFirst();
            //    // 次のストロークを漢字優先とする
            //    _kBestList->setKanjiPreferredNextCands();
            //    if (_startStrokeCount == 1) _startStrokeCount = 0;  // 先頭での漢字優先なら、0 クリアしておく(この後、clear()が呼ばれるので、それと状態を合わせるため)
            //}

            _LOG_DETAIL(L"_kBestList.size={}", _kBestList->size());

            //// すべての単語素片が1文字で、それが漢字・ひらがな・カタカナ以外だったら、現在の先頭候補を優先させる
            //if (!pieces.empty() && areAllPiecesNonJaChar(pieces)) {
            //    _LOG_DETAIL(L"ALL PIECES NON JA CHAR");
            //    selectFirst();
            //}
            //_LOG_DETAIL(L"_kBestList.size={}", _kBestList->size());

            // 候補リストの更新
            _kBestList->updateKBestList(pieces, currentStrokeCount, strokeBack, bKatakanaConversion);

            //LOG_DEBUGH(L"G:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            //LOG_DEBUGH(_T(".\nresult kBest:\n{}"), pKBestList->debugString());
            size_t numBS = 0;
            MString outStr = _kBestList->getTopString();
            size_t commonLen = calcCommonPrefixLenWithPrevStr(outStr);
            numBS = _prevOutputStr.size() - commonLen;
            _prevOutputStr = outStr;
            outStr = utils::safe_substr(outStr, commonLen);

            _LOG_DETAIL(_T("OUTPUT: {}, numBS={}\n\n{}"), to_wstr(outStr), numBS, _kBestList->debugKBestString());
            if (IS_LOG_DEBUGH_ENABLED) {
                while (_debugLogQueue.size() >= 10) _debugLogQueue.pop_front();
                _debugLogQueue.push_back(std::format(L"========================================\nENTER: currentStrokeCount={}, pieces: {}\n",
                    currentStrokeCount, formatStringOfWordPieces(pieces)));
                if (pieces.back().numBS() <= 0) {
                    _debugLogQueue.push_back(std::format(L"\n{}\nOUTPUT: {}, numBS={}\n\n", _kBestList->debugKBestString(SETTINGS->multiStreamBeamSize), to_wstr(outStr), numBS));
                }
            }

            //LOG_DEBUGH(L"H:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            // 解候補を仮想鍵盤に表示する
            std::vector<MString> candStrings = _kBestList->getTopCandStrings();
            if (candStrings.size() > 1) {
                STATE_COMMON->SetVirtualKeyboardStrings(VkbLayout::MultiStreamCandidates, EMPTY_MSTR, candStrings);
                STATE_COMMON->SetWaitingCandSelect(_kBestList->selectedCandPos());
            }
            //LOG_DEBUGH(L"I:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));

            _LOG_DETAIL(_T("LEAVE:\nkanjiPreferredNextCands={}\nkBest:\n{}"), _kBestList->kanjiPreferredNextCandsDebug(),  _kBestList->debugCandidates(10));
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

DEFINE_CLASS_LOGGER(Lattice2);

std::unique_ptr<Lattice2> Lattice2::Singleton;

void Lattice2::createLattice() {
    LOG_INFOH(L"ENTER");
    lattice2::loadCostAndNgramFile();
    lattice2::loadMazegakiPriorFile();
    Singleton.reset(new lattice2::LatticeImpl());
    LOG_INFOH(L"LEAVE");
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

//void Lattice2::saveRealtimeNgramFile() {
//    lattice2::saveRealtimeNgramFile();
//}

void Lattice2::saveLatticeRelatedFiles() {
    lattice2::saveRealtimeNgramFile();
    lattice2::saveMazegakiPriorFile();
}

void Lattice2::reloadGlobalPostRewriteMapFile() {
    lattice2::readGlobalPostRewriteMapFile();
}

