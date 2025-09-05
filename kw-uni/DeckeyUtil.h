#pragma once

#include "deckey_id_defs.h"

namespace DeckeyUtil {
    inline bool is_combo_deckey(int deckey) {
        return deckey >= COMBO_DECKEY_START && deckey < COMBO_DECKEY_END;
    }

    inline bool is_ordered_combo(int deckey) {
        return deckey >= ORDERED_COMBO_DECKEY_START && deckey < ORDERED_COMBO_DECKEY_END;
    }

    // 通常キーに変換
    inline int unshiftDeckey(int deckey) {
        return (deckey % PLANE_DECKEY_NUM);
    }

    inline int unshiftIfOrdered(int deckey) {
        return is_ordered_combo(deckey) ? (deckey % PLANE_DECKEY_NUM) : deckey;
    }
}
