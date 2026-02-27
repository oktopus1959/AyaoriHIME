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

    public:
        Viterbi(OptHandlerPtr opts);
        ~Viterbi();

        /**
         * 形態素解析処理
         * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
         */
        void analyze(LatticePtr lattice, StringRef tempDictEntries);

        /** ユーザー辞書の再オープン */
        void reload_userdics();

    };

} // namespace analyzer