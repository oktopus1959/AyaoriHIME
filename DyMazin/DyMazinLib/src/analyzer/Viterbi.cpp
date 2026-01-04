#include "exception.h"
#include "my_utils.h"

#include "Connector.h"
#include "Tokenizer.h"
#include "Viterbi.h"

#include "util/xsv_parser.h"
#include "featureDef.h"

#if 1 || defined(_DEBUG)
#define _LOG_DEBUGH_FLAG true
#undef LOG_DEBUGH
#define LOG_DEBUGH LOG_INFOH
#else
#define _LOG_DEBUGH_FLAG false
#endif

namespace analyzer {
    DECLARE_LOGGER;

    static const double kDefaultTheta = 0.75;

#if _LOG_DEBUGH_FLAG
    void showConnectionResult(const Vector<NodePtr>& nodes) {
        StringStream msg;
        msg << L"\n.\n--------------------------------------------------\n";
        msg << L"Connection results:\n";
        for (const auto& node : nodes) {
            auto prev = node->prev();
            msg << (prev ? (prev->isDummyBOS() ? prev->feature() : prev->surface()->toString()) : L"NONE");
            msg << L"[" << (prev ? prev->accumCost() : 0) << L"]";
            msg << L" < " << (node->accumCost() - (prev ? prev->accumCost(): 0) - node->wcost());
            msg << L" > " << node->toVerbose();
            msg << L" | [" << node->accumCost() << L"]";
            msg<< L"\n";
        };
        msg<< L"--------------------------------------------------\n";
        LOG_INFO(msg.str());
    }
#endif

    class Viterbi::Impl {
        DECLARE_CLASS_LOGGER;
        friend class Viterbi;

    private:
        OptHandlerPtr opts;

        //val whatLog = WhatLog();

        UniqPtr<Tokenizer> tokenizer;

        UniqPtr<Connector> connector;

        int cost_factor_;
        double theta;

    public:
        Impl(OptHandlerPtr opts) :
            opts(opts),
            tokenizer(MakeUniq<Tokenizer>(opts)),
            connector(MakeUniq<Connector>(*opts)),
            cost_factor_(opts->getInt(L"cost-factor", 800)),
            theta(opts->getDouble(L"theta", kDefaultTheta))
        {
            LOG_INFOH(L"ENTER: cost_factor_={}, theta={}", cost_factor_, theta);
            CHECK_OR_THROW(!tokenizer->getDictionaries().empty(), L"Dictionary is empty");
            LOG_INFOH(L"LEAVE");
        }

        //---------------------------------------------------------------------------
        ///**
        // * 形態素解析処理
        // */
        //void analyze(LatticePtr lattice) {
        //    LOG_INFO(L"ENTER");
        //    CHECK_OR_THROW(lattice && lattice->sentence,
        //        L"Viterbi.analyze: lattice must not be null and have non-null sentence");

        //    // viterbi 処理 (解析部本体)
        //    viterbi(lattice);
        //    // 最良コストのPathを next で連結する
        //    linkBestPath(lattice);
        //    LOG_INFO(L"LEAVE");
        //}

    private:
        //---------------------------------------------------------------------------
        // private
        /**
         * viterbi 処理 --
         * 単語の辞書引きと先行ノードとの接続処理を行って、ラティス構造を構築する。
         */
        void viterbi(LatticePtr lattice, int mazePenalty, int mazeConnPenalty, bool allowNonTerminal) {
            LOG_INFO(L"ENTER");
            auto sentence = lattice->sentence;
            auto len = sentence->length();
            auto begin = sentence->begin();
            auto end = sentence->end();

            // 処理前の番兵ノード
            //auto bos_node = lattice->bosNode();
            auto eos_node = lattice->eosNode();

            connect(lattice, lattice->dummyBosNodes(), lattice->bosNodes(), lattice->isNBest(), mazePenalty, mazeConnPenalty);

            // 文の先頭から末尾に向かって、形態素ノードを作成し、先行ノードと接続させてラティスを作っていく
            for (size_t pos = 0; pos < len; ++pos) {
                if (!lattice->getEndNodes(pos).empty()) {
                    // 当位置(pos)が終端となっている経路(接続可能な先行ノード)がある場合は、ここを開始点として辞書引き＆未知語解析
                    auto begin_nodes = tokenizer->lookup(*lattice, pos, mazePenalty, allowNonTerminal);    // begin_nodes: pos を起点とするNodeの集合

                    // pos位置を開始点とするNode集合を設定する
                    lattice->setBeginNodes(pos, begin_nodes);
                    // 各Nodeの終点位置集合への追加
                    for (const auto& node : begin_nodes) {
                        lattice->addEndNode(node);
                    }

                    // posを終点とする先行ノード群と接続させる
                    connect(lattice, begin_nodes, lattice->getEndNodes(pos), lattice->isNBest(), mazePenalty, mazeConnPenalty);
                }
            }

            // EOSノードを、最後尾ノードに接続させる
            // 文末が空白文字で終わっているような場合は、有効な最後尾ノードの endpos は文末位置より前にある
            //auto pos = lattice->end_nodes.lastIndexWhere{ _.nonEmpty };
            connect(lattice, { eos_node }, lattice->getLastNonEmptyEndNodes(), lattice->isNBest(), mazePenalty, mazeConnPenalty);

            // 両番兵に変化がないか確認 (TODO: 後で削除する)
            //assert(lattice->getEndNodes(0).size() == 1 && lattice->bosNode() == bos_node);
            assert(lattice->getLastBeginNodes().size() == 1 && lattice->eosNode() == eos_node);
            LOG_INFO(L"LEAVE");
        }

        // 末尾Nodeからたどって、最良コストのPathを next で連結する
        void linkBestPath(LatticePtr lattice) {
            LOG_INFO(L"ENTER");
            auto node = lattice->eosNode();
            while (node->prev()) {
                node->setBest(true);
                node->setAccumCost2(node->accumCost());
                auto prev_node = node->prev();
                prev_node->setNext(node);
                node = prev_node;
            }
            LOG_INFO(L"LEAVE");
        }

        bool isMazeNode(NodePtr node) {
            auto features = utils::split(node->feature(), TAB);
            if (features.size() > FEATURE_OFF) {
                auto feats = utils::parseCSV(features[FEATURE_OFF]);
                return (feats.size() > MAZE_OFF && feats[MAZE_OFF] == L"MAZE");
            }
            return false;
        }

        /**
         * ある文字位置(pos)におけるノードの連結 (コスト計算)<br>
         * rightNodes: pos を開始位置とするノードの集合
         * leftNodes:  pos を終了位置とするノードの集合
         * 最良解については、EOSからBOS方向への逆順のリンクとなる。
         * Nbest解については、Path によって leftNodes と rightNodes の全組み合わせを記録しておく。
         */
        template<template<typename...> typename C, typename A>
        void connect(LatticePtr lattice, const Vector<NodePtr>& rightNodes, const C<NodePtr, A>& leftNodes, bool isNbest, int mazePenalty, int mazeConnPenalty) {
            LOG_DEBUGH(L"ENTER: isNbest={}, mazePenalty={}, mazeConnPenalty={}", isNbest, mazePenalty, mazeConnPenalty);
            for (const auto& rnode : rightNodes) {
                int best_cost = INT_MAX;
                NodePtr best_lnode = nullptr;
                // rnodeに接続する lnodeのうち、最良コストのものを選出する
                for (const auto& lnode : leftNodes) {
                    int connCost = connector->cost(*lnode, *rnode);  // connCost: connection cost
                    if (mazePenalty < 0) {
                        // TODO: ここは時間がかかるので、何か工夫が必要
                        // 交ぜ書きの連接は劣後
                        if (isMazeNode(lnode) && isMazeNode(rnode)) {
                            connCost += mazeConnPenalty;
                        }
                    }
                    int cost = lnode->accumCost() + connCost + rnode->wcost();

                    if (cost < best_cost) {
                        best_lnode = lnode;
                        best_cost = cost;
                    }

                    if (isNbest) {
                        // NBest の場合は、 Path オブジェクトによってノード間の接続関係を記録しておき、あとでたどれるようにしておく
                        // lnode.rpath -> path; path.rnode -> rnode
                        // lnode <- path.lnode; path <- rnode.lpath
                        auto path = lattice->newPath();
                        path->cost(connCost);
                        path->setRnode(rnode);
                        path->setLnode(lnode);
                        path->setLnext(rnode->lpath());
                        rnode->setLpath(path);
                        path->setRnext(lnode->rpath());
                        lnode->setRpath(path);
                        LOG_DEBUGH(L"accum={}: path: {}", cost, path->debugString());
                    }
                }

                if (!best_lnode) {
                    THROW_RTE(L"{}: no left node found or cost calculation failed.", rnode->surface()->caseForm());
                }

                // 最良コストのlnodeへのリンクを張る(prev のみを使用; next は用いない)
                rnode->setPrev(best_lnode);
                //rnode->next.reset();
                rnode->setNext(nullptr);
                rnode->setAccumCost(best_cost);
            }

#if _LOG_DEBUGH_FLAG
            if (Reporting::Logger::IsInfoHEnabled()) showConnectionResult(rightNodes);
#endif
            LOG_DEBUGH(L"LEAVE");
        }

#if 0
        static void calc_alpha(Node& n, double beta) {
            n.alpha(0.0);
            auto path = n.lpath();
            while (path) {
                n.alpha(util::logsumexp(
                    n.alpha(),
                    -beta * path->cost + path->lnode->alpha(),
                    path == n.lpath()));
                path = path->lnext;
            }
        }

        static void calc_beta(Node& n, double beta) {
            n.beta(0.0);
            auto path = n.rpath();
            while (path) {
                n.beta(util::logsumexp(
                    n.beta(),
                    -beta * path->cost + path->rnode->beta(),
                    path == n.rpath()));
                path = path->rnext;
            }
        }
#endif
    };
    DEFINE_CLASS_LOGGER(Viterbi::Impl);

    //---------------------------------------------------------------------------
    // コンストラクタ
    Viterbi::Viterbi(OptHandlerPtr opts){
        LOG_INFOH(L"ENTER: ctor");
        pImpl.reset(new Impl(opts));
        LOG_INFOH(L"LEAVE: ctor");
    }

    // デストラクタ
    Viterbi::~Viterbi() {
    }

    DEFINE_CLASS_LOGGER(Viterbi);

    /**
     * 形態素解析処理
     */
    void Viterbi::analyze(LatticePtr lattice, int mazePenalty, int mazeConnPenalty, bool allowNonTerminal) {
        LOG_INFO(L"ENTER");
        CHECK_OR_THROW(lattice && lattice->sentence,
            L"Viterbi.analyze: lattice must not be null and have non-null sentence");

        // viterbi 処理 (解析部本体)
        pImpl->viterbi(lattice, mazePenalty, mazeConnPenalty, allowNonTerminal);
        // 最良コストのPathを next で連結する
        pImpl->linkBestPath(lattice);
        LOG_INFO(L"LEAVE");
    }

    /** ユーザー辞書の再ロード */
    void Viterbi::reload_userdics() {
        LOG_INFOH(L"ENTER");
        pImpl->tokenizer->reload_userdics();
        LOG_INFOH(L"LEAVE");
    }

} // namespace analyzer
