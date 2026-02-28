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

        static std::map<String, int> realtimeDict;

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
        int loadNgramFile(StringRef ngramFilePath) {
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

        int calcBonus(int bonusPoint) {
            if (bonusPoint > ngramMaxBonusPoint) bonusPoint = ngramMaxBonusPoint;
            return bonusPoint * ngramBonusPointFactor;
        }

        // 与えられた文字列の先頭部分にマッチするエントリ（複数可）を検索する
        // @param str 検索対象の文字列
        // @param pos 検索開始位置
        // @return マッチしたエントリのボーナスポイントのリスト。(各エントリの長さのをインデックスとする)
        std::vector<int> commonPrefixSearch(const String& str, size_t pos) {
            LOG_DEBUG(L"ENTER: str={}, pos={}", str, pos);
            std::vector<int> result;
            result.resize(maxLen + 1, 0);  // インデックスはエントリの長さを表す。0 はエントリがないことを表す。
            if (minLen > 0) {
                size_t end = std::min(pos + maxLen, str.size());
                for (size_t i = pos + minLen; i <= end; ++i) {
                    size_t len = i - pos;
                    if (len == 1 && utils::is_pure_kanji(str[pos])) {
                        if (pos == 0 || pos + 1 >= str.size() || !utils::is_hiragana(str[pos - 1]) || !utils::is_hiragana(str[pos + 1])) {
                            continue;
                        }
                    }
                    String key = str.substr(pos, i - pos);
                    auto it = realtimeDict.find(key);
                    if (it != realtimeDict.end()) {
                        result[len] = calcBonus(it->second);
                        LOG_DEBUG(L"FOUND: key={}, bonus[{}]={}", key, len, result[len]);
                    }
                }
            }
            LOG_DEBUG(L"LEAVE: result.size()={}", result.size());
            return result;
        }
    } // namespace RealtimeDict
} // namespace analyze
