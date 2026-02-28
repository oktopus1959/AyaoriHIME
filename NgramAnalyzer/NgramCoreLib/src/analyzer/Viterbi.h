#pragma once

#include "OptHandler.h"
#include "Logger.h"

#include "Lattice.h"

namespace analyzer {
    class Viterbi {
        DECLARE_CLASS_LOGGER;
    private:
        class Impl;
        UniqPtr<Impl> pImpl;

        /**
         * 形態素解析処理
         * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
         */
        void analyze(LatticePtr lattice, StringRef tempDictEntries);

    public:
        Viterbi(OptHandlerPtr opts);
        ~Viterbi();

        /**
         * N-Best 解析の実行。N-Bestな解析結果が一つずつ格納されたArrayを返す。
         * @param sentence 解析対象文
         * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
         * @param results N-Bestな解析結果を格納するvector
         * @param nBest N-Bestの最大数
         * @param mazePenalty 交ぜ書き候補に与えるペナルティ。これが負値ならボーナスなり、他の交ぜ書き候補を含めた解を返す
         * @return 最良解析結果のコスト
         */
        int parseNBest(StringRef sentence, StringRef tempEntries, Vector<String>& results, size_t nBest, bool needResults);

        /** ユーザー辞書の再オープン */
        void reload_userdics();

    };

} // namespace analyzer