#pragma once

#include "std_utils.h"

#include "OptHandler.h"
#include "Lattice.h"
#include "TextWriter.h"
#include "Viterbi.h"
#include "dict/DictionaryInfo.h"

namespace analyzer {
    /**
     * Model class
     */
    class Model {
        DECLARE_CLASS_LOGGER;

    private:
        OptHandlerPtr opts;

        TextWriterPtr writer;

        Viterbi viterbi;

        String bos_feature;

        /**
         * 辞書情報
         */
        DictionaryInfoPtr dictionary_info();

    public:
        // Constructor
        Model(OptHandlerPtr opts);

        /**
         * 形態素解析の実行
         * @param sentence 解析対象文
         * @param nBest N-Best解の個数 (省略可; デフォルト=1)
         * @return 解析結果を格納したラティスオブジェクト
         */
        LatticePtr analyze(StringRef sentence, size_t nBest, int mazePenalty, int mazeConnPenalty, bool allowNonTerminal);

        static String getBosFeature(OptHandlerPtr opts, StringRef defval = L"");

        /** ユーザー辞書の再ロード */
        void reload_userdics();

    };

} // namespace analyzer

using ModelPtr = SharedPtr<analyzer::Model>;