#pragma once

#include "std_utils.h"

#include "NodeT.h"

namespace node
{
    class Node;

    // 生の Node ポインター
    typedef Node* NodePtr;

    class Path;

    class Node {
        DECLARE_CLASS_LOGGER;

    public:
        Node(RangeStringPtr sf);
        ~Node();

    private:
        // 形態素の表層形文字列
        RangeStringPtr _surface;

        // 素性文字列
        String _feature;

        // 形態素に前接する空白文字も含んだ文字列長
        int _rlength = 0;

        /**
         * character type
         */
        int char_type = 0;

        /**
         * ノード種別
         */
        NodeType _stat = NodeType::NORMAL_NODE;

        /**
         * right attribute id
         */
        int _rcAttr = 0;

        /**
         * left attribute id
         */
        int _lcAttr = 0;

        /**
         * this node is best node.
         */
        bool _isBest = false;

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
        /**
         * word cost.
         */
        int _wcost = 0;

        /**
         * best accumulative cost from bos node to this node.
         */
        int _accumCost = 0;
        int _accumCost2 = 0;

    public:
        inline RangeStringPtr surface() const { return _surface; }

        inline int length() const { return !_surface ? 0 : (int)_surface->length(); }

        inline const String& feature() const { return _feature; }
        inline void setFeature(const String& feature) { _feature = feature; }

        inline int rlength() const { return _rlength; }
        inline void setRlength(int len) { _rlength = len; }

        inline int charType() const { return char_type; }
        inline void setCharType(int ctype) { char_type = ctype; }

        inline String getNodeType() const { return getNodeTypeStr(_stat); }
        inline NodeType stat() const { return _stat; }
        inline void setStat(NodeType stat) { _stat = stat; }

        inline bool isUnknown() const { return _stat == NodeType::UNKNOWN_NODE; }
        inline bool isBOS() const { return _stat == NodeType::BOS_NODE; }
        inline bool isDummyBOS() const { return _stat == NodeType::DUMMY_BOS_NODE; }
        inline bool isEOS() const { return _stat == NodeType::EOS_NODE; }

        inline int rcAttr() const { return _rcAttr; }
        inline void setRcAttr(int attr) { _rcAttr = attr; }
        inline int lcAttr() const { return _lcAttr; }
        inline void setLcAttr(int attr) { _lcAttr = attr; }

        inline bool isBest() const { return _isBest; }
        inline void setBest(bool best) { _isBest = best; }

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
        static SharedPtr<Node> Create(RangeStringPtr str);
    };

} // namespace node
