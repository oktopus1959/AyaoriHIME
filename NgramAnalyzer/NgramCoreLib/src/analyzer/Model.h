#pragma once

#include "std_utils.h"

#include "OptHandler.h"
#include "Lattice.h"
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

        Viterbi viterbi;

        String bos_feature;

        /**
         * 辞書情報
         */
        DictionaryInfoPtr dictionary_info();

        /**
         * 形態素解析の実行
         * @param sentence 解析対象文
         * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
         * @param nBest N-Best解の個数 (省略可; デフォルト=1)
         * @return 解析結果を格納したラティスオブジェクト
         */
        LatticePtr analyze(StringRef sentence, StringRef tempEntries, size_t nBest);

    public:
        // Constructor
        Model(OptHandlerPtr opts);

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

        //static String getBosFeature(OptHandlerPtr opts, StringRef defval = L"");

        /** ユーザー辞書の再ロード */
        void reload_userdics();

    };

} // namespace analyzer

using ModelPtr = SharedPtr<analyzer::Model>;