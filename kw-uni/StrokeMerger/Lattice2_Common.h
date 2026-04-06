#pragma once

#include "string_type.h"
#include "Constants.h"

//#include "Lattice.h"

#define LOG_SAVE_DICT LOG_INFOH
#define _LOG_DETAIL if (SETTINGS->multiStreamDetailLog || IS_LOG_INFO_ENABLED) LOG_INFO_QUEUE
#define _LOG_DETAILH if (SETTINGS->multiStreamDetailLog || IS_LOG_INFOH_ENABLED) LOG_INFO_QUEUE
#define _LOG_DETAILW if (SETTINGS->multiStreamDetailLog || IS_LOG_WARN_ENABLED) LOG_INFO_QUEUE
#include "DebugLog.h"

namespace lattice2 {

    // ビームサイズ
    //size_t BestKSize = 5;
    inline size_t maxCandidatesSize = 0;

    //// Realtime 3gram のカウントからボーナス値を算出する際の係数
    //int REALTIME_TRIGRAM_BONUS_FACTOR = 100;

    //int TIER1_NUM = 5;
    //int TIER2_NUM = 10;

    // cost ファイルに登録がある場合のデフォルトのボーナス
    inline int DEFAULT_WORD_BONUS = 1000;

    inline int DIFFERENT_HINSHI_PENALTY = 10000;

    inline bool isDecimalString(StringRef item) {
        return utils::reMatch(item, L"[+\\-]?[0-9]+");
    }

} // namespace lattice2

