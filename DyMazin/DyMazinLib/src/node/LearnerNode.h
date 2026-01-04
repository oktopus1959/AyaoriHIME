#pragma once

#include "std_utils.h"
#include "string_utils.h"

#include "NodeT.h"

namespace node
{
    class LearnerNode;
    class LearnerPath;

    using LearnerNodePtr = SharedPtr<LearnerNode>;
    using LearnerPathPtr = SharedPtr<LearnerPath>;
    //typedef LearnerNode* LearnerNodePtr;

    class LearnerNode : public NodeBase {
        DECLARE_CLASS_LOGGER;
    public:
        // コンストラクタ
        LearnerNode();
        LearnerNode(RangeStringPtr str);
        // デストラクタ
        ~LearnerNode();

    public:
        /**
         * pointer to the previous node.
         */
        LearnerNodePtr prev;

        /**
         * pointer to the next node.
         */
        LearnerNodePtr next;

        /**
         * pointer to the right path.
         * this value is NULL if analyze-only-one mode.
         */
        LearnerPathPtr rpath;

        /**
         * pointer to the right path.
         * this value is NULL if analyze-only-one mode.
         */
        LearnerPathPtr lpath;

    public:
        RangeStringPtr surface;
        SharedPtr<LearnerNode> anext;
        double wdcost = 0.0;
        //  override def wcostAdd(wc: Int) { wdcost += wc }
        //  override def wcostSet(wc: Int) { }
        double dcost = 0.0;
        Vector<size_t> fvector;

        static bool node_cmp_eq(LearnerNodePtr node1, LearnerNodePtr node2, size_t size, size_t unk_size);

        /**
         * あるノード位置での期待値計算   -- これは並列実行が可能
         */
        static void calc_expectation(LearnerNodePtr node, Vector<double>& expected, double Z);

        /**
         * あるノード位置での α 値の計算 (CRFにおける前向き・後向きアルゴリズムの一種）
         */
        static void calc_alpha(LearnerNodePtr n);

        /**
         * あるノード位置での β 値の計算 (CRFにおける前向き・後向きアルゴリズムの一種）
         */
        static void calc_beta(LearnerNodePtr n);

        // Learnerノードファクトリ
        static SharedPtr<LearnerNode> Create(RangeStringPtr str) {
            return MakeShared<LearnerNode>(str);
        }
    };

    /**
     * LearnerNode の等値比較を行うクラス
     */
    class LearnerNodeComparator {
        size_t norCmpSize;      // 既知語の素性比較数
        size_t unkCmpSize;      // 未知語の素性比較数

        // 素性文字列 feat1 と feat2 の最初の n 個の素性を比較 (素性は "," で区切られている)
        bool compareNfeats(StringRef feat1, StringRef feat2, size_t n);

    public:
        /**
         * LearnerNode の等値比較を行う
         * @param node1 登録語
         */
        bool apply(LearnerNodePtr node1, LearnerNodePtr node2);
    };

} // namespace node

using LearnerNodePtr = node::LearnerNodePtr;
