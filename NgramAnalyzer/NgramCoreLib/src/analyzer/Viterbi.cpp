#include "exception.h"
#include "my_utils.h"

#include "Connector.h"
#include "Tokenizer.h"
#include "Viterbi.h"
#include "RealtimeDict.h"

#include "util/xsv_parser.h"
#include "featureDef.h"

#include "NgramDebugLog.h"

namespace analyzer {
    DECLARE_LOGGER;

    extern int UNKNOWN_OTHER_COST;
    int GLUE_BONUS_FACTOR = 500;
    int GLUE_BONUS_MIN_LEN = 4;
    static const int kShortHiraganaPenalty = 2000;
    static const int kMorphBoundaryMismatchPenalty = 5000;
    static const int kMissingLowOrderQuadgramBasePenalty = 2000;
    static const int kLowOrderQuadgramLength = 4;
    static const int MaxAccumBonus = 10000;

    static const int MorphPenaltyEntryFound = 7000;

    static const double kDefaultTheta = 0.75;

#if _LOG_DEBUGH_FLAG
    void showConnectionResult(const Vector<NodePtr>& nodes) {
        StringStream msg;
        msg << L"\n.\n--------------------------------------------------\n";
        msg << L"Connection results:\n";
        for (const auto& node : nodes) {
            msg << node->accumCost() << L" | ";
            auto prev = node->prev();
            msg << (prev ? (prev->isBOS() ? L"BOS" : prev->surface()->toString()) : L"NONE") << L" | ";
            msg << (node->accumCost() - (prev ? prev->accumCost(): 0) - node->wcost()) << L" | ";
            msg<< node->toVerbose() << L"\n";
        };
        msg<< L"--------------------------------------------------\n";
        LOG_INFOH(msg.str());
    }
#endif

    class Viterbi::Impl {
        DECLARE_CLASS_LOGGER;
        friend class Viterbi;

    private:
        OptHandlerPtr opts;

        UniqPtr<Tokenizer> tokenizer;

        int cost_factor_;
        Map<String, bool> exactMatchCache;

    public:
        Impl(OptHandlerPtr opts) :
            opts(opts),
            tokenizer(MakeUniq<Tokenizer>(opts)),
            cost_factor_(opts->getInt(L"cost-factor", 800))
        {
            LOG_INFOH(L"ENTER: cost_factor_={}", cost_factor_);
            CHECK_OR_THROW(!tokenizer->getDictionaries().empty(), L"Dictionary is empty");
            LOG_INFOH(L"LEAVE");
        }

    private:
        //---------------------------------------------------------------------------
        // private
        bool isLowOrderNgramNode(const NodePtr& node) const {
            if (!node || node->isBOS() || node->isEOS()) return false;
            int len = node->length();
            return len == 1 || len == 2;
        }

        bool isMissingQuadgramPenaltyTarget(StringRef key) const {
            //bool hasNonHiragana = false;
            for (wchar_t ch : key) {
                if (ch == GETA_CHAR || utils::is_space(ch) || utils::is_katakana(ch)) {
                    return false;
                }
                //if (!utils::is_hiragana(ch)) {
                //    hasNonHiragana = true;
                //}
            }
            //return hasNonHiragana;
            return true;
        }

        bool findExactMatchCached(StringRef key) {
            auto it = exactMatchCache.find(key);
            if (it != exactMatchCache.end()) {
                return it->second;
            }
            bool found = tokenizer->findExactMatch(key);
            exactMatchCache[key] = found;
            return found;
        }

        int calcMissingLowOrderQuadgramPenalty(const NodePtr& lnode, const NodePtr& rnode, String& key) {
            key.clear();
            if (!isLowOrderNgramNode(rnode)) return 0;

            Vector<NodePtr> nodes;
            int totalLen = 0;
            int totalBonus = 0;
            NodePtr node = rnode;
            while (node && totalLen < kLowOrderQuadgramLength) {
                if (!isLowOrderNgramNode(node)) return 0;

                nodes.push_back(node);
                totalLen += node->length();
                totalBonus += node->bonus();

                if (totalLen > kLowOrderQuadgramLength) return 0;
                node = (node == rnode) ? lnode : node->prev();
            }

            if (totalLen != kLowOrderQuadgramLength) return 0;

            for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
                key.append((*it)->surface()->toString());
            }

            if (!isMissingQuadgramPenaltyTarget(key) || findExactMatchCached(key)) return 0;

            int bonusPenalty = std::max(0, totalBonus);
            return bonusPenalty + kMissingLowOrderQuadgramBasePenalty;
        }

        int calcMissingHeadGetaKanjiNgramPenalty(const NodePtr& lnode, const NodePtr& rnode, String& key) {
            key.clear();
            if (!lnode || !rnode || rnode->isBOS() || rnode->isEOS()) return 0;
            if (rnode->length() != 1 && rnode->length() != 2) return 0;
            if (lnode->length() != 2) return 0;

            NodePtr prev = lnode->prev();
            if (!prev || !prev->isBOS()) return 0;

            auto lsurface = lnode->surface();
            if (!lsurface) return 0;

            wchar_t head = lsurface->charAt(lsurface->begin());
            wchar_t kanji = lsurface->charAt(lsurface->begin() + 1);
            if (head != GETA_CHAR || !utils::is_kanji(kanji)) return 0;

            key.push_back(kanji);
            key.append(rnode->surface()->toString());
            if (!isMissingQuadgramPenaltyTarget(key) || findExactMatchCached(key)) return 0;

            int bonusPenalty = std::max(0, lnode->bonus() + rnode->bonus());
            return bonusPenalty + kMissingLowOrderQuadgramBasePenalty;
        }

        const Vector<bool>* createMorphBoundaries(StringRef sentence, StringRef morphEntries, Vector<bool>& morphBoundaries) {
            if (morphEntries.empty()) {
                return nullptr;
            }

            String joined;
            Vector<size_t> boundaryPositions;
            size_t pos = 0;
            for (const auto& morph : utils::split(morphEntries, L'|')) {
                if (!morph.empty()) {
                    joined.append(morph);
                    bool found = false;
                    if (std::any_of(morph.begin(), morph.end(), [](wchar_t ch) {return !utils::is_hiragana(ch);})) {
                        // ひらがな以外があったら、どの位置でも分割可能とする
                        for (size_t i = 0; i < morph.size() - 1; ++i) {
                            boundaryPositions.push_back(pos + i + 1);
                        }
                    }
                    pos += morph.length();
                    boundaryPositions.push_back(pos);
                }
            }

            if (joined != sentence) {
                LOG_WARN(L"morphEntries ignored: joined morphEntries does not match sentence. sentence={}, morphEntries={}", sentence, morphEntries);
                return nullptr;
            }

            morphBoundaries.assign(sentence.length() + 1, false);
            for (size_t boundaryPos : boundaryPositions) {
                if (boundaryPos > 0 && boundaryPos < sentence.length()) {
                    morphBoundaries[boundaryPos] = true;
                }
            }
            return &morphBoundaries;
        }

        /**
         * viterbi 処理 --
         * 単語の辞書引きと先行ノードとの接続処理を行って、ラティス構造を構築する。
         * @param morphEntries 形態素解析によって分かち書きされた形態素列 ("|" 区切り)
         * @param tempDictEntries 一時的なユーザー辞書エントリ ("|" 区切り)
         */
        void viterbi(LatticePtr lattice, StringRef morphEntries, StringRef tempDictEntries) {
            auto sentence = lattice->sentence;
            auto len = sentence->length();
            auto begin = sentence->begin();
            auto end = sentence->end();
            exactMatchCache.clear();

            LOG_INFOH(L"ENTER: lattice->sentence={}, morphEntries={}, tempDictEntries={}", sentence->toString(), morphEntries, tempDictEntries);

            // 処理前の番兵ノード
            auto bos_node = lattice->bosNode();
            auto eos_node = lattice->eosNode();

            // 各位置における、lookupされたNgram群の最大長
            // Ngramの接続位置で、それをカバーするようなNgramがあればその長さを GLUE ボーナスの計算に使用するために記録しておく
            Vector<int>  glueNgramMaxLens(len + 1, 0);

            Vector<bool> morphBoundaries;
            const Vector<bool>* pMorphBoundaries = createMorphBoundaries(sentence->toString(), morphEntries, morphBoundaries);

            // TemporaryDict をリセットする (ユーザー辞書のエントリを一時的に追加するためなどに使用)
            tokenizer->resetTempDict(tempDictEntries);

            // 文の先頭から末尾に向かって、形態素ノードを作成し、先行ノードと接続させてラティスを作っていく
            for (size_t pos = 0; pos < len; ++pos) {
                if (!lattice->getEndNodes(pos).empty()) {
                    // 当位置(pos)が終端となっている経路(接続可能な先行ノード)がある場合は、ここを開始点として辞書引き＆未知語解析
                    auto begin_nodes = tokenizer->lookup(*lattice, pos);    // begin_nodes: pos を起点とするNodeの集合

                    // pos位置を開始点とするNode集合を設定する
                    lattice->setBeginNodes(pos, begin_nodes);
                    // 各Nodeの終点位置集合への追加
                    for (const auto& node : begin_nodes) {
                        lattice->addEndNode(node);
                    }

                    // posを終点とする先行ノード群と接続させる
                    connect(lattice, pos, glueNgramMaxLens, pMorphBoundaries, begin_nodes, lattice->getEndNodes(pos), lattice->isNBest());
                }
            }

            // EOSノードを、最後尾ノードに接続させる
            // 文末が空白文字で終わっているような場合は、有効な最後尾ノードの endpos は文末位置より前にある
            //auto pos = lattice->end_nodes.lastIndexWhere{ _.nonEmpty };
            connect(lattice, len, glueNgramMaxLens, pMorphBoundaries, { eos_node }, lattice->getLastNonEmptyEndNodes(), lattice->isNBest());

            if (IS_LOG_WARN_ENABLED) {
                // 両番兵に変化がないか確認
                LOG_INFO(L"CHECK BOTH SENTINELS STATUS");
                assert(lattice->getEndNodes(0).size() == 1 && lattice->bosNode() == bos_node);
                assert(lattice->getLastBeginNodes().size() == 1 && lattice->eosNode() == eos_node);
            }
            LOG_INFOH(L"LEAVE: lattice->sentence={}", sentence->toString());
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

        /**
         * ある文字位置(pos)におけるノードの連結 (コスト計算)<br>
         * rightNodes: pos を開始位置とするノードの集合
         * leftNodes:  pos を終了位置とするノードの集合
         * 最良解については、EOSからBOS方向への逆順のリンクとなる。
         * Nbest解については、Path によって leftNodes と rightNodes の全組み合わせを記録しておく。
         */
        template<template<typename...> typename C, typename A>
        void connect(LatticePtr lattice, size_t pos, Vector<int>& glueNgramMaxLens, const Vector<bool>* morphBoundaries, const Vector<NodePtr>& rightNodes, const C<NodePtr, A>& leftNodes, bool isNbest) {
            LOG_DEBUGH(L"ENTER: isNbest={}", isNbest);
            int rnodeMaxLen = 0;
            for (const NodePtr rnode : rightNodes) {
                LOG_DEBUGH(L"  rnode: BEGIN={}", rnode->toVerbose());
                int best_cost = INT_MAX;
                int best_accumBonus = 0;
                NodePtr best_node = nullptr;
                // rnodeに接続する lnodeのうち、最良コストのものを選出する
                for (const NodePtr lnode : leftNodes) {
                    int connCost = 0; //connector->cost(*lnode, *rnode);  // connCost: connection cost
                    LOG_DEBUGH(L"    lnode={}, isShortHira={}", lnode->toVerbose() ,lnode->isShortHiragana());
                    if (lnode->isShortHiragana()) {
                        // 直前ノードが短いひらがな語で、その前または後にもひらがなが接続する場合はペナルティを加算する
                        NodePtr llnode = lnode->prev();
                        LOG_DEBUG(L"      rnode->isHeadHiragana={}", rnode->isHeadHiragana());
                        LOG_DEBUG(L"      llnode->isTailHiragana={}", llnode && llnode->isTailHiragana());
                        if (rnode->isHeadHiragana() || (llnode && llnode->isTailHiragana())) {
                            connCost += kShortHiraganaPenalty;
                            LOG_DEBUG(L"      short Hiragana({}) penalty: connCost={}", lnode->surface()->toString(), connCost);
                        }
                    } else if (lnode->isGeta()) {
                        // 先頭の 〓 に長めのNgramが後接する場合は、〓 の未知語コストを一部相殺する
                        NodePtr llnode = lnode->prev();
                        int rlen = rnode->length();
                        if (llnode && llnode->isBOS() && rlen >= 3) {
                            int getaBonus = rlen >= 4 ? lnode->wcost() * 3 / 4 : lnode->wcost() / 2;
                            connCost -= getaBonus;
                            LOG_DEBUG(L"      leading GETA ngram bonus: rlen={}, bonus={}, connCost={}", rlen, getaBonus, connCost);
                        } else if (rnode->isKanji()) {
                            // 直前ノードが 〓 で、rnodeが漢字語の場合はペナルティを軽減する
                            if (rnode->length() >= 4) {
                                connCost -= UNKNOWN_OTHER_COST;
                            } else if (rnode->length() == 3) {
                                connCost -= UNKNOWN_OTHER_COST / 2;
                            }
                            LOG_DEBUG(L"      lnode: GETA, rnode: KANJI, connCost={}", connCost);
                        }
                    }

                    if (morphBoundaries && pos > 0 && pos + 1 < morphBoundaries->size() && !(*morphBoundaries)[pos]) {
                        connCost += kMorphBoundaryMismatchPenalty;
                        LOG_DEBUG(L"      morph boundary mismatch: pos={}, penalty={}, connCost={}", pos, kMorphBoundaryMismatchPenalty, connCost);
                    }

                    String missingQuadgramKey;
                    int missingQuadgramPenalty = calcMissingLowOrderQuadgramPenalty(lnode, rnode, missingQuadgramKey);
                    if (missingQuadgramPenalty > 0) {
                        connCost += missingQuadgramPenalty;
                        LOG_DEBUG(L"      missing low-order quadgram penalty: key={}, penalty={}, connCost={}", missingQuadgramKey, missingQuadgramPenalty, connCost);
                    }

                    String missingHeadGetaKanjiNgramKey;
                    int missingHeadGetaKanjiNgramPenalty = calcMissingHeadGetaKanjiNgramPenalty(lnode, rnode, missingHeadGetaKanjiNgramKey);
                    if (missingHeadGetaKanjiNgramPenalty > 0) {
                        connCost += missingHeadGetaKanjiNgramPenalty;
                        LOG_DEBUG(L"      missing head-geta kanji ngram penalty: key={}, penalty={}, connCost={}", missingHeadGetaKanjiNgramKey, missingHeadGetaKanjiNgramPenalty, connCost);
                    }

                    // GLUE ボーナスの適用
                    size_t chkPos = (int)pos <= lnode->length() ? 0 : pos - lnode->length();
                    int maxGlueLen = 0;
                    while (chkPos < pos) {
                        int gnl = glueNgramMaxLens[chkPos];
                        if (gnl > (int)(pos - chkPos) && gnl > maxGlueLen) maxGlueLen = gnl;
                        ++chkPos;
                    }
                    int glueBonus = 0;
                    if (maxGlueLen >= GLUE_BONUS_MIN_LEN) {
                        glueBonus = (maxGlueLen - GLUE_BONUS_MIN_LEN + 1) * GLUE_BONUS_FACTOR;
                        LOG_DEBUG(L"      glueBonus={} ((maxGlueLen({}) - GLUE_BONUS_MIN_LEN({}) + 1) * GLUE_BONUS_FACTOR({}))", glueBonus, maxGlueLen, GLUE_BONUS_MIN_LEN, GLUE_BONUS_FACTOR);
                    }

                    int bonus = rnode->bonus();
                    int accumBonus = lnode->accumBonus() + glueBonus;
                    if (accumBonus >= MaxAccumBonus) {
                        LOG_DEBUG(L"      accumBonus + gluBonus capped: orig={}, capped={}", accumBonus, MaxAccumBonus);
                        accumBonus = MaxAccumBonus;
                        glueBonus = accumBonus - lnode->accumBonus();  // lnodeの累積ボーナスと合わせて上限に達する分だけglueBonusを適用する
                        bonus = 0;      // 累積ボーナスが上限に達している場合は、rnodeのボーナスも適用しない
                        LOG_DEBUG(L"      bonus adjusted-A: accumBonus={}, gluBonus={}, bonus={}", accumBonus, glueBonus, bonus);
                    } else {
                        accumBonus += bonus;
                        if (accumBonus >= MaxAccumBonus) {
                            LOG_DEBUG(L"      accumBonus + bonus capped: orig={}, capped={}", accumBonus, MaxAccumBonus);
                            accumBonus = MaxAccumBonus;
                            bonus = accumBonus - lnode->accumBonus() - glueBonus;
                            LOG_DEBUG(L"      bonus adjusted-B: accumBonus={}, gluBonus={}, bonus={}", accumBonus, glueBonus, bonus);
                        }
                    }
                    connCost -= glueBonus;
                    LOG_DEBUG(L"      connCost={}, glueBonus={}, bonus={}, accumBonus={}", connCost, glueBonus, bonus, accumBonus);

                    int cost = lnode->accumCost() + connCost + rnode->wcost() - bonus;

                    if (cost < best_cost) {
                        best_node = lnode;
                        best_cost = cost;
                        best_accumBonus = accumBonus;
                    }

                    LOG_DEBUGH(L"      cost={}, best_cost={}, best_accumBonus={}", cost, best_cost, best_accumBonus);

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
                        LOG_DEBUGH(L"      path: {}", path->debugString());
                    }
                } // for lnode

                if (!best_node) {
                    THROW_RTE(L"{}: no left node found or cost calculation failed.", rnode->surface()->caseForm());
                }

                // 最良コストのlnodeへのリンクを張る(prev のみを使用; next は用いない)
                rnode->setPrev(best_node);
                //rnode->next.reset();
                rnode->setNext(nullptr);
                rnode->setAccumCost(best_cost);
                rnode->setAccumBonus(best_accumBonus);

                if (rnode->length() > rnodeMaxLen) {
                    rnodeMaxLen = rnode->length();
                }
                LOG_DEBUGH(L"  rnode: END={}", rnode->toVerbose());
            } // for rnode
            glueNgramMaxLens[pos] = rnodeMaxLen;

#if _LOG_DEBUGH_FLAG
            if (Logger::IsInfoHEnabled()) showConnectionResult(rightNodes);
#endif
            LOG_DEBUGH(L"LEAVE");
        }

        // penaltyEntries によるペナルティの取得
        // penaltyEntries ペナルティを与えるべき形態素列 ("|" 区切り)
        int getMorphPenalty(StringRef penaltyEntries) {
            LOG_DEBUGH(L"ENTER: penaltyEntries={}", penaltyEntries);
            int morphPenalty = 0;
            if (!penaltyEntries.empty() && !tokenizer->findExactMatch(penaltyEntries)) {
                morphPenalty = MorphPenaltyEntryFound;
                LOG_INFOH(L"ADD morphPenalty");
            }
            LOG_DEBUGH(L"LEAVE: morphPenalty={}", morphPenalty);
            return morphPenalty;
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
     * N-Best 解析の実行。N-Bestな解析結果が一つずつ格納されたArrayを返す。
     * @param sentence 解析対象文
     * @param morphEntries 形態素解析による分割結果
     * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
     * @param penaltyEntries ペナルティを与えるべき形態素列 ("|" 区切り)
     * @param results N-Bestな解析結果を格納するvector
     * @param nBest N-Bestの最大数
     * @param mazePenalty 交ぜ書き候補に与えるペナルティ。これが負値ならボーナスなり、他の交ぜ書き候補を含めた解を返す
     * @return 最良解析結果のコスト
     */
    int Viterbi::parseNBest(StringRef sentence, StringRef morphEntries, StringRef tempEntries, StringRef penaltyEntries, Vector<String>& results, size_t nBest, bool needResults) {
        //warnings.clear();
        LOG_INFO(L"ENTER: sentence={}, penaltyEntries={}, nBest={}", sentence, penaltyEntries, nBest);

        auto lattice = Lattice::CreateLattice(sentence, L"", nBest);
        analyze(lattice, morphEntries, tempEntries);
        int baseCost = lattice->getSolutions(results, needResults);
        int morphPenalty = pImpl->getMorphPenalty(penaltyEntries);
        int userNgramPenalty = RealtimeDict::getUserNgramPenalty(sentence);
        int totalCost = baseCost + morphPenalty + userNgramPenalty;
        LOG_INFOH(L"LEAVE: {}: baseCost={}, morphPenalty={}, userNgramPenalty={}", totalCost, baseCost, morphPenalty, userNgramPenalty);
        return totalCost;
    }

    /**
     * 形態素解析処理
     * @param morphEntries 形態素解析による分割結果
     * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
     */
    void Viterbi::analyze(LatticePtr lattice, StringRef morphEntries, StringRef tempDictEntries) {
        LOG_INFO(L"ENTER");
        CHECK_OR_THROW(lattice && lattice->sentence,
            L"Viterbi.analyze: lattice must not be null and have non-null sentence");

        // viterbi 処理 (解析部本体)
        pImpl->viterbi(lattice, morphEntries, tempDictEntries);
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
