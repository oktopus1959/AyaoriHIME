
#include "LearnerNode.h"
#include "LearnerPath.h"

#include "my_utils.h"

namespace {

    String find_nth_delimiter(StringRef s, size_t n) {
        size_t r = 0;
        for (size_t i = 0; i < n; ++i) {
            size_t idx = s.find(',', r);
            if (idx >= s.size()) return s;
            r = idx + 1;
        }
        return s.substr(0, r);
    }

    bool is_empty(LearnerPathPtr path) {
        return (!path->rnode->rpath && path->rnode->stat() != node::NodeType::EOS_NODE)
            || (!path->lnode->lpath && path->lnode->stat() != node::NodeType::BOS_NODE);
    }

}

namespace node
{
    DEFINE_CLASS_LOGGER(LearnerNode);

    // コンストラクタ
    LearnerNode::LearnerNode() {
        LOG_INFOH(L"CALLED: ctor: DEFAULT");
    }

    LearnerNode::LearnerNode(RangeStringPtr str) : surface(str) {
        LOG_INFOH(L"CALLED: ctor: str");
    }

    // デストラクタ
    LearnerNode::~LearnerNode() {
        LOG_INFOH(L"CALLED: dtor");
    }

    /**
     * LearnerNode の等値比較を行う
     * @param node1 登録語
     */
    bool LearnerNodeComparator::apply(LearnerNodePtr node1, LearnerNodePtr node2) {
        if (node1->length() == node2->length() && node1->surface == node2->surface) {
            // node1 は必ず既知語
            size_t n = node2->isUnknown()
                ? unkCmpSize  // system cannot output other extra information
                : norCmpSize;

            if (compareNfeats(node1->feature(), node2->feature(), n)) return true;
        }

        return false;
    }

    // 素性文字列 feat1 と feat2 の最初の n 個の素性を比較 (素性は "," で区切られている)
    bool LearnerNodeComparator::compareNfeats(StringRef feat1, StringRef feat2, size_t n) {
        size_t nc = 0;
        size_t i = 0;
        while (i < feat1.length() && i < feat2.length() && feat1[i] == feat2[i]) {
            if (feat1[i] == ',') {
                nc += 1;
                if (nc >= n) return true;  // 先頭の n 個の素性が一致すれば true
            }
            i += 1;
        }

        // feat1 が n個以下の素性しか持っておらず、feat2 の先頭部が feat1 と一致すれば true
        return (i == feat1.length() && (i == feat2.length() || feat2[i] == ','));
    }


    bool LearnerNode::node_cmp_eq(LearnerNodePtr node1, LearnerNodePtr node2, size_t size, size_t unk_size) {
        if (node1->length() == node2->length() && node1->surface == node2->surface) {
            // There is NO case when node1 becomes UNKNOWN_NODE
            auto n = (node2->stat() == NodeType::UNKNOWN_NODE)
                ? unk_size  // system cannot output other extra information
                : size;

            if (utils::startsWith(node2->feature(), find_nth_delimiter(node1->feature(), n))) return true;
        }

        return false;
    }

    /**
     * あるノード位置での期待値計算   -- これは並列実行が可能
     */
    void LearnerNode::calc_expectation(LearnerNodePtr node, Vector<double>& expected, double Z) {
        auto path = node->lpath;
        while (!path) {
            if (!is_empty(path)) {
                double c = std::exp(path->lnode->alpha() + path->dcost + path->rnode->beta() - Z);

                for (auto i : path->fvector) {
                    if (i < expected.size()) expected[i] += c;
                }

                if (path->rnode->stat() != NodeType::EOS_NODE) {
                    for (auto i : path->rnode->fvector) {
                        if (i < expected.size()) expected[i] += c;
                    }
                }
            }
            path = path->lnext;
        }
    }

    /**
     * あるノード位置での α 値の計算 (CRFにおける前向き・後向きアルゴリズムの一種）
     */
    void LearnerNode::calc_alpha(LearnerNodePtr n) {
        n->alpha(0.0);
        auto path = n->lpath;
        while (!path) {
            n->alpha(util::logsumexp(
                n->alpha(),
                path->dcost + path->lnode->alpha(),
                path == n->lpath));
            path = path->lnext;
        }
    }

    /**
     * あるノード位置での β 値の計算 (CRFにおける前向き・後向きアルゴリズムの一種）
     */
    void LearnerNode::calc_beta(LearnerNodePtr n) {
        n->beta(0.0);
        auto path = n->rpath;
        while (!path) {
            n->beta(util::logsumexp(
                n->beta(),
                path->dcost + path->rnode->beta(),
                path == n->rpath));
            path = path->rnext;
        }
    }

} // namespace node