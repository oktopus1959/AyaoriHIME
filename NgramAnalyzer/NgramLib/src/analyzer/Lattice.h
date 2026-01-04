#pragma once

#include "std_utils.h"
#include "string_utils.h"
#include "exception.h"
#include "constants/Constants.h"

#include "node/Node.h"
#include "node/Path.h"
using Node = node::Node;
using NodePtr = node::NodePtr;
using NodeType = node::NodeType;
using Path = node::Path;
using PathPtr = node::PathPtr;

#include "Lazy.h"

#include "RangeString.h"
#include "NBestGenerator.h"

using namespace util;

namespace analyzer {
    class Lattice;
}
using LatticePtr = std::shared_ptr<analyzer::Lattice>;

namespace analyzer {

    /**
     * Lattice
     */
    class Lattice {
        DECLARE_CLASS_LOGGER;

    private:
        // Nodeのライフサイクルを管理するためのNodeプール
        Vector<SharedPtr<Node>> node_pool;

        // Pathのライフサイクルを管理するためのPathプール
        Vector<SharedPtr<Path>> path_pool;

    public:
        RangeStringPtr sentence;

    public:
        // 新しいノードを作成して返す
        NodePtr newNode(RangeStringPtr str, node::NodeType nt) {
            auto node = Node::Create(str, nt);
            node_pool.push_back(node);
            return node.get();
        }

        NodePtr newNode(size_t from, size_t to, node::NodeType nt) {
            return newNode(sentence->subString(from, to), nt);
        }

        PathPtr newPath() {
            auto path = Path::Create();
            path_pool.push_back(path);
            return path.get();
        }

    protected:
        String sentinelFeature;
        //  protected val request_type: Int
        size_t nBest;
        //  val theta : Double

        // end_nodes[idx]: idx位置を終点とするNodeの集合
        Vector<Deque<NodePtr>> end_nodes;      // initialize() で (sentence.length + 1)に resize

        // begin_nodes[idx]: idx位置を開始点とするNodeの集合
        //Vector<Deque<NodePtr>> begin_nodes;    // initialize() で (sentence.length + 1)に resize
        Vector<Vector<NodePtr>> begin_nodes;    // initialize() で (sentence.length + 1)に resize

        // 任意位置での空の始端Node集合を表す(定数)
        Vector<NodePtr> empty_begin_nodes;
        // 任意位置での空の終端Node集合を表す(定数)
        Deque<NodePtr> empty_end_nodes;

        NodePtr createEONnode() {
            auto node = newNode(sentence->end(), sentence->end(), NodeType::EONBEST_NODE);
            //node->setStat(NodeType::EONBEST_NODE);
            return node;
        }

    private:
        void initialize();

        NodePtr createSentinel(size_t pos, NodeType status) {
            auto node = newNode(pos, pos, status);  // BOS_KEY is dummy surface string
            //node->isbest = 1;
            node->setBest(true);
            //node->setStat(status);
            return node;
        }

    public:
        //  var theta : Double = 0d
        double Z = 0.0;

        //public:
        // 仮想文頭ノードを返す
        const NodePtr& bosNode() const {
            return end_nodes.front().front();
        }

        // 仮想文末ノードを返す
        const NodePtr& eosNode() const {
            return begin_nodes.back().front();
        }

        // idx を開始位置とするノード群にセット
        void setBeginNodes(size_t idx, const Vector<NodePtr>& nodes) {
            if (idx < begin_nodes.size()) begin_nodes[idx] = nodes;
        }

        const Vector<NodePtr>& getBeginNodes(size_t idx) const {
            return idx < begin_nodes.size() ? begin_nodes[idx] : empty_begin_nodes;
        }

        const Vector<NodePtr>& getLastBeginNodes() const {
            return begin_nodes.empty() ? empty_begin_nodes : begin_nodes.back();
        }

        // end node リストの先頭に追加する。
        void addEndNode(const NodePtr& node) {
            auto endpos = node->surface()->end();
            end_nodes[endpos].push_front(node);
        }

        // idx位置の end_nodes (隣接後端ノード列)を返す
        const Deque<NodePtr>& getEndNodes(size_t idx) const {
            return idx < end_nodes.size() ? end_nodes[idx] : empty_end_nodes;
        }

        const Deque<NodePtr>& getLastNonEmptyEndNodes() const{
            for (size_t pos = end_nodes.size(); pos > 0; --pos) {
                const auto& nodes = end_nodes[pos - 1];
                if (!nodes.empty()) return nodes;
            }
            return empty_end_nodes;
        }

        /**
         * N-Best 解析か
         */
        bool isNBest() const {
            return nBest != 1;
        }

        /**
         * 最良解のコストを取得する
         */
        int bestCost() const;

    private:
        Lattice(
            RangeStringPtr sentence,
            StringRef sentinelFeature,
            size_t nBest);

        Lattice(StringRef sentence, StringRef bosEosFeat, size_t nBest = 1);

    public:
        ~Lattice();

    public:
        static SharedPtr<Lattice> CreateLattice(RangeStringPtr sentence, StringRef sentinelFeature, size_t nBest);

        static SharedPtr<Lattice> CreateLattice(StringRef sentence, StringRef sentinelFeature, size_t nBest);

    private:
        LazyByCreator<NBestGenerator> nbestGenerator;
        //Lazy<NBestGenerator> nbestGenerator;
        //mutable SharedPtr<NBestGenerator> nbestGenerator;

    public:
        // String warnings;

        /**
         * 解を取得する
         * @param results N個の解を格納したArrayを返す
         * @return 最良コスト
         */
        int getSolutions(Vector<String>& results, bool needResult) const {
            //warnings.clear();
            if (needResult) {
                if (nBest == 1) {
                    results.push_back(toString());
                } else {
                    enumNBestAsString(results);
                }
            }
            return bestCost();
        }

        /**
        * N-Best解の取得
        * @param result N個の解を格納したArray
        */
        void enumNBestAsString(Vector<String>& result) const;

        /**
         * BOS/EOSを除いたノードリストを返す
         */
        Vector<Node*> nodeList() const {
            Vector<Node*> result;
            Node* node = bosNode()->next();     // BOSの次
            while (node && node->next()) {
                result.push_back(node);
                node = node->next();
            }
            return result;
        }

        /**
        * BOS/EOSを含むノードリストを返す
        */
        Vector<Node*> nodeListWithSentinel() const {
            Vector<Node*> result;
            Node* node = bosNode();
            while (node) {
                result.push_back(node);
                node = node->next();
            }
            return result;
        }

        /**
         * 同じ位置で同じ長さのノードを集める
         */
        Vector<Node*> gatherSameLenNodes(size_t pos, int len) const;

        /**
        * 解析結果を文字列で返す
        */
        String toString() const;

        String toMazeString() const;

        String writeLattice() const;

        static String writeLattice(const Lattice& lattice);

    };

    ///**
    // * LearnerLattice companion
    // */
    //object LearnerLattice {
    //  def apply(sentence: String, bosEosFeat: String) = {
    //    new LearnerLattice(RangeString.create(sentence), bosEosFeat,
    //        LearnerNodeFactory, LearnerPathFactory)
    //  }
    //}
} // namespace analyzer
