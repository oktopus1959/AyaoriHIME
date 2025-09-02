#pragma once

#include "string_type.h"

//#include "Lattice.h"

namespace lattice2 {

    // ビームサイズ
    //size_t BestKSize = 5;
    inline size_t maxCandidatesSize = 0;

    //// Realtime 3gram のカウントからボーナス値を算出する際の係数
    //int REALTIME_TRIGRAM_BONUS_FACTOR = 100;

    //int TIER1_NUM = 5;
    //int TIER2_NUM = 10;

    // グローバルな後置書き換えマップ
    inline std::map<MString, MString> globalPostRewriteMap;

} // namespace lattice2

