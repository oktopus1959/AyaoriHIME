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
         */
        void analyze(LatticePtr lattice);

        /** ユーザー辞書の再オープン */
        void reload_userdics();

    };

} // namespace analyzer