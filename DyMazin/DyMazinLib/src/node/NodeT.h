#pragma once

#include "std_utils.h"
#include "string_type.h"
#include "misc_utils.h"
#include "Logger.h"

#include "analyzer/RangeString.h"

#include "NodeType.h"

namespace node
{
    /**
     * ノード(形態素)の基本情報
     */
    class NodeBase {
    private:
        // 形態素の表層形文字列
        RangeStringPtr _surface;

        // 素性文字列
        String _feature;

        // ノードid
        int _id = 0;

        // 形態素に前接する空白文字も含んだ文字列長
        int _rlength = 0;

        /**
         * right attribute id
         */
        int _rcAttr = 0;

        /**
         * left attribute id
         */
        int _lcAttr = 0;

        //  /**
        //   * unique part of speech id. This value is defined in "pos.def" file.
        //   */
        //  int posid;

        /**
         * character type
         */
        int char_type = 0;

        /**
         * ノード種別
         */
        NodeType _stat = NodeType::NORMAL_NODE;

        /**
         * set 1 if this node is best node.
         */
        //int isbest = 0;
        bool _isBest = false;

        /**
         * forward accumulative log summation.
         */
        double _alpha = 0.0;

        /**
         * backward accumulative log summation.
         */
        double _beta = 0.0;

        /**
         * marginal probability.
         */
        double _prob = 0.0;

        /**
         * word cost.
         */
         //  virtual void wcostAdd(int wc) = 0;
         //  virtual void wcostSet(int wc) = 0;
        int _wcost = 0;

    public:
        inline RangeStringPtr surface() const { return _surface; }

        inline const String& feature() const { return _feature; }
        inline void feature(const String& feature) { _feature = feature; }

        inline int id() const { return _id; }

        inline int rlength() const { return _rlength; }
        inline void rlength(int len) { _rlength = len; }

        inline int rcAttr() const { return _rcAttr; }
        inline void rcAttr(int attr) { _rcAttr = attr; }
        inline int lcAttr() const { return _lcAttr; }
        inline void lcAttr(int attr) { _lcAttr = attr; }

        inline int charType() const { return char_type; }
        inline void charType(int ctype) { char_type = ctype; }

        /**
         * NodeType String
         */
        inline String getNodeType() const { return getNodeTypeStr(_stat); }

        inline NodeType stat() const { return _stat; }

        inline void stat(NodeType stat) { _stat = stat; }

        /**
         * Unknown word
         */
        inline bool isUnknown() const { return _stat == NodeType::UNKNOWN_NODE; }

        /**
         * BOS node?
         */
        inline bool isBOS() const { return _stat == NodeType::BOS_NODE; }

        /**
         * EOS node?
         */
        inline bool isEOS() const { return _stat == NodeType::EOS_NODE; }

        /**
         * length of the surface form.
         */
        inline int length() const { return !_surface ? 0 : (int)_surface->length(); }

        /**
         * ノード種別が指定のものと等しいか
         */
        inline bool isStat(NodeType s) const { return _stat == s; }

        inline bool isBest() const { return _isBest; }
        inline void isBest(bool best) { _isBest = best; }

        inline double alpha() const { return _alpha; }
        inline void alpha(double a) { _alpha = a; }

        inline double beta() const { return _beta; }
        inline void beta(double b) { _beta = b; }

        inline double prob() const { return _prob; }
        inline void prob(double p) { _prob = p; }

        inline int wcost() const { return _wcost; }
        inline void wcost(int cost) { _wcost = cost; }
        inline void addWcost(int cost) { _wcost += cost; }

        /**
         * verbose String
         */
        String toVerbose() const {
            return _surface->toString() + _T(": ") + _feature + _T(", ") + std::to_wstring((int)_stat);
        }

    public:
        NodeBase() { }

        NodeBase(RangeStringPtr sf)
            : _surface(sf)
        { }

    };

    //template<class N, class P>
    //class NodeT : public NodeBase {
    //public:
    //    /**
    //     * pointer to the previous node.
    //     */
    //    N* prev;

    //    /**
    //     * pointer to the next node.
    //     */
    //    N* next;

    //    /**
    //     * pointer to the right path.
    //     * this value is NULL if analyze-only-one mode.
    //     */
    //    P* rpath;

    //    /**
    //     * pointer to the right path.
    //     * this value is NULL if analyze-only-one mode.
    //     */
    //    P* lpath;

    //public:
    //    NodeT() { }

    //    NodeT(RangeStringPtr sf)
    //        : NodeBase(sf)
    //    { }

    //};

    ///**
    // * ノードのファクトリ
    // */
    //template<class N>
    //class NodeFactoryT {
    //    virtual SharedPtr<N> newNode(RangeStringPtr s) = 0;
    //};

} // namespace node