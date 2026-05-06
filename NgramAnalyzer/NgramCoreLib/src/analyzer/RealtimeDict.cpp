#include "std_utils.h"
#include "file_utils.h"
#include "string_utils.h"
#include "Logger.h"
#include "RealtimeDict.h"

#include "NgramDebugLog.h"

namespace analyzer {
    namespace RealtimeDict {
        DEFINE_LOCAL_LOGGER(RealtimeDict);

        // リアルタイムN-gramの最大ボーナス
        static const int kMaxNgramBonus = 5000;

        // リアルタイムN-gramの辞書 (単語 -> カウント)
        static std::map<String, int> realtimeDict;

        // ユーザー定義のN-gramの辞書 (単語 -> カウント)。ユーザー定義のN-gramカウントは、リアルタイムN-gramカウントに加算される
        static std::map<String, int> userDict;

        size_t minLen = 1;  // 最小N-gram長
        size_t maxLen = 4;  // 最大N-gram長

        int ngramInflexBonusPoint = 25;         // Ngramに与えるボーナスポイントの変曲点
        int ngramBonusPointFactor = 100;        // ボーナスポイントに対するボーナス係数

        int maxRealtimeCount = 0;  // リアルタイムN-gramの最大カウント (ユーザー定義のN-gramのカウントは含まない)

        bool realtimeNgram_updated = false;

        void setParameters(size_t minNgramLen, size_t maxNgramLen, int maxBonusPoint, int bonusPointFactor) {
            LOG_INFOH(L"CALLED: minNgramLen={}, maxNgramLen={}, maxBonusPoint={}, bonusPointFactor={}", minNgramLen, maxNgramLen, maxBonusPoint, bonusPointFactor);
            minLen = minNgramLen;
            maxLen = maxNgramLen;
            ngramInflexBonusPoint = maxBonusPoint;
            ngramBonusPointFactor = bonusPointFactor;
        }

        int updateEntry(const String& word, int delta) {
            LOG_DEBUGH(L"ENTER: word={}, delta={}", word, delta);
            int count = realtimeDict[word] += delta;
            if (count > kMaxNgramBonus) {
                maxRealtimeCount = count;
            }
            realtimeNgram_updated = true;
            LOG_DEBUGH(L"LEAVE: word={}, count={}", word, count);
            return count;
        }

        inline bool isDecimalString(StringRef item) {
            return utils::reMatch(item, L"[+\\-]?[0-9]+");
        }

        // リアルタイムNgramファイルのロード
        int loadRealtimeNgramFile(StringRef ngramFilePath) {
            LOG_INFOH(_T("LOAD: {}"), ngramFilePath);
            int nEntries = 0;
            realtimeDict.clear();
            maxRealtimeCount = 0;
            utils::IfstreamReader reader(ngramFilePath);
            if (reader.success()) {
                for (const auto& line : reader.getAllLines()) {
                    auto items = utils::split(utils::replace_all(utils::strip(line), L" +", L"\t"), '\t');
                    if (items.size() == 2 && !items[0].empty() && items[0][0] != L'#' && !items[1].empty()) {
                        String sCount = items[1];
                        if (isDecimalString(sCount)) {
                            int count = std::stoi(sCount);
                            realtimeDict[items[0]] = count;
                            if (count > maxRealtimeCount) {
                                maxRealtimeCount = count;
                            }
                            ++nEntries;
                        }
                    }
                }
            }
            LOG_INFOH(_T("DONE: nEntries={}"), nEntries);
            return nEntries;
        }

        // ユーザー定義のNgramファイルのロード
        int loaUserdNgramFile(StringRef ngramFilePath) {
            LOG_INFOH(_T("LOAD: {}"), ngramFilePath);
            int nEntries = 0;
            userDict.clear();
            utils::IfstreamReader reader(ngramFilePath);
            if (reader.success()) {
                for (const auto& line : reader.getAllLines()) {
                    auto items = utils::split(utils::replace_all(utils::strip(line), L" +", L"\t"), '\t');
                    if (items.size() >= 1 && !items[0].empty() && items[0][0] != L'#') {
                        int count = ngramInflexBonusPoint;  // ユーザー定義のN-gramは、デフォルトで最大ボーナスポイントを与える
                        if (items.size() > 1 && !items[1].empty()) {
                            String sCount = items[1];
                            if (isDecimalString(sCount)) {
                                count = std::stoi(sCount);
                            }
                        }
                        StringRef word = items[0];
                        userDict[word] = count;
                        userDict[L"〓" + word] = count;
                        if (word.size() >= 6) {
                            // 6gram以上なら、5gramに分割して登録する
                            size_t len = 5;
                            for (size_t pos = 0; pos + len <= word.size(); ++pos) {
                                LOG_DEBUGH(_T("register SUB 5gram: sub 5gram={}, count={}"), word.substr(pos, len), count);
                                userDict[word.substr(pos, len)] = count;
                            }
                        }
                        ++nEntries;
                    }
                }
            }
            LOG_INFOH(_T("DONE: nEntries={}"), nEntries);
            return nEntries;
        }

        // リアルタイムNgramファイルの保存
        void saveNgramFile(StringRef ngramFilePath, int rotationNum) {
            LOG_INFOH(L"ENTER: file={}, realtimeNgram_updated={}", ngramFilePath, realtimeNgram_updated);
#ifndef _DEBUG
            if (realtimeNgram_updated) {
                // 一旦、一時ファイルに書き込み
                auto pathTmp = ngramFilePath + L".tmp";
                {
                    LOG_INFOH(_T("SAVE: realtime ngram file pathTmp={}"), pathTmp.c_str());
                    utils::OfstreamWriter writer(pathTmp);
                    if (writer.success()) {
                        for (const auto& pair : realtimeDict) {
                            String line;
                            //int count = pair.second;
                            //if (count < 0 || count > 1 || (count == 1 && NgramCoreLib::Logger::IsWarnEnabled())) {
                                // count が 0 または 1 の N-gramは無視する
                            line.append(pair.first);           // 単語
                            line.append(_T("\t"));
                            line.append(std::to_wstring(pair.second));  // カウント
                            writer.writeLine(utils::utf8_encode(line));
                            //}
                        }
                        realtimeNgram_updated = false;
                    }
                    LOG_INFOH(_T("DONE: entries count={}"), realtimeDict.size());
                }
                // pathTmp ファイルのサイズが path ファイルのサイズよりも小さい場合は、書き込みに失敗した可能性があるので、既存ファイルを残す
                utils::compareAndMoveFileToBackDirWithRotation(pathTmp, ngramFilePath, rotationNum);
            }
#endif
            LOG_INFOH(L"LEAVE: file={}", ngramFilePath);
        }

#include <cmath>

        // リアルタイムN-gramのカウントからボーナスポイントを計算する。
        // カウントが大きくなるほど、ボーナスポイントの増加は緩やかになるようにする。
        // ボーナスポイントの最大値は kMaxNgramBonus で、カウントが maxRealtimeCount 以上の場合は常に kMaxNgramBonus を与える。
        int calcRealtimeBonus(int count) {
            if (count <= 0 || maxRealtimeCount <= 0) {
                return 0;
            }

            if (count >= maxRealtimeCount) {
                return kMaxNgramBonus;
            }

            double normalized = std::log1p(static_cast<double>(count)) / std::log1p(static_cast<double>(maxRealtimeCount));
            int bonus = static_cast<int>(std::round(normalized * kMaxNgramBonus));
            if (bonus < 0) {
                return 0;
            }
            if (bonus > kMaxNgramBonus) {
                return kMaxNgramBonus;
            }
            return bonus;
        }

        // ユーザー定義のN-gramのカウントからボーナスポイントを計算する。カウントが大きくなるほど、ボーナスポイントの増加は緩やかになるようにする。
        int calcUserBonus(int bonusPoint) {
            double point = bonusPoint;
            if (point > ngramInflexBonusPoint) {
                if (point <= ngramInflexBonusPoint * 2) {
                    point = ngramInflexBonusPoint + (point - ngramInflexBonusPoint) * 0.4;
                } else if (point <= ngramInflexBonusPoint * 4) {
                    point = ngramInflexBonusPoint * 1.4 + (point - ngramInflexBonusPoint * 2) * 0.2;
                } else if (point <= ngramInflexBonusPoint * 40) {
                    point = ngramInflexBonusPoint * 1.8 + (point - ngramInflexBonusPoint * 4) * 0.1;
                } else if (point <= ngramInflexBonusPoint * 400) {
                    point = ngramInflexBonusPoint * 2.16 + (point - ngramInflexBonusPoint * 40) * 0.02;
                } else {
                    point = ngramInflexBonusPoint * 9.36 + (point - ngramInflexBonusPoint * 400) * 0.01;
                }
            }
            return (int)(point * ngramBonusPointFactor);
        }

        inline bool isKanjiOrGeta(wchar_t ch) {
            return utils::is_kanji(ch) || ch == L'〓';
        }

        // 与えられた文字列の先頭部分にマッチするエントリ（複数可）を検索する
        // @param str 検索対象の文字列
        // @param pos 検索開始位置
        // @return マッチしたエントリのボーナスポイントのリスト。(各エントリの長さのをインデックスとする)
        std::vector<int> commonPrefixSearch(const String& str, size_t pos, bool hiraganaBigramEnabled, bool hiraganaQuadgramEnabled) {
            LOG_DEBUGH(L"ENTER: str={}, pos={}, bigram={}, quadgram={}, maxRtCount={}, minLen={}, maxLen={}", str, pos, hiraganaBigramEnabled, hiraganaQuadgramEnabled, maxRealtimeCount, minLen, maxLen);
            std::vector<int> result;    // マッチしたエントリのボーナスポイントのリスト。(各エントリの長さのをインデックスとする)
            result.resize(maxLen + 1, 0);  // エントリの長さでインデックスされる。値 0 はエントリがないことを表す。
            if (minLen > 0 && !str.empty()) {
                size_t end = std::min(pos + maxLen, str.size());
                bool firstGeta = pos == 0 && str[0] == L'〓';
                bool isKanjiNextToGeta = firstGeta && str.size() > 1 && utils::is_pure_kanji(str[1]);
                size_t nextPos = isKanjiNextToGeta ? pos + 1 : pos;     // 先頭がゲタ+漢字なら、ゲタを飛ばして漢字から始まるNgramも検索対象とする
                for (size_t i = pos + minLen; i <= end; ++i) {
                    bool bSkipRealTimeSearch = false;
                    size_t len = i - pos;
                    if (len == 1 && utils::is_kanji(str[pos])) {
                        if (pos == 0 || pos + 1 >= str.size() || isKanjiOrGeta(str[pos - 1]) || isKanjiOrGeta(str[pos + 1])) {
                            // 1文字の漢字で、前後も漢字かゲタ文字の場合は、realtime N-gramエントリとして扱わない
                            bSkipRealTimeSearch = true;
                            LOG_DEBUG(L"Skip Realtime Kanji unigram");
                        }
                    }
                    // 先頭が漢字なら、ゲタで始まらないNgramも検索対象とする
                    for (size_t j = pos; j <= nextPos; ++j) {
                        String key = str.substr(j, len);
                        int bonus = 0;
                        if (!bSkipRealTimeSearch) {
                            // リアルタイムN-gram辞書とユーザー定義N-gram辞書の両方を検索して、カウントを合算する
                            auto it = realtimeDict.find(key);
                            if (it != realtimeDict.end()) {
                                // リアルタイムN-gram辞書では、エントリの長さが 3 であれば常にカウントを使用する。
                                // エントリの長さが 2 であれば、hiraganaBigramEnabled が有効な場合、または両方の文字が漢字の場合にカウントを使用する。
                                // エントリの長さが 4 であれば、hiraganaQuadgramEnabled が有効な場合にカウントを使用する。
                                if (len == 3 ||
                                    (len == 2 && (hiraganaBigramEnabled || (utils::is_kanji(key[0]) && utils::is_kanji(key[1])))) ||
                                    (len == 4 && hiraganaQuadgramEnabled)) {
                                    bonus = calcRealtimeBonus(it->second);
                                    LOG_DEBUG(L"realtime FOUND: key={}, count={}, bonus={}", key, it->second, bonus);
                                }
                            }
                        }
                        {
                            // ユーザー定義N-gram辞書を検索して、カウントを合算する
                            auto it = userDict.find(key);
                            if (it != userDict.end()) {
                                int userBonus = calcUserBonus(it->second);
                                LOG_DEBUG(L"userDic FOUND: key={}, count={}, bonus={}", key, it->second, userBonus);
                                bonus += userBonus;
                            }
                        }
                        result[len] = bonus;
                    }
                }
            }
            LOG_DEBUGH(L"LEAVE: result.size()={}", result.size());
            return result;
        }

        /**
         * 与えられた文字列に完全一致するエントリがあるか
         */
        bool findExactMatch(const String& key) {
            auto iter1 = userDict.find(key);
            if (iter1 != userDict.end()) {
                LOG_DEBUGH(L"FOUND: {} in userDict", key);
                return true;
            }

            auto iter2 = realtimeDict.find(key);
            if (iter2 != realtimeDict.end()) {
                LOG_DEBUGH(L"FOUND: {} in realtimeDict", key);
                return true;
            }
            return false;
        }

    } // namespace RealtimeDict
} // namespace analyze
