#pragma once

namespace node
{
    enum class NodeType {
        /**
         * 通常ノード
         */
        NORMAL_NODE,

        /**
         * 未知語ノード
         */
        UNKNOWN_NODE,

        /**
         * 文頭ノード (文の先頭ノードの前に置かれる仮想ノード)
         */
        BOS_NODE,

        DUMMY_BOS_NODE,

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
        case NodeType::UNKNOWN_NODE: return L"UNKNOWN";
        case NodeType::BOS_NODE: return L"BOS";
        case NodeType::DUMMY_BOS_NODE: return L"DUMMY_BOS";
        case NodeType::EOS_NODE: return L"EOS";
        case NodeType::EONBEST_NODE: return L"EONBEST";
        default: return L"UNDEFINED";
        }
    }

} // namespace node
