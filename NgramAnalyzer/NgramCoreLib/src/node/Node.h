#pragma once

#include "std_utils.h"

#include "analyzer/RangeString.h"
#include "NodeType.h"
#include "Logger.h"

namespace node
{
    class Node;

    // 生の Node ポインター
    typedef Node* NodePtr;

    class Path;

    class Node {
        DECLARE_CLASS_LOGGER;

    public:
        Node(RangeStringPtr sf, node::NodeType nt);
        ~Node();

    private:
        // Ngramの表層形文字列
        RangeStringPtr _surface;

        // Ngramに前接する空白文字も含んだ文字列長
        int _rlength = 0;

        // ひらがなのみ、漢字のみ、混合などの属性
        WordType _word_type;

        // ノード種別
        NodeType _stat = NodeType::NORMAL_NODE;

        // 最良ノードフラグ
        bool _isBest = false;

        // 先頭部のひらがな長
        bool _is_head_hiragana = false;

        // 末尾部のひらがな長
        bool _is_tail_hiragana = false;

        bool _is_short_hiragana = false;

        /**
         * pointer to the previous(left) node.
         */
        Node* _prev;

        /**
         * pointer to the next(right) node.
         */
        Node* _next;

        /**
         * pointer to the right path.
         * this value is NULL if analyze-only-one mode.
         */
        Path* _rpath;

        /**
         * pointer to the right path.
         * this value is NULL if analyze-only-one mode.
         */
        Path* _lpath;

    private:
        // 単語コスト
        int _wcost = 0;

        /**
         * best accumulative cost from bos node to this node.
         */
        int _accumCost = 0;
        int _accumCost2 = 0;

    public:
        inline RangeStringPtr surface() const { return _surface; }

        inline int length() const { return !_surface ? 0 : (int)_surface->length(); }

        inline int rlength() const { return _rlength; }
        inline void setRlength(int len) { _rlength = len; }

        inline WordType wordType() const { return _word_type; }
        inline String wordTypeStr() const { return node::getWordTypeStr(_word_type); }
        inline bool isKanji() const { return _word_type == WordType::KANJI; }
        inline bool isGeta() const { return _word_type == WordType::GETA; }

        inline String nodeTypeStr() const { return getNodeTypeStr(_stat); }
        inline NodeType stat() const { return _stat; }
        inline void setStat(NodeType stat) { _stat = stat; }

        inline bool isBOS() const { return _stat == NodeType::BOS_NODE; }
        inline bool isEOS() const { return _stat == NodeType::EOS_NODE; }

        inline bool isBest() const { return _isBest; }
        inline void setBest(bool best) { _isBest = best; }

        inline bool isHeadHiragana() const { return _is_head_hiragana; }
        inline bool isTailHiragana() const { return _is_tail_hiragana; }

        inline bool isShortHiragana() { return _is_short_hiragana; }

        // prev(left) node
        inline Node* prev() const { return _prev; }
        inline void setPrev(Node* prev) { _prev = prev; }

        // next(right) node
        inline Node* next() const { return _next; }
        inline void setNext(Node* next) { _next = next; }

        inline int wcost() const { return _wcost; }
        inline void setWcost(int cost) { _wcost = cost; }
        inline void addWcost(int cost) { _wcost += cost; }

        inline int accumCost() const { return _accumCost; }
        inline void setAccumCost(int cost) { _accumCost = cost; }

        inline int accumCost2() const { return _accumCost2; }
        inline void setAccumCost2(int cost) { _accumCost2 = cost; }

        inline Path* rpath() const { return _rpath; }
        inline void setRpath(Path* path) { _rpath = path; }

        inline Path* lpath() const { return _lpath; }
        inline void setLpath(Path* path) { _lpath = path; }

        /**
         * verbose String
         */
        String toVerbose() const;

        // ノードファクトリ
        static SharedPtr<Node> Create(RangeStringPtr str, node::NodeType nt);
    };

} // namespace node
