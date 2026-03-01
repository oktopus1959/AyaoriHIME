#include "std_utils.h"
#include "file_utils.h"
#include "string_utils.h"
#include "Logger.h"
#include "RealtimeDict.h"

#if 1
#undef LOG_INFO
#undef LOG_DEBUGH
#undef LOG_DEBUG
#define LOG_INFO LOG_INFOH
#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFOH
#endif

namespace analyzer {
    namespace RealtimeDict {
        DEFINE_LOCAL_LOGGER(RealtimeDict);

        // リアルタイムN-gramの辞書 (単語 -> カウント)
        static std::map<String, int> realtimeDict;

        // ユーザー定義のN-gramの辞書 (単語 -> カウント)。ユーザー定義のN-gramカウントは、リアルタイムN-gramカウントに加算される
        static std::map<String, int> userDict;

        size_t minLen = 1;  // 最小N-gram長
        size_t maxLen = 4;  // 最大N-gram長

        int ngramMaxBonusPoint = 25;            // Ngramに与えるボーナスポイントの最大値
        int ngramBonusPointFactor = 100;        // ボーナスポイントに対するボーナス係数

        bool realtimeNgram_updated = false;

        void setParameters(size_t minNgramLen, size_t maxNgramLen, int maxBonusPoint, int bonusPointFactor) {
            LOG_INFOH(L"CALLED: minNgramLen={}, maxNgramLen={}, maxBonusPoint={}, bonusPointFactor={}", minNgramLen, maxNgramLen, maxBonusPoint, bonusPointFactor);
            minLen = minNgramLen;
            maxLen = maxNgramLen;
            ngramMaxBonusPoint = maxBonusPoint;
            ngramBonusPointFactor = bonusPointFactor;
        }

        int updateEntry(const String& word, int delta) {
            int count = realtimeDict[word] += delta;
            realtimeNgram_updated = true;
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
            utils::IfstreamReader reader(ngramFilePath);
            if (reader.success()) {
                for (const auto& line : reader.getAllLines()) {
                    auto items = utils::split(utils::replace_all(utils::strip(line), L" +", L"\t"), '\t');
                    if (items.size() == 2 && !items[0].empty() && items[0][0] != L'#' && !items[1].empty()) {
                        String sCount = items[1];
                        if (isDecimalString(sCount)) {
                            int count = std::stoi(sCount);
                            realtimeDict[items[0]] = count;
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
                        int count = ngramMaxBonusPoint;  // ユーザー定義のN-gramは、デフォルトで最大ボーナスポイントを与える
                        if (!items[1].empty()) {
                            String sCount = items[1];
                            if (isDecimalString(sCount)) {
                                count = std::stoi(sCount);
                            }
                        }
                        userDict[items[0]] = count;
                        userDict[L"〓" + items[0]] = count;
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
                            //if (count < 0 || count > 1 || (count == 1 && Reporting::Logger::IsWarnEnabled())) {
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

        int calcBonus(int bonusPoint) {
            double point = bonusPoint;
            if (point > ngramMaxBonusPoint) {
                if (point <= ngramMaxBonusPoint * 2) {
                    point = ngramMaxBonusPoint + (point - ngramMaxBonusPoint) * 0.4;
                } else if (point <= ngramMaxBonusPoint * 4) {
                    point = ngramMaxBonusPoint * 1.4 + (point - ngramMaxBonusPoint * 2) * 0.2;
                } else if (point <= ngramMaxBonusPoint * 40) {
                    point = ngramMaxBonusPoint * 1.8 + (point - ngramMaxBonusPoint * 4) * 0.1;
                } else if (point <= ngramMaxBonusPoint * 400) {
                    point = ngramMaxBonusPoint * 2.16 + (point - ngramMaxBonusPoint * 40) * 0.02;
                } else {
                    point = ngramMaxBonusPoint * 9.36 + (point - ngramMaxBonusPoint * 400) * 0.01;
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
        std::vector<int> commonPrefixSearch(const String& str, size_t pos) {
            LOG_DEBUG(L"ENTER: str={}, pos={}", str, pos);
            std::vector<int> result;
            result.resize(maxLen + 1, 0);  // エントリの長さでインデックスされる。値 0 はエントリがないことを表す。
            if (minLen > 0) {
                size_t end = std::min(pos + maxLen, str.size());
                for (size_t i = pos + minLen; i <= end; ++i) {
                    size_t len = i - pos;
                    if (len == 1 && utils::is_kanji(str[pos])) {
                        if (pos == 0 || pos + 1 >= str.size() || isKanjiOrGeta(str[pos - 1]) || isKanjiOrGeta(str[pos + 1])) {
                            // 1文字の漢字で、前後も漢字かゲタ文字の場合は、N-gramエントリとして扱わない
                            continue;
                        }
                    }
                    String key = str.substr(pos, i - pos);
                    int count = 0;
                    // リアルタイムN-gram辞書とユーザー定義N-gram辞書の両方を検索して、カウントを合算する
                    auto it = realtimeDict.find(key);
                    if (it != realtimeDict.end()) {
                        count = it->second;
                        LOG_DEBUG(L"realtime FOUND: key={}, count={}", key, it->second);
                    }
                    it = userDict.find(key);
                    if (it != userDict.end()) {
                        count += it->second;
                        LOG_DEBUG(L"userDic FOUND: key={}, count={}", key, it->second);
                    }
                    if (count > 0) {
                        result[len] = calcBonus(count);
                        LOG_DEBUG(L"FOUND: key={}, count={}, bonus[{}]={}", key, count, len, result[len]);
                    }
                }
            }
            LOG_DEBUG(L"LEAVE: result.size()={}", result.size());
            return result;
        }
    } // namespace RealtimeDict
} // namespace analyze
