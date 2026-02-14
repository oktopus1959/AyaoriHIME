#include "Logger.h"
#include "file_utils.h"
#include "string_utils.h"

#include "settings.h"

#include "Lattice2_Common.h"
#include "Lattice2_Ngram.h"

#include "Ngram/NgramBridge.h"

namespace lattice2 {
    DECLARE_LOGGER;     // defined in Lattice2.cpp

//#define SYSTEM_NGRAM_FILE           JOIN_USER_FILES_FOLDER(L"mixed_all.ngram.txt")
//#define KATAKANA_COST_FILE          JOIN_USER_FILES_FOLDER(L"katakana.cost.txt")
#define REALTIME_NGRAM_MAIN_FILE    JOIN_USER_FILES_FOLDER(L"realtime.ngram.txt")
#define REALTIME_NGRAM_TEMP_FILE    JOIN_USER_FILES_FOLDER(L"realtime.ngram.tmp.txt")
#define SELECTED_NGRAM_FILE         JOIN_USER_FILES_FOLDER(L"selected.ngram.txt")
#define USER_NGRAM_FILE             JOIN_USER_FILES_FOLDER(L"user.ngram.txt")
//#define USER_COST_FILE              JOIN_USER_FILES_FOLDER(L"userword.cost.txt")

    const static int DATE_PATTERN_BONUMS_POINT = 100;

    // 利用者の選択によって嵩上げされるNgramに課されるボーナスを計算するためのベースとなるカウント
    std::map<MString, int> realtimeNgramBonusCounts;

    // Ngram頻度がオンラインで更新されたか
    bool realtimeNgram_updated = false;

    // 利用者定義のボーナスを計算するためのカウント
    std::map<MString, int> userNgramBonusCounts;

    // ボーナスの加減算の対象にならない固定Ngramリスト (user.ngram.txt で、カウントの先頭が FIXED_NGRAM_MARKER のもの)
    std::map<MString, int> fixedNgramBonusCounts;

    const static wchar_t FIXED_NGRAM_MARKER = L'$';

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
        LOG_INFOH(_T("LOAD: {}"), path.c_str());
        int maxFreq = 0;
        utils::IfstreamReader reader(path);
        if (reader.success()) {
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(utils::replace_all(utils::strip(line), L" +", L"\t"), '\t');
                if (items.size() == 2 &&
                    items[0].size() >= 2 && items[0].size() <= 3 && items[0][0] != L'#' &&         // 2-3gramに限定
                    !items[1].empty()) {

                    MString word = to_mstr(items[0]);
                    String sCount = items[1];
                    if (sCount.size() >= 2 && sCount[0] == FIXED_NGRAM_MARKER && isDecimalString(sCount.substr(1))) {
                        // 固定Ngram
                        int count = std::stoi(sCount.substr(1));
                        if (count == 0) count = 1;   // 0 は 1 に補正
                        _LOG_DETAIL(L"Fixed Ngram: word={}, count={}", to_wstr(word), count);
                        fixedNgramBonusCounts[word] = count;
                    } else if (isDecimalString(sCount)) {
                        int count = std::stoi(sCount);
                        ngramMap[word] = count;
                        if (maxFreq < count) maxFreq = count;
                    }
                }
            }
        }
        LOG_INFOH(_T("DONE: maxFreq={}"), maxFreq);
        return maxFreq;
    }

    String SelectedNgramPairBonus::debugString() const {
        return std::format(L"NgramPair='{}', BonusPoint={}", to_wstr(ngramPair), bonusPoint);
    }

    String debugStringOfSelectedNgramPairBonusSet(const std::set<SelectedNgramPairBonus>& s) {
        String result;
        for (const auto& item : s) {
            result.append(item.debugString() + L"\n");
        }
        return result;
    }

    const size_t MAX_SELECTED_NGRAM_LEN = 5;

    // ユーザー選択によるポジティブ|ネガティブNgram対を扱うクラス
    class SelectedNgram {
        std::map<MString, int> selectedNgrams;

        std::map<MString, std::set<SelectedNgramPairBonus>> selectedNgramMap;

        bool bUpdated = false;

    public:
        // ユーザー選択によるポジティブ|ネガティブNgram対の読み込み
        // 形式: <Positive Ngram>|<Negative Ngram> <TAB> <ボーナスポイント>
        void loadSelectedNgramFile(StringRef ngramFile) {
            selectedNgramMap.clear();
            auto path = utils::joinPath(SETTINGS->rootDir, ngramFile);
            LOG_INFOH(_T("LOAD SELECTED: {}"), path.c_str());
            utils::IfstreamReader reader(path);
            if (reader.success()) {
                size_t count = 0;
                for (const auto& line : reader.getAllLines()) {
                    auto items = utils::split(utils::replace_all(utils::strip(line), L" +", L"\t"), '\t');
                    if (items.size() == 2 &&
                        !items[0].empty() && items[0][0] != L'#' &&
                        !items[1].empty() && isDecimalString(items[1])) {
                        MString pair = to_mstr(items[0]);
                        auto words = utils::split(pair, '|');
                        if (words.size() == 2 && !words[0].empty() && !words[1].empty()) {
                            int point = std::stoi(items[1]);
                            selectedNgrams[pair] = point;
                            selectedNgramMap[words[0]].insert(SelectedNgramPairBonus{ pair, point });      // positive ngram
                            selectedNgramMap[words[1]].insert(SelectedNgramPairBonus{ pair, -point });     // negative ngram
                            if (count < 10) {
                                _LOG_DETAIL(L"Selected Ngram Pair: {} => point={}", to_wstr(pair), point);
                            }
                        }
                    }
                }
            }
            LOG_INFOH(_T("DONE"));
        }

        void saveSelectedNgramFile(StringRef ngramFile) {
            if (bUpdated) {
                LOG_INFOH(_T("Selected Ngram updated. Saving to file..."));
                auto path = utils::joinPath(SETTINGS->rootDir, ngramFile);
                LOG_INFOH(_T("SAVE SELECTED: {}"), path.c_str());
                utils::OfstreamWriter writer(path);
                if (writer.success()) {
                    for (const auto& entry : selectedNgrams) {
                        writer.writeLine(std::format(L"{}\t{}", to_wstr(entry.first), entry.second));
                    }
                }
                bUpdated = false;
                LOG_INFOH(_T("DONE"));
            }
        }

        void updateSelectedNgram(const MString& posi, const MString& nega) {
            auto key = nega + MSTR_VERT_BAR + posi;
            auto iter = selectedNgrams.find(key);
            int bonusPoint = 0;
            if (iter != selectedNgrams.end()) {
                iter->second -= SETTINGS->ngramManualSelectDelta;
                bonusPoint = iter->second;
                bUpdated = true;
            } else {
                key = posi + MSTR_VERT_BAR + nega;
                iter = selectedNgrams.find(key);
                if (iter != selectedNgrams.end()) {
                    iter->second += SETTINGS->ngramManualSelectDelta;
                    bonusPoint = iter->second;
                    bUpdated = true;
                } else {
                    // 新規追加
                    bonusPoint = SETTINGS->ngramManualSelectDelta;
                    selectedNgrams[key] = bonusPoint;
                    selectedNgramMap[posi].insert(SelectedNgramPairBonus{ key, SETTINGS->ngramManualSelectDelta });      // positive ngram
                    selectedNgramMap[nega].insert(SelectedNgramPairBonus{ key, -SETTINGS->ngramManualSelectDelta });     // negative ngram
                    bUpdated = true;
                }
            }
            _LOG_DETAIL(L"key={}, bonusPoint={}", to_wstr(key), bonusPoint);
        }

        //void updateSelectedNgram(const MString& posi, const MString& nega, bool geta) {
        //    if (geta) {
        //        _updateSelectedNgram(MSTR_GETA + posi, MSTR_GETA + nega);
        //    }
        //    _updateSelectedNgram(posi, nega);
        //}

        void gatherSelectedNgramPairBonus(std::set<SelectedNgramPairBonus>& resultSet, const MString& ngram) const {
            auto iter = selectedNgramMap.find(ngram);
            if (iter != selectedNgramMap.end()) {
                resultSet.insert(iter->second.begin(), iter->second.end());
                _LOG_DETAIL(L"FOUND: ngram={}, set<SelectedNgramPairBonus>: {}", to_wstr(ngram), debugStringOfSelectedNgramPairBonusSet(resultSet));
            }
        }

        // 指定された文字列に対して、そこに含まれるNgramに対応する選択Ngramペアボーナスを取得
        // Ngramは、2~5文字
        const std::set<SelectedNgramPairBonus> findNgramPairBonus(const MString& str) {
            std::set<SelectedNgramPairBonus> resultSet;
            for (size_t i = 0; i < str.size(); ++i) {
                if (i == 0) {
                    // 先頭文字の場合は、〓付きも調べる (例: 〓池 vs 〓での)
                    for (size_t len = 1; len <= MAX_SELECTED_NGRAM_LEN && i + len <= str.size(); ++len) {
                        gatherSelectedNgramPairBonus(resultSet, MSTR_GETA + str.substr(i, len));
                    }
                }
                for (size_t len = 2; len <= MAX_SELECTED_NGRAM_LEN && i + len <= str.size(); ++len) {
                    gatherSelectedNgramPairBonus(resultSet, str.substr(i, len));
                }
            }
            return resultSet;
        }
    };

    SelectedNgram selectedNgramInstance;

    // 指定された文字列に対して、そこに含まれるNgramに対応する選択Ngramペアボーナスを取得 (Ngramは、2~5文字)
    const std::set<SelectedNgramPairBonus> findNgramPairBonus(const MString& str) {
        return selectedNgramInstance.findNgramPairBonus(str);
    }

    // 2つの文字列の最初の差分部分を見つける
    // 戻り値: (startPos, endPos1, endPos2) (「先頭共通部の直後から、次に同じ文字で再同期できる最初の位置まで」を差分として返す)
    std::tuple<size_t, size_t, size_t> findFirstDiff(const MString& s1, const MString& s2) {
        size_t len1 = s1.size();
        size_t len2 = s2.size();
        size_t startPos = 0;
        while (startPos < len1 && startPos < len2 && s1[startPos] == s2[startPos]) {
            ++startPos;
        }
        if (startPos < len1 && startPos < len2) {
            size_t endPos = startPos + 1;
            while (endPos < len1 && endPos < len2) {
                if (s1[endPos] == s2[endPos]) {
                    return { startPos, endPos, endPos };
                }
                size_t pos = endPos - 1;
                while (pos > startPos) {
                    if (s1[pos] == s2[endPos]) {
                        return { startPos, pos, endPos };
                    }
                    if (s1[endPos] == s2[pos]) {
                        return { startPos, endPos, pos };
                    }
                    --pos;
                }
                ++endPos;
            }
        }
        return { startPos, len1, len2 };
    }

    // 候補選択による、SelectedNgramの更新
    void updateSelectedNgramByUserSelect(const MString& oldCand, const MString& newCand) {
        LOG_INFOH(L"ENTER: oldCand={}, newCand={}", to_wstr(oldCand), to_wstr(newCand));
        size_t baseSize = oldCand.size();
        size_t diffSize = newCand.size();
        LOG_DEBUGH(L"baseSize={}, diffSize={}", baseSize, diffSize);
        auto [startPos, endPos1, endPos2] = findFirstDiff(oldCand, newCand);
        if (startPos < endPos1 && startPos < endPos2) {
            size_t len1 = endPos1 - startPos;
            size_t len2 = endPos2 - startPos;
            if (len1 == 1 || len2 == 1) {
                // 1文字差分の場合は、前後の1文字を含めて更新する
                if (startPos == 0) {
                    // 先頭だったら、〓を前に追加して処理する
                    if (len1 == 1 && len2 == 1) {
                        if (len1 + 1 <= baseSize && len2 + 1 <= diffSize) {
                            // 両者ともに1文字差分の場合は、後ろ1文字も含めて処理する
                            selectedNgramInstance.updateSelectedNgram(
                                MSTR_GETA + newCand.substr(startPos, len2 + 1),
                                MSTR_GETA + oldCand.substr(startPos, len1 + 1));
                        }
                    } else {
                        selectedNgramInstance.updateSelectedNgram(
                            MSTR_GETA + newCand.substr(startPos, len2),
                            MSTR_GETA + oldCand.substr(startPos, len1));
                    }
                }
                ++len1;
                ++len2;
                if (startPos > 0) {
                    selectedNgramInstance.updateSelectedNgram(
                        newCand.substr(startPos - 1, len2),
                        oldCand.substr(startPos - 1, len1));
                }
                if (startPos + len1 <= baseSize && startPos + len2 < diffSize) {
                    selectedNgramInstance.updateSelectedNgram(
                        newCand.substr(startPos, len2),
                        oldCand.substr(startPos, len1));
                }
            } else if (len1 <= MAX_SELECTED_NGRAM_LEN && len2 <= MAX_SELECTED_NGRAM_LEN) {
                // 2~5文字の差分の場合は、そのまま更新する
                selectedNgramInstance.updateSelectedNgram(
                    newCand.substr(startPos, len2),
                    oldCand.substr(startPos, len1));
            }
        }
        LOG_INFOH(L"LEAVE");
    }

#define REALTIME_NGRAM_FILE (SETTINGS->useTmpRealtimeNgramFile ? REALTIME_NGRAM_TEMP_FILE : REALTIME_NGRAM_MAIN_FILE)

    void loadNgramFiles() {
        LOG_INFO(L"ENTER");
        realtimeNgramBonusCounts.clear();
        _loadNgramFile(REALTIME_NGRAM_FILE, realtimeNgramBonusCounts);
        fixedNgramBonusCounts.clear();
        userNgramBonusCounts.clear();
        _loadNgramFile(USER_NGRAM_FILE, userNgramBonusCounts);
        selectedNgramInstance.loadSelectedNgramFile(SELECTED_NGRAM_FILE);
        maxCandidatesSize = 0;
        LOG_INFO(L"LEAVE");
    }

    // リアルタイムNgramファイルの保存
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
                        //int count = pair.second;
                        //if (count < 0 || count > 1 || (count == 1 && Reporting::Logger::IsWarnEnabled())) {
                            // count が 0 または 1 の N-gramは無視する
                            line.append(to_wstr(pair.first));           // 単語
                            line.append(_T("\t"));
                            line.append(std::to_wstring(pair.second));  // カウント
                            writer.writeLine(utils::utf8_encode(line));
                        //}
                    }
                    realtimeNgram_updated = false;
                }
                LOG_SAVE_DICT(_T("DONE: entries count={}"), realtimeNgramBonusCounts.size());
            }
            // pathTmp ファイルのサイズが path ファイルのサイズよりも小さい場合は、書き込みに失敗した可能性があるので、既存ファイルを残す
            utils::compareAndMoveFileToBackDirWithRotation(pathTmp, path, SETTINGS->backFileRotationGeneration);
        }

        selectedNgramInstance.saveSelectedNgramFile(SELECTED_NGRAM_FILE);
#endif
        LOG_SAVE_DICT(L"LEAVE: file={}", REALTIME_NGRAM_FILE);
    }

    //inline bool is_space_or_vbar(mchar_t ch) {
    //    return ch == ' ' || ch == '|';
    //}

    int _getFixedNgramBonusPoint(const MString& word) {
        int bonusPoint = 0;
        auto iter = fixedNgramBonusCounts.find(word);
        if (iter != fixedNgramBonusCounts.end()) {
            bonusPoint = iter->second;
        } else if (word.size() > 2) {
            // 3文字以上のNgramについて、先頭または末尾2文字が固定Ngramである場合、そのボーナスを適用する
            MString head2 = word.substr(0, 2);
            iter = fixedNgramBonusCounts.find(head2);
            if (iter != fixedNgramBonusCounts.end()) {
                bonusPoint = iter->second;
            } else {
                MString tail2 = word.substr(word.size() - 2, 2);
                iter = fixedNgramBonusCounts.find(tail2);
                if (iter != fixedNgramBonusCounts.end()) {
                    bonusPoint = iter->second;
                }
            }
        }
        _LOG_DETAIL(L"{}: bonusPoint={}", to_wstr(word), bonusPoint);
        return bonusPoint;
    }

    void _updateRealtimeNgramCountByWord(bool bIncrease, const MString& word, bool manualSelect) {
        if (manualSelect) {
            if (_getFixedNgramBonusPoint(word) != 0) {
                _LOG_DETAIL(L"Manual select: {} is FIXED ngram, ignored.", to_wstr(word));
                return;
            }
        }
        int delta = manualSelect ? SETTINGS->ngramManualSelectDelta : 1;
        if (!bIncrease) {
            delta = -delta;
        }
        int count = realtimeNgramBonusCounts[word] += delta;
        realtimeNgram_updated = true;
        if (manualSelect) {
            _LOG_DETAIL(L"Manual select: realtimeNgramBonusCounts[{}] = {}, delta={}", to_wstr(word), count, delta);
        } else {
            LOG_DEBUGH(L"Auto update: realtimeNgramBonusCounts[{}] = {}, delta={}", to_wstr(word), count, delta);
        }
    }

    // リアルタイムNgramの更新
    void _updateRealtimeNgram(bool bIncrease, const MString& str, bool bManual) {
        LOG_DEBUGH(L"ENTER: bIncrease={}, str={}, bManual={}", bIncrease, to_wstr(str), bManual);
        int strlen = (int)str.size();
        int hirakanLen = 0;
        int kanjiLen = 0;
        for (int pos = 0; pos < strlen; ++pos) {
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

    int _getNgramBonusPoint(const MString& word, std::map<MString, int>& ngramMap) {
        auto iter = ngramMap.find(word);
        if (iter != ngramMap.end()) {
            int bonusPoint = iter->second;
            if (bonusPoint > 0) {
                _LOG_DETAIL(L"{}: bonusPoint={}", to_wstr(word), bonusPoint);
                return bonusPoint;
            }
        }
        return 0;
    }

    // realtimeおよびユーザー定義によるNgramのボーナスを取得
    int getNgramBonus(const MString& word) {
        int bonusPoint0 = _getFixedNgramBonusPoint(word);
        if (bonusPoint0 == 0) {
            bonusPoint0 = _getNgramBonusPoint(word, realtimeNgramBonusCounts) + _getNgramBonusPoint(word, userNgramBonusCounts);
        }
        int bonusPoint = bonusPoint0;
        if (bonusPoint > SETTINGS->ngramMaxBonusPoint) {
            bonusPoint = SETTINGS->ngramMaxBonusPoint;
        } else if (bonusPoint < -SETTINGS->ngramMaxBonusPoint) {
            bonusPoint = -SETTINGS->ngramMaxBonusPoint;
        }
        if (bonusPoint != bonusPoint0) {
            _LOG_DETAIL(L"bonusPoint CLIPPED from {} to {}", bonusPoint0, bonusPoint);
        }
        int bonus = calcNgramBonus(bonusPoint);
        _LOG_DETAIL(L"BONUS={}: bonusPoint={} (orig={})", bonus, bonusPoint, bonusPoint0);
        return bonus;
    }

    //int getWordConnCost(const MString& s1, const MString& s2) {
    //    return get_base_ngram_cost(utils::last_substr(s1, 1) + utils::safe_substr(s2, 0, 1)) / 2;
    //}

    MString kanjiNumChars = to_mstr(L"一二三四五六七八九十〇");
    std::wregex kanjiDateTime(L"^[一二三四五六七八九十〇](年[一二三四五六七八九十〇]+月([一二三四五六七八九十〇]+日)?|月[一二三四五六七八九十〇]+日|[一二三四五六七八九十〇]日)");
    
    size_t datePatternMatchedLen(const MString& str, size_t pos) {
        if (pos < str.size() && kanjiNumChars.find(str[pos]) != MString::npos) {
            std::wsmatch match;
            std::wstring wstr = to_wstr(str.substr(pos));
            if (std::regex_search(wstr, match, kanjiDateTime)) {
                return match.length(0);
            }
        }
        return 0;
    }

    std::pair<int, size_t> findMatchedNgramPos(const MString& str, size_t startPos, size_t endPos, size_t n) {
        size_t strlen = str.size();
        if (endPos > strlen) {
            endPos = strlen;
        }
        int bonus = 0;
        for (size_t pos = startPos; pos + n <= endPos; ++pos) {
            MString subStr = utils::safe_substr(str, pos, n);
            bonus = getNgramBonus(subStr);
            if (bonus != 0) {
                _LOG_DETAIL(L"Matched {}gram: str={}, pos={}, word={}, bonus={}", n, to_wstr(str), pos, to_wstr(subStr), bonus);
                return { bonus, pos };
            }
        }
        return { 0, endPos };
    }

    // Ngramコストの取得
    int getNgramCost(const MString& str, const std::vector<MString>& morphs, bool bUseGeta) {
        _LOG_DETAIL(L"\nENTER: str={}: geta={}", to_wstr(str), bUseGeta);
        std::vector<MString> ngrams;
        MString targetStr = bUseGeta ? MSTR_GETA + str : str;
        // Ngram 解析
        int cost0 = NgramBridge::ngramCalcCost(targetStr, morphs, ngrams, SETTINGS->multiStreamDetailLog);
        int cost = cost0;
        _LOG_DETAIL(L"ngrams: initial cost={}\n--------\n{}\n--------", cost, to_wstr(utils::join(ngrams, '\n')));
        if (targetStr.size() >= 2) {
            for (size_t pos = 0; pos < targetStr.size() - 1; ++pos) {
                // realtimeおよびユーザー定義による2gramと3gramのボーナスを差し引く
                int bonus = 0;
                int bonus2 = getNgramBonus(utils::safe_substr(targetStr, pos, 2));
                int bonus3 = 0;
                if (pos + 2 < targetStr.size()) {
                    bonus3 = getNgramBonus(utils::safe_substr(targetStr, pos, 3));
                    if (bonus3 == 0) {
                        // 3gramボーナスがなければ、「M月N日」パターンに該当するか確認
                        if (utils::reMatch(to_wstr(targetStr), kanjiDateTime)) {
                            bonus3 = calcNgramBonus(DATE_PATTERN_BONUMS_POINT);
                        }
                    }
                }
                bonus = bonus2 + bonus3;
                if (bonus2 != 0 && bonus3 != 0) {
                    // 2gramと3gramの両方にボーナスがある場合は平均を取る
                    bonus /= 2;
                }
                cost -= bonus;
                _LOG_DETAIL(L"pos={}, word={}, cost={}, bonus={} (2gram={}, 3gram={})", pos, to_wstr(utils::safe_substr(targetStr, pos, 3)), cost, bonus, bonus2, bonus3);
            }
        }
        _LOG_DETAIL(L"LEAVE: str={}: initialCost={}, resultCost={}, adjustedCost={} (* NGRAM_COST_FACTOR({}))\n",
            to_wstr(str), cost0, cost, cost * SETTINGS->ngramCostFactor, SETTINGS->ngramCostFactor);
        return cost;
    }

} // namespace lattice2
