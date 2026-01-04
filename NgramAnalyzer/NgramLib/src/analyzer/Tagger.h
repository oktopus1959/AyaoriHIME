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
         * 形態素解析の実行 (最良解のみを出力)
         * @param sentence 解析対象の文
         * @return 解析結果
         */
        String parse(StringRef sentence, size_t nBest);

        /**
         * N-Best 解析の実行。N-Bestな解析結果が一つずつ格納されたArrayを返す。
         * @param sentence 解析対象文
         * @param results N-Bestな解析結果を格納するvector
         * @param nBest N-Bestの最大数
         * @return 最良解析結果のコスト
         */
        int parseNBest(StringRef sentence, Vector<String>& results, size_t nBest, bool needResults);

    };
} // namespace analyzer

using TaggerPtr = SharedPtr<analyzer::Tagger>;

