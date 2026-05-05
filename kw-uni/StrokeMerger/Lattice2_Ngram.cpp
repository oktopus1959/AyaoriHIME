#include "Logger.h"
#include "file_utils.h"
#include "string_utils.h"

#include "settings.h"

#include "Lattice2_Common.h"
#include "Lattice2_Ngram.h"

#include "Ngram/NgramBridge.h"

namespace {
    DEFINE_LOGGER(Lattice2_Ngram);
}

namespace lattice2 {

//#define SYSTEM_NGRAM_FILE           JOIN_USER_FILES_FOLDER(L"mixed_all.ngram.txt")
//#define KATAKANA_COST_FILE          JOIN_USER_FILES_FOLDER(L"katakana.cost.txt")
#define REALTIME_NGRAM_MAIN_FILE    JOIN_USER_FILES_FOLDER(L"realtime.ngram.txt")
#define REALTIME_NGRAM_TEMP_FILE    JOIN_USER_FILES_FOLDER(L"realtime.ngram.txt.tmp")
#define SELECTED_NGRAM_MAIN_FILE    JOIN_USER_FILES_FOLDER(L"selected.ngram.txt")
#define SELECTED_NGRAM_TEMP_FILE    JOIN_USER_FILES_FOLDER(L"selected.ngram.txt.tmp")
#define USER_NGRAM_MAIN_FILE        JOIN_USER_FILES_FOLDER(L"user.ngram.txt")
#define USER_NGRAM_TEMP_FILE        JOIN_USER_FILES_FOLDER(L"user.ngram.txt.tmp")
//#define USER_COST_FILE              JOIN_USER_FILES_FOLDER(L"userword.cost.txt")

    const static int DATE_PATTERN_BONUMS_POINT = 100;

    const int minRealtimeNgramLen = 1;  // 最小N-gram長
    const int maxRealtimeNgramLen = 4;  // 最大N-gram長

    // Ngram差分の登録対象にならない固定Ngram
    std::set<MString> fixedNgramEntries;

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

    // リアルタイムNgram辞書のパラメータ設定
    void setRealtimeDictParameters() {
        NgramBridge::setRealtimeDictParameters(minRealtimeNgramLen, maxRealtimeNgramLen, SETTINGS->ngramMaxBonusPoint, SETTINGS->ngramBonusPointFactor);
    }

    // Ngram差分の登録対象にならない固定Ngramをユーザー定義Ngramファイルから読み込む
    int _loadFixedNgramFile(StringRef ngramFilePath) {
        LOG_INFOH(_T("LOAD: {}"), ngramFilePath.c_str());
        size_t nEntries = 0;
        utils::IfstreamReader reader(ngramFilePath);
        if (reader.success()) {
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(utils::replace_all(utils::strip(line), L" +", L"\t"), '\t');
                if (items.size() >= 1 && !items[0].empty() && items[0][0] != L'#') {
                    MString word = to_mstr(items[0]);
                    _LOG_DETAIL(L"Fixed ngram entry: {}", to_wstr(word));
                    fixedNgramEntries.insert(word);
                    fixedNgramEntries.insert(MSTR_GETA + word);
                    ++nEntries;
                }
            }
        }
        LOG_INFOH(_T("DONE: nEntries={}"), nEntries);
        return nEntries;
    }

    bool isFixedNgramEntry(const MString& word) {
        auto iter = fixedNgramEntries.find(word);
        return iter != fixedNgramEntries.end();
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

    const size_t MAX_SELECTED_NGRAM_LEN = 8;

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
                            if (!isFixedNgramEntry(words[0]) && !isFixedNgramEntry(words[1])) {
                                // 固定Ngramエントリが含まれていない場合のみ、選択Ngramペアとして登録
                                int point = std::stoi(items[1]);
                                selectedNgrams[pair] = point;
                                selectedNgramMap[words[0]].insert(SelectedNgramPairBonus{ pair, point });      // positive ngram
                                selectedNgramMap[words[1]].insert(SelectedNgramPairBonus{ pair, -point });     // negative ngram
                                if (count++ < 100) {
                                    _LOG_DETAIL(L"Selected Ngram Pair: {} => point={}", to_wstr(pair), point);
                                }
                            } else {
                                _LOG_DETAIL(L"Skipped fixed ngram entry in selected ngram pair: {}", to_wstr(pair));
                            }
                        }
                    }
                }
            }
            LOG_INFOH(_T("DONE"));
        }

        void saveSelectedNgramFile(StringRef ngramFile, int genNum) {
            if (bUpdated) {
                LOG_INFOH(_T("Selected Ngram updated. Saving to file..."));
                auto path = utils::joinPath(SETTINGS->rootDir, ngramFile);
                // 一旦、一時ファイルに書き込み
                auto pathTmp = path + L".tmp";
                LOG_INFOH(_T("SAVE SELECTED: {}"), pathTmp.c_str());
                {
                    utils::OfstreamWriter writer(pathTmp);
                    if (writer.success()) {
                        for (const auto& entry : selectedNgrams) {
                            writer.writeLine(std::format(L"{}\t{}", to_wstr(entry.first), entry.second));
                        }
                        bUpdated = false;
                    }
                    LOG_INFOH(_T("DONE"));
                }
                // pathTmp ファイルのサイズが pathTmp ファイルのサイズよりも小さい場合は、書き込みに失敗した可能性があるので、既存ファイルを残す
                utils::compareAndMoveFileToBackDirWithRotation(pathTmp, path, genNum);
            }
        }

        void updateSelectdNgramMap(const MString& key, const MString& posi, const MString& nega, int bonusPoint) {
            _LOG_DETAIL(L"key={}, pos={}, nega={}, bonusPoint={}", to_wstr(key), to_wstr(posi), to_wstr(nega), bonusPoint);
            auto pred = [&key](const SelectedNgramPairBonus& item) { return item.ngramPair == key; };
            if (selectedNgramMap.contains(posi)) std::erase_if(selectedNgramMap[posi], pred);
            if (selectedNgramMap.contains(nega)) std::erase_if(selectedNgramMap[nega], pred);
            selectedNgramMap[posi].insert(SelectedNgramPairBonus{ key, bonusPoint });       // positive ngram
            selectedNgramMap[nega].insert(SelectedNgramPairBonus{ key, -bonusPoint });      // negative ngram
        }

        // Ngram差分の更新
        void updateSelectedNgram(const MString& posi, const MString& nega) {
            if (isFixedNgramEntry(posi) && isFixedNgramEntry(nega)) {
                // Ngram差分が固定Ngramエントリに含まれている場合は、差分登録を行わない
                _LOG_DETAIL(L"{} and {} are BOTH FIXED ngram entries", to_wstr(posi), to_wstr(nega));
                return;
            }
            auto key = nega + MSTR_VERT_BAR + posi;
            auto iter = selectedNgrams.find(key);
            _LOG_DETAIL(L"key={} entry {}FOUND", to_wstr(key), iter != selectedNgrams.end() ? L"" : L"NOT ");
            int bonusPoint = 0;
            if (iter != selectedNgrams.end()) {
                iter->second -= SETTINGS->ngramManualSelectDelta;
                bonusPoint = iter->second;
                updateSelectdNgramMap(key, posi, nega, -bonusPoint);
                bUpdated = true;
            } else {
                key = posi + MSTR_VERT_BAR + nega;
                iter = selectedNgrams.find(key);
                _LOG_DETAIL(L"key={} entry {}FOUND", to_wstr(key), iter != selectedNgrams.end() ? L"" : L"NOT ");
                if (iter != selectedNgrams.end()) {
                    iter->second += SETTINGS->ngramManualSelectDelta;
                    bonusPoint = iter->second;
                    updateSelectdNgramMap(key, posi, nega, bonusPoint);
                    bUpdated = true;
                } else {
                    // 新規追加
                    _LOG_DETAIL(L"add new entry: key={}", to_wstr(key));
                    bonusPoint = SETTINGS->ngramManualSelectDelta;
                    selectedNgrams[key] = bonusPoint;
                    updateSelectdNgramMap(key, posi, nega, bonusPoint);
                    bUpdated = true;
                }
            }
            LOG_INFOH(L"key={}, bonusPoint={}", to_wstr(key), bonusPoint);
        }

        //void updateSelectedNgram(const MString& posi, const MString& nega, bool geta) {
        //    if (geta) {
        //        _updateSelectedNgram(MSTR_GETA + posi, MSTR_GETA + nega);
        //    }
        //    _updateSelectedNgram(posi, nega);
        //}

    private:
        bool _gatherSelectedNgramPairBonus(std::set<SelectedNgramPairBonus>& resultSet, const MString& ngram) const {
            auto iter = selectedNgramMap.find(ngram);
            if (iter != selectedNgramMap.end()) {
                resultSet.insert(iter->second.begin(), iter->second.end());
                _LOG_DETAIL(L"FOUND: ngram={}, set<SelectedNgramPairBonus>: {}", to_wstr(ngram), debugStringOfSelectedNgramPairBonusSet(resultSet));
                return true;
            }
            return false;
        }

        String _joinSelectedNgramPairBonusSet(const std::set<SelectedNgramPairBonus>& s) const {
            String result;
            for (const auto& item : s) {
                if (!result.empty()) result.append(L", ");
                result.append(item.debugString());
            }
            return result;
        }

    public:
        // 指定された文字列に対して、そこに含まれるNgramに対応する選択Ngramペアボーナスを取得
        // Ngramは、1~M文字
        const std::set<SelectedNgramPairBonus> findNgramPairBonus(const MString& str) {
            //LOG_DEBUGH(L"ENTER: str={}", to_wstr(str));
            std::set<SelectedNgramPairBonus> resultSet;
            bool kanji = false;
            bool pKanji = false;
            bool ppKanji = false;
            for (size_t i = 0; i < str.size(); ++i) {
                ppKanji = pKanji;
                pKanji = kanji;
                kanji = utils::is_kanji(str[i]);
                if (i == 0) {
                    // 先頭文字の場合は、str と一致する〓付きも調べる (例: 〓池 vs 〓での)
                    // ただし、ひらがな1文字のケースを除く
                    if (str.size() > 1 || !utils::is_hiragana(str.front())) {
                        //for (size_t len = 1; len <= MAX_SELECTED_NGRAM_LEN && i + len <= str.size(); ++len) {
                        //    _gatherSelectedNgramPairBonus(resultSet, MSTR_GETA + str.substr(i, len));
                        //}
                        _LOG_DETAIL(L"Pattern: {}", to_wstr(MSTR_GETA + str));
                        bool found = _gatherSelectedNgramPairBonus(resultSet, MSTR_GETA + str);
                        if (!found && str.size() > 2) {
                            // 先頭全体の〓付きパターンが見つからなかった場合は、短い長さのパターンも調べる
                            for (size_t len = 3; len <= MAX_SELECTED_NGRAM_LEN && len < str.size(); ++len) {
                                _LOG_DETAIL(L"Pattern: {}", to_wstr(MSTR_GETA + str.substr(0, len)));
                                _gatherSelectedNgramPairBonus(resultSet, MSTR_GETA + str.substr(0, len));
                            }
                        }
                    }
                } else if (i + 1 < str.size()) {
                    // 先頭以外で、後続する文字がある場合は、先頭からの〓付きも調べる
                    if (i == 1 && (pKanji || kanji)) {
                        // 先頭から2文字目にかけて、漢字が含まれるパターンの場合
                        _LOG_DETAIL(L"Pattern: {}", to_wstr(MSTR_GETA + str.substr(0, 2)));
                        _gatherSelectedNgramPairBonus(resultSet, MSTR_GETA + str.substr(0, 2));
                    } else if (i > 1) {
                        // 先頭から3文字目以降の場合は、ひらがなのみのパターンも含めて調べる
                        _LOG_DETAIL(L"Pattern: {}", to_wstr(MSTR_GETA + str.substr(0, i + 1)));
                        _gatherSelectedNgramPairBonus(resultSet, MSTR_GETA + str.substr(0, i + 1));
                    }
                }
                if (i >= 2 && !ppKanji && pKanji && !kanji) {
                    // 「非漢字-漢字-非漢字」のパターンの場合は、漢字1文字についても調べる
                    _LOG_DETAIL(L"Pattern: non-kanji [{}] - kanji [{}] - non-kanji [{}]", to_wstr(str[i - 2]), to_wstr(str[i - 1]), to_wstr(str[i]));
                    _gatherSelectedNgramPairBonus(resultSet, str.substr(i - 1, 1));
                }
                for (size_t len = 2; len <= MAX_SELECTED_NGRAM_LEN && i + len <= str.size(); ++len) {
                    // 2文字以上の差分についてはGETA(〓)なしパターンも調べる
                    _gatherSelectedNgramPairBonus(resultSet, str.substr(i, len));
                }
            }
#if _LOG_DEBUG_ENABLED
            LOG_DEBUGH(L"RESULTS: str={}, resultSet={}", to_wstr(str), resultSet.empty() ? L"EMPTY" : _joinSelectedNgramPairBonusSet(resultSet));
#endif
            return resultSet;
        }
    }; // class SelectedNgram

    SelectedNgram selectedNgramInstance;

    // 指定された文字列に対して、そこに含まれるNgramに対応する選択Ngramペアボーナスを取得 (Ngramは、2~M文字)
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

    // 2つの文字列の最初の差分部分を見つける(拡張版)
    // * findFirstDiff と同様に、先頭共通部の直後から、次に同じ文字で再同期できる最初の位置までを差分として返す
    // * ただし、再同期した部分が1文字だけで、次の文字からまた差分がある場合は、その再同期した1文字も含めて差分とみなす
    //   (例: 「おのれの」と「その学」は、2文字目の「の」を含んで差分とみなす)
    // 戻り値: (startPos, endPos1, endPos2) (「先頭共通部の直後から、次に同じ文字で再同期できる最初の位置まで」を差分として返す)
    std::tuple<size_t, size_t, size_t> findFirstDiffEx(const MString& s1, const MString& s2) {
        size_t len1 = s1.size();
        size_t len2 = s2.size();
        size_t startPos = 0;
        // 最初に差分がある位置まで進む
        while (startPos < len1 && startPos < len2 && s1[startPos] == s2[startPos]) {
            ++startPos;
        }
        _LOG_DETAIL(L"first diff: startPos={}, len1={}, len2={}", startPos, len1, len2);

        bool firstResync = true;    // 最初の再同期か
        auto checkResync = [&](size_t endPos1, size_t endPos2) -> int {
            //_LOG_DETAIL(L"checkResync: s1[{}]={:c}, s2[{}]={:c}", endPos1, endPos1 < len1 ? s1[endPos1] : L'_', endPos2, endPos2 < len2 ? s2[endPos2] : L'_');
            if (endPos1 < len1 && endPos2 < len2 && s1[endPos1] == s2[endPos2]) {
                if (firstResync && endPos1 + 1 < len1 && endPos2 + 1 < len2 && s1[endPos1 + 1] != s2[endPos2 + 1]) {
                    // 再同期した位置の次の文字が異なっている
                    firstResync = false;
                    return 1;
                }
                // 再同期した
                return 0;
            }
            // 再同期できなかった
            return -1;
        };


        // startPos <= i < pos の範囲に漢字があるか
        auto checkKanji = [&](const MString& s, size_t pos) -> bool {
            for (size_t i = startPos; i < pos && i < s.size(); ++i) {
                if (utils::is_kanji(s[i])) {
                    return true;
                }
            }
            return false;
        };

        if (startPos < len1 && startPos < len2) {
            size_t endPos = startPos + 1;
            while (endPos < len1 || endPos < len2) {
                int res = checkResync(endPos, endPos);
                _LOG_DETAIL(L"checkResync: pos1={}, pos2={}, res={}", endPos, endPos, res == 0 ? L"SYNC" : L"NG");
                if (res == 0) {
                    // 再同期した
                    _LOG_DETAIL(L"RETURN: ( startPos={}, endPos={}, endPos={} )", startPos, endPos, endPos);
                    return { startPos, endPos, endPos };
                } else {
                    // 再同期できなかったか、再同期した位置の次の文字が異なっている
                    size_t pos = endPos - 1;
                    while (pos > startPos) {
                        if (checkKanji(s1, pos) || !checkKanji(s2, endPos)) {
                            // pos < endPos なので、s1のpos位置までに漢字がなく、かつs2のendPos位置までに漢字がある場合はスキップ
                            res = checkResync(pos, endPos);
                            _LOG_DETAIL(L"checkResync: pos1={}, pos2={}, res={}", pos, endPos, res == 0 ? L"SYNC" : L"NG");
                            if (res == 0) {
                                // 再同期した
                                _LOG_DETAIL(L"RETURN: ( startPos={}, pos={}, endPos={} )", startPos, pos, endPos);
                                return { startPos, pos, endPos };
                            }
                        }
                        if (checkKanji(s2, pos) || !checkKanji(s1, endPos)) {
                            // pos < endPos なので、s2のpos位置までに漢字がなく、かつs1のendPos位置までに漢字がある場合はスキップ
                            res = checkResync(endPos, pos);
                            _LOG_DETAIL(L"checkResync: pos1={}, pos2={}, res={}", endPos, pos, res == 0 ? L"SYNC" : L"NG");
                            if (res == 0) {
                                // 再同期した
                                _LOG_DETAIL(L"RETURN: ( startPos={}, endPos={}, pos={} )", startPos, endPos, pos);
                                return { startPos, endPos, pos };
                            }
                        }
                        --pos;
                    }
                }
                ++endPos;
            }
        }
        _LOG_DETAIL(L"RETURN: ( startPos={}, len1={}, len2={} )", startPos, len1, len2);
        return { startPos, len1, len2 };
    }

    // 候補選択による、SelectedNgramの更新
    // 対となるのは
    //   - GETA(〓)付きの先頭Ngram
    //   - 「非漢字-漢字-非漢字」のパターンの漢字1文字
    //   - 漢字のみの2gram以上
    //   - それ以外の3gram以上
    //   - 短い差分で、どちらかにひらがなが含まれる場合は、前後の文字も含めて更新
    void updateSelectedNgramByUserSelect(const MString& oldCand, const MString& newCand) {
        LOG_INFOH(L"ENTER: oldCand={}, newCand={}", to_wstr(oldCand), to_wstr(newCand));
        size_t baseSize = oldCand.size();
        size_t diffSize = newCand.size();
        auto [startPos, endPos1, endPos2] = findFirstDiffEx(oldCand, newCand);
        LOG_INFOH(L"{}|{}, startPos={}, endPos1={}, endPos2={}, baseSize={}, diffSize={}",
            to_wstr(utils::safe_substr(oldCand, startPos, endPos1-startPos)),
            to_wstr(utils::safe_substr(newCand, startPos, endPos2-startPos)),
            startPos, endPos1, endPos2, baseSize, diffSize);
        if (startPos < endPos1 && startPos < endPos2) {
            size_t len1 = endPos1 - startPos;
            size_t len2 = endPos2 - startPos;

            // 共に1文字の差分、またはどちらかが2文字以下でひらがなを含む差分の場合は、短い差分として扱う
            auto checkShortDiff = [&]() -> bool {
                if (len1 == 1 || len2 == 1) {
                    return true;
                }
                if (len1 == 2 && (utils::is_hiragana(oldCand[startPos]) || utils::is_hiragana(oldCand[startPos + 1]))) {
                    // oldCandの2文字のうちどちらかがひらがななら、短い差分としてみなす
                    return true;
                }
                if (len2 == 2 && (utils::is_hiragana(newCand[startPos]) || utils::is_hiragana(newCand[startPos + 1]))) {
                    // newCandの2文字のうちどちらかがひらがななら、短い差分としてみなす
                    return true;
                }
                return false;
            };

            auto addShortPairWithGeta = [&](size_t xlen1, size_t xlen2) -> void {
                // 先頭だったら、〓を前に追加して処理する
                //if (xlen1 <= 2 && xlen2 <= 2) {
                //    if (xlen1 + 1 <= baseSize && xlen2 + 1 <= diffSize) {
                //        // 両者ともに2文字以下の差分の場合は、可能なら後ろ1文字も含めて処理する
                //        LOG_INFOH(L"append postfix char");
                //        ++xlen1;
                //        ++xlen2;
                //    }
                //}
                selectedNgramInstance.updateSelectedNgram(
                    MSTR_GETA + newCand.substr(0, xlen2),
                    MSTR_GETA + oldCand.substr(0, xlen1));
            };

            auto addPair = [&](size_t xstartPos, size_t xlen1, size_t xlen2) -> void {
                selectedNgramInstance.updateSelectedNgram(
                    newCand.substr(xstartPos, xlen2),
                    oldCand.substr(xstartPos, xlen1));
            };

            if (checkShortDiff()) {
                // 短い差分の場合は、前後の文字を含めて更新する
                LOG_INFOH(L"short diff");
                if (startPos == 0) {
                    // 先頭だったら、〓を前に追加して処理する
                    if ((len1 <= 2 || len2 <= 2) && len1 + 1 <= baseSize && len2 + 1 <= diffSize) {
                        // どちらかが2文字以下で後に文字が続くなら、後の1文字も含めて処理する
                        LOG_INFOH(L"append GETA and postfix char: startPos={}, len1={}, len2={}", startPos, len1 + 1, len2 + 1);
                        addShortPairWithGeta(len1 + 1, len2 + 1);
                        if ((len1 == 1 || len2 == 1) && len1 + 2 <= baseSize && len2 + 2 <= diffSize) {
                            // どちらかが1文字で後に2文字が続くなら、後の2文字も含めて処理する
                            LOG_INFOH(L"append GETA and postfix char: startPos={}, len1={}, len2={}", startPos, len1 + 2, len2 + 2);
                            addShortPairWithGeta(len1 + 2, len2 + 2);
                        }
                    } else {
                        LOG_INFOH(L"append GETA: startPos={}, len1={}, len2={}", startPos, len1, len2);
                        addShortPairWithGeta(len1, len2);
                    }
                } else {
                    // 先頭以外の短い差分の場合
                    if (len1 == 1 && len2 == 1 && baseSize > startPos + 1 && diffSize > startPos + 1 &&
                        utils::is_kanji(oldCand[startPos]) && utils::is_kanji(newCand[startPos]) &&
                        !utils::is_kanji(oldCand[startPos - 1]) && !utils::is_kanji(oldCand[startPos + 1])) {
                        // 文字列中の漢字1文字だけが異なっている場合(比較の時も漢字1文字だけが異なっている場合にその1文字だけを差分として処理する)
                        LOG_INFOH(L"pair with only 1 kanji");
                        addPair(startPos, 1, 1);
                        //selectedNgramInstance.updateSelectedNgram(
                        //    newCand.substr(startPos, 1),
                        //    oldCand.substr(startPos, 1));
                    } else {
                        if (startPos == 1) {
                            // 1文字目だったら、〓と先頭文字を前に追加して処理する
                            if (startPos + len1 + 1 <= baseSize && startPos + len2 + 1 <= diffSize) {
                                LOG_INFOH(L"append GETA and prefix and postfix chars: startPos=0, len1={}, len2={}", len1 + 2, len2 + 2);
                                addShortPairWithGeta(len1 + 2, len2 + 2);
                            } else {
                                LOG_INFOH(L"append GETA and prefix char: startPos=0, len1={}, len2={}", len1 + 1, len2 + 1);
                                addShortPairWithGeta(len1 + 1, len2 + 1);
                            }
                        } else {
                            // 2文字目以降の場合
                            // 前後の文字も含めて処理する
                            if (startPos + len1 + 1 <= baseSize && startPos + len2 + 1 <= diffSize) {
                                // 後続文字がある
                                if (len1 == 1 || len2 == 1) {
                                    // どちらも1文字の差分の場合は、前後の1文字を含めて3文字で処理する
                                    LOG_INFOH(L"append prefix and postfix char: startPos={}, len1={}, len2={}", startPos - 1, len1 + 2, len2 + 2);
                                    addPair(startPos - 1, len1 + 2, len2 + 2);
                                } else {
                                    // どちらかが2文字以上の差分の場合は、後続の1文字も含めて処理する
                                    LOG_INFOH(L"append postfix char: startPos={}, len1={}, len2={}", startPos, len1 + 1, len2 + 1);
                                    addPair(startPos, len1 + 1, len2 + 1);
                                }
                            } else {
                                // 後続文字がない場合
                                if (len1 == 1 || len2 == 1) {
                                    // どちらかが1文字の差分の場合は、前の2文字を含めて3文字以上で処理する
                                    LOG_INFOH(L"append prefix 2 chars: startPos={}, len1={}, len2={}", startPos - 2, len1 + 2, len2 + 2);
                                    addPair(startPos - 2, len1 + 2, len2 + 2);
                                } else {
                                    // 両者とも2文字以上の差分の場合は、前の1文字を含めて3文字以上で処理する
                                    LOG_INFOH(L"append prefix char: startPos={}, len1={}, len2={}", startPos - 1, len1 + 1, len2 + 1);
                                    addPair(startPos - 1, len1 + 1, len2 + 1);
                                }
                            }
                        }
                    }
                }
            } else if (len1 <= MAX_SELECTED_NGRAM_LEN && len2 <= MAX_SELECTED_NGRAM_LEN) {
                // 3~M文字の差分または両者とも漢字のみの差分の場合は、そのまま更新する
                LOG_INFOH(L"long diff");
                selectedNgramInstance.updateSelectedNgram(
                    newCand.substr(startPos, len2),
                    oldCand.substr(startPos, len1));
            }
        }
        LOG_INFOH(L"LEAVE");
    }

#define REALTIME_NGRAM_FILE (SETTINGS->useTmpRealtimeNgramFile ? REALTIME_NGRAM_TEMP_FILE : REALTIME_NGRAM_MAIN_FILE)
#define SELECTED_NGRAM_FILE (SETTINGS->useTmpRealtimeNgramFile ? SELECTED_NGRAM_TEMP_FILE : SELECTED_NGRAM_MAIN_FILE)
#define USER_NGRAM_FILE (SETTINGS->useTmpRealtimeNgramFile ? USER_NGRAM_TEMP_FILE : USER_NGRAM_MAIN_FILE)

    // 各種Ngramファイルの読み込み
    void loadNgramFiles() {
        LOG_INFO(L"ENTER");

        // リアルタイムNgramファイルの読み込み
        auto realtimeNgramPath = utils::joinPath(SETTINGS->rootDir, REALTIME_NGRAM_FILE);
        int nEntries = NgramBridge::loadRealtimeDict(realtimeNgramPath);
        LOG_INFO(L"loadRealtimeDict(): nEntries={}", realtimeNgramPath, nEntries);

        // ユーザー定義Ngramファイルの読み込み
        auto userNgramPath = utils::joinPath(SETTINGS->rootDir, USER_NGRAM_FILE);
        nEntries = NgramBridge::loadUserNgram(userNgramPath);
        LOG_INFO(L"loadUserNgram(): nEntries={}", userNgramPath, nEntries);
        //fixedNgramBonusCounts.clear();
        //userNgramBonusCounts.clear();
        //_loadNgramFile(USER_NGRAM_FILE, userNgramBonusCounts);
        _loadFixedNgramFile(userNgramPath);

        // ユーザー選択Ngramファイルの読み込み
        selectedNgramInstance.loadSelectedNgramFile(SELECTED_NGRAM_FILE);

        maxCandidatesSize = 0;

        LOG_INFO(L"LEAVE");
    }

    // リアルタイムNgramファイルの保存
    void saveRealtimeNgramFile() {
        LOG_SAVE_DICT(L"ENTER: file={}", REALTIME_NGRAM_FILE);
#ifndef _DEBUG
        auto realtimeNgramPath = utils::joinPath(SETTINGS->rootDir, REALTIME_NGRAM_FILE);
        int backupGeneration = SETTINGS->useTmpRealtimeNgramFile ? 0 : SETTINGS->backFileRotationGeneration;
        NgramBridge::saveRealtimeDict(realtimeNgramPath, backupGeneration);
        selectedNgramInstance.saveSelectedNgramFile(SELECTED_NGRAM_FILE, backupGeneration);
#endif
        LOG_SAVE_DICT(L"LEAVE: file={}", REALTIME_NGRAM_FILE);
    }

    //inline bool is_space_or_vbar(mchar_t ch) {
    //    return ch == ' ' || ch == '|';
    //}

    void _updateRealtimeNgramCountByWord(bool bIncrease, const MString& word) {
        int delta = bIncrease ? 1 : -1;
        int count = NgramBridge::updateRealtimeEntry(to_wstr(word), delta);
        LOG_DEBUGH(L"updateRealtimeEntry({}): resultCount={}", to_wstr(word), count);
    }

    // リアルタイムNgramの更新
    void _updateRealtimeNgram(bool bIncrease, const MString& str) {
        LOG_DEBUGH(L"ENTER: bIncrease={}, str={}", bIncrease, to_wstr(str));
        int strlen = (int)str.size();
        int hirakanLen = 0;
        int hiraLen = 0;
        int kanjiLen = 0;
        bool kanji = false;
        bool pKanji = false;
        bool ppKanji = false;
        for (int pos = 0; pos < strlen; ++pos) {
            ppKanji = pKanji;
            pKanji = kanji;
            if (utils::is_hiragana(str[pos])) {
                kanji = false;
                kanjiLen = 0;
                ++hiraLen;
                ++hirakanLen;
            } else if (utils::is_kanji(str[pos])) {
                kanji = true;
                ++kanjiLen;
                ++hirakanLen;
                hiraLen = 0;
            } else {
                hirakanLen = 0;
                hiraLen = 0;
                kanjiLen = 0;
                kanji = false;
            }
            // 漢字1文字で、前後が非漢字の場合は、その漢字のみのNgramも更新する (例: 「池」の前後がひらがななら、「池」も更新する)
            if (pos >= 3 && !ppKanji && pKanji && !kanji) {
                _updateRealtimeNgramCountByWord(bIncrease, str.substr(pos - 1, 1));
            }
            //if (kanjiLen >= 2 && pos >= 1) {
            //    // 漢字が2文字以上連続している場合は、2gramを更新する
            //    _updateRealtimeNgramCountByWord(bIncrease, str.substr(pos - 1, 2));
            //}
            if (hirakanLen >= 2 && pos >= 1) {
                // ひらがなor漢字が2文字以上連続している場合は、2gramを更新する
                _updateRealtimeNgramCountByWord(bIncrease, str.substr(pos - 1, 2));
            }
            if (hirakanLen >= 3 && pos >= 2) {
                // ひらがなor漢字が3文字以上連続している場合は、3gramを更新する
                _updateRealtimeNgramCountByWord(bIncrease, str.substr(pos - 2, 3));
            }
            if (hiraLen >= 4 && pos >= 3) {
                // ひらがなが4文字以上連続している場合は、4gramを更新する
                _updateRealtimeNgramCountByWord(bIncrease, str.substr(pos - 3, 4));
            }
        }
        LOG_DEBUGH(L"LEAVE: str={}", to_wstr(str));
    }

    // 編集バッファのフラッシュによるリアルタイムNgramの更新
    void updateRealtimeNgram(const MString& str) {
        _updateRealtimeNgram(true, str);
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

    MString kanjiNumChars = to_mstr(L"一二三四五六七八九十〇");
    std::wregex kanjiDateTime(L"[一二三四五六七八九十〇](年[一二三四五六七八九十〇]+月([一二三四五六七八九十〇]+日)?|月[一二三四五六七八九十〇]+日|[一二三四五六七八九十〇]日)");
    
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

//#define SUCCESSIVE_HIRAGANA_LEN 8
//#define LONG_HIRAGANA_LEN 4
//#define SUCCESSIVE_HIRAGANA_COST 5000
//
//    // 8文字以上連続するひらがな列は、その中に 4gram 以上の長さのひらがな列が存在しなければ、コスト増(SUCCESSIVE_HIRAGANA_COST)となる
//    int calcSuccessiveHiraganaCost(const std::vector<MString>& morphs) {
//        size_t hiraganaSeqLen = 0;
//        bool hasLongHiragana = false;
//        int cost = 0;
//        for (const auto& morph : morphs) {
//            size_t tabPos = morph.find(L'\t');
//            if (tabPos >= morph.size()) tabPos = morph.size();
//            MString word = morph.substr(0, tabPos);
//            if (std::all_of(word.begin(), word.end(), [](mchar_t ch) { return utils::is_hiragana(ch) || (wchar_t)ch == L'〓'; })) {
//                size_t wordLen = word.size();
//                hiraganaSeqLen += wordLen;
//                if (wordLen >= LONG_HIRAGANA_LEN) {
//                    hasLongHiragana = true;
//                }
//                _LOG_DETAIL(L"HIRAGANA word={}, wordLen={}, hiraganaSeqLen={}, hasLongHiragana={}", to_wstr(word), wordLen, hiraganaSeqLen, hasLongHiragana);
//            } else {
//                _LOG_DETAIL(L"NON-HIRAGANA word={}", to_wstr(word));
//                if (hiraganaSeqLen >= SUCCESSIVE_HIRAGANA_LEN && !hasLongHiragana) {
//                    cost += SUCCESSIVE_HIRAGANA_COST;
//                    _LOG_DETAIL(L"Successive hiragana cost applied: cost={}", SUCCESSIVE_HIRAGANA_COST);
//                }
//                hiraganaSeqLen = 0;
//                hasLongHiragana = false;
//            }
//        }
//        if (hiraganaSeqLen >= SUCCESSIVE_HIRAGANA_LEN && !hasLongHiragana) {
//            cost += SUCCESSIVE_HIRAGANA_COST;
//            _LOG_DETAIL(L"Successive hiragana cost applied: cost={}", SUCCESSIVE_HIRAGANA_COST);
//        }
//        return cost;
//    }

    // Ngramコストの取得
    int getNgramCost(const MString& str, const std::vector<MString>& morphs, bool bUseGeta) {
        _LOG_DETAIL(L"\nENTER: str={}: geta={}, morphs={}", to_wstr(str), bUseGeta, to_wstr(utils::join(morphs, L'|')));
        std::vector<MString> ngrams;
        MString targetStr = bUseGeta ? MSTR_GETA + str : str;
        // Ngram 解析 (NgramAnalyzer)
        int cost0 = NgramBridge::ngramCalcCost(targetStr, morphs, ngrams, SETTINGS->multiStreamDetailLog);
        int cost = cost0;
        _LOG_DETAIL(L"ngrams: initial cost={}\n--------\n{}\n--------", cost, to_wstr(utils::join(ngrams, '\n')));
        if (utils::reMatch(to_wstr(targetStr), kanjiDateTime)) {
            cost -= calcNgramBonus(DATE_PATTERN_BONUMS_POINT);
        }
        //cost += calcSuccessiveHiraganaCost(morphs);
        _LOG_DETAIL(L"LEAVE: str={}: initialCost={}, resultCost={}, adjustedCost={} (* NGRAM_COST_FACTOR({}))\n",
            to_wstr(str), cost0, cost, cost * SETTINGS->ngramCostFactor, SETTINGS->ngramCostFactor);
        return cost;
    }

} // namespace lattice2
