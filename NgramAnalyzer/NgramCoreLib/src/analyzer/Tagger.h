#pragma once

#include "std_utils.h"
#include "Logger.h"

#include "Model.h"

namespace analyzer {
    class Tagger {
        DECLARE_CLASS_LOGGER;

        ModelPtr model;

        //Vector<String> warnings;

    public:
        Tagger(ModelPtr model);

        /**
         * N-Best 解析の実行。N-Bestな解析結果が一つずつ格納されたArrayを返す。
         * @param sentence 解析対象文
         * @param realtimeEntries リアルタイム登録による辞書エントリ
         * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
         * @param results N-Bestな解析結果を格納するvector
         * @param nBest N-Bestの最大数
         * @return 最良解析結果のコスト
         */
        int parseNBest(StringRef sentence, const std::map<String, int>& realtimeEntries, StringRef tempEntries, Vector<String>& results, size_t nBest, bool needResults);

    };
} // namespace analyzer

using TaggerPtr = SharedPtr<analyzer::Tagger>;

