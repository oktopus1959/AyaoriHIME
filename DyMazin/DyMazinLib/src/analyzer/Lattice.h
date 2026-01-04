#pragma once

#include "std_utils.h"
#include "string_utils.h"
#include "exception.h"
#include "constants/Constants.h"

#include "util/OptHandler.h"
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
    class TextWriter;
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
        SharedPtr<TextWriter> writer;

    public:
        // 新しいノードを作成して返す
        NodePtr newNode(RangeStringPtr str) {
            auto node = Node::Create(str);
            node_pool.push_back(node);
            return node.get();
        }

        NodePtr newNode(size_t from, size_t to) {
            return newNode(sentence->subString(from, to));
        }

        PathPtr newPath() {
            auto path = Path::Create();
            path_pool.push_back(path);
            return path.get();
        }

    protected:
        String sentinelFeature;
        String sentinelBosFeature = L"BOS";
        String sentinelGetaFeature = L"GETA";
        int sentinelGetaRcAttr = 1285;      // 名詞一般
        //int sentinelGetaCost = 1000;
        //  protected val request_type: Int
        size_t nBest;
        //  val theta : Double

        NodePtr bos_node;

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
            auto node = newNode(sentence->end(), sentence->end());
            node->setStat(NodeType::EONBEST_NODE);
            return node;
        }

    private:
        void initialize(int getaCost);

        NodePtr createSentinel(size_t pos, NodeType status);

        NodePtr createDummyBos();

        NodePtr createGetaBos(int getaCost);

    public:
        //  var theta : Double = 0d
        double Z = 0.0;

        //public:
        // 仮想文頭ノードを返す
        const NodePtr& bosNode() const {
            return bos_node;
        }

        const Vector<NodePtr> bosNodes() const {
            return { bos_node };
        }

        // Dummy文頭ノードを返す
        const NodePtr& dummyBosNode() const {
            const NodePtr& bos1 = end_nodes[0].front();
            if (end_nodes[0].size() < 2) {
                return bos1;
            }
            const NodePtr& bos2 = end_nodes[0].back();
            return bos1->next() ? bos1 : bos2;
        }

        const Vector<NodePtr> dummyBosNodes() const {
            if (end_nodes[0].size() < 2) {
                return { end_nodes[0].front() };
            }
            return { end_nodes[0].front(), end_nodes[0].back() };
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
        Lattice(RangeStringPtr sentence, StringRef sentinelFeature, SharedPtr<TextWriter> writer, size_t nBest, int getaCost);

        Lattice(StringRef sentence, StringRef bosEosFeat, SharedPtr<TextWriter> writer, size_t nBest = 1, int getaCost = -1);

    public:
        ~Lattice();

    public:
        static SharedPtr<Lattice> CreateLattice(OptHandlerPtr opts, RangeStringPtr sentence, StringRef sentinelFeature, SharedPtr<TextWriter> writer, size_t nBest);

        static SharedPtr<Lattice> CreateLattice(OptHandlerPtr opts, StringRef sentence, StringRef sentinelFeature, SharedPtr<TextWriter> writer, size_t nBest);

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
        int getSolutions(Vector<String>& results, bool otherMazeCand = false) const;

        /**
        * N-Best解の取得
        * @param result N個の解を格納したArray
        */
        void enumNBestAsString(Vector<String>& result) const;

        /**
         * BOS/EOSを除いたノードリストを返す
         */
        Vector<Node*> nodeList() const;

        /**
        * BOS/EOSを含むノードリストを返す
        */
        Vector<Node*> nodeListWithSentinel() const;

        /**
         * 同じ位置で同じ長さのノードを集める
         */
        Vector<Node*> gatherSameLenNodes(size_t pos, int len, StringRef myHinshi) const;

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
