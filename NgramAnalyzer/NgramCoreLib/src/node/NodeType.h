#pragma once
#include "std_utils.h"

namespace node
{
    enum class NodeType {
        /**
         * 通常ノード
         */
        NORMAL_NODE,

        /**
         * 文頭ノード (文の先頭ノードの前に置かれる仮想ノード)
         */
        BOS_NODE,

        /**
         * 文末ノード (文の末尾ノードの後に置かれる仮想ノード)
         */
        EOS_NODE,

        /**
         * N-Best解析の終わりを示す仮想ノード
         */
        EONBEST_NODE
    };

    inline String getNodeTypeStr(NodeType nt) {
        switch (nt) {
        case NodeType::NORMAL_NODE: return L"NORMAL";
        case NodeType::BOS_NODE: return L"BOS";
        case NodeType::EOS_NODE: return L"EOS";
        case NodeType::EONBEST_NODE: return L"EONBEST";
        default: return L"UNDEFINED";
        }
    }

    enum class WordType {
        // BOS
        BOS,
        // EOS
        EOS,
        // ひらがなのみ
        HIRAGANA,
        // 漢字のみ
        KANJI,
        // ひらがなと漢字の混合
        MIXED,
        // ゲタ文
        GETA,
        // その他未定義
        UNKNOWN
    };

    inline String getWordTypeStr(WordType nt) {
        switch (nt) {
        case WordType::BOS: return L"BOS";
        case WordType::EOS: return L"EOS";
        case WordType::HIRAGANA: return L"HIRAGANA";
        case WordType::KANJI: return L"KANJI";
        case WordType::MIXED: return L"MIXED";
        case WordType::GETA: return L"GETA";
        case WordType::UNKNOWN: return L"UNKNOWN";
        default: return L"UNDEFINED";
        }
    }

} // namespace node
