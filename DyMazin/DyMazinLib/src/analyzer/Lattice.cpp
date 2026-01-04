#include "Lattice.h"
#include "TextWriter.h"
#include "util/string_utils.h"

namespace {
    int getaCost(OptHandlerPtr opts) {
        return opts->getInt(L"geta-cost", -1);
    }
}

namespace analyzer {
    DEFINE_CLASS_LOGGER(Lattice);

    // Constructor
    Lattice::Lattice(
        RangeStringPtr sentence,
        StringRef sentinelFeature,
        SharedPtr<TextWriter> writer,
        size_t nBest,
        int getaCost)
        : sentence(sentence), sentinelFeature(sentinelFeature), writer(writer), nBest(nBest)
    {
        LOG_INFO(L"CALLED: ctor");
        //CHECK_OR_THROW(sentence->length() > 0, L"Lattice: sentence must not be null");
        CHECK_OR_THROW(nBest >= 0 && nBest < NBEST_MAX, L"Lattice: nBest must be >= 0 and <= {}", NBEST_MAX);

        initialize(getaCost);
    }

     // Constructor
    Lattice::Lattice(StringRef sentence, StringRef bosEosFeat, SharedPtr<TextWriter> writer, size_t nBest, int getaCost)
        : Lattice(RangeString::Create(sentence), bosEosFeat, writer, nBest, getaCost)
    {
    }

    // Destructor
    Lattice::~Lattice() {
        LOG_INFO(L"CALLED: dtor");
    }

    // private
    // Builder
    LatticePtr Lattice::CreateLattice(OptHandlerPtr opts, RangeStringPtr sentence, StringRef sentinelFeature, SharedPtr<TextWriter> writer, size_t nBest) {
        return LatticePtr(new Lattice(sentence, sentinelFeature, writer, nBest, getaCost(opts)));
    }

    LatticePtr Lattice::CreateLattice(OptHandlerPtr opts, StringRef sentence, StringRef sentinelFeature, SharedPtr<TextWriter> writer, size_t nBest) {
        return LatticePtr(new Lattice(sentence, sentinelFeature, writer, nBest, getaCost(opts)));
    }

    void Lattice::initialize(int getaCost) {
        LOG_INFO(L"CALLED: initialize: getaCost={}", getaCost);

        bos_node = createSentinel(0, NodeType::BOS_NODE);

        // +1 は Dummy BOSノードの分
        end_nodes.resize(sentence->length() + 1);
        end_nodes[0].push_back(createDummyBos());
        if (getaCost >= 0) {
            LOG_INFO(L"ADDING GETA BOS NODE: getaCost={}", getaCost);
            end_nodes[0].push_back(createGetaBos(getaCost));
        }

        // +1 は EOS ノードの分
        begin_nodes.resize(sentence->length() + 1);
        begin_nodes[sentence->end()].push_back(createSentinel(sentence->end(), NodeType::EOS_NODE));

        Path::Reset();

        // lazy
        LAZY_INITIALIZE(nbestGenerator, NBestGenerator::Create(eosNode()));
    }

    NodePtr Lattice::createSentinel(size_t pos, NodeType status) {
        auto node = newNode(pos, pos);  // BOS_KEY is dummy surface string
        node->setFeature(sentinelFeature);
        //node->isbest = 1;
        node->setBest(true);
        node->setStat(status);
        return node;
    }

    NodePtr Lattice::createDummyBos() {
        auto node = newNode(0, 0);
        node->setFeature(sentinelBosFeature);
        node->setRcAttr(0);
        node->setWcost(0);
        node->setAccumCost(0);
        node->setAccumCost2(0);
        //node->isbest = 1;
        node->setBest(true);
        node->setStat(NodeType::DUMMY_BOS_NODE);
        return node;
    }

    NodePtr Lattice::createGetaBos(int getaCost) {
        auto node = newNode(0, 0);
        node->setFeature(sentinelGetaFeature);
        node->setRcAttr(sentinelGetaRcAttr);
        node->setWcost(getaCost);
        node->setAccumCost(0);
        node->setAccumCost2(0);
        //node->isbest = 1;
        node->setBest(false);
        node->setStat(NodeType::DUMMY_BOS_NODE);
        return node;
    }

    // public
    /**
     * 解を取得する
     * @param results N個の解を格納したArrayを返す
     * @return 最良コスト
     */
    int Lattice::getSolutions(Vector<String>& results, bool otherMazeCand) const {
        LOG_INFO(L"ENTER: nBest={}, otherMazeCand={}, writer={}", nBest, otherMazeCand, writer != nullptr);
        //warnings.clear();
        if (nBest == 1) {
            if (otherMazeCand && writer) {
                results.push_back(toMazeString());
            } else {
                results.push_back(toString());
            }
        } else {
            enumNBestAsString(results);
        }
        int cost = bestCost();
        LOG_INFO(L"LEAVE: bestCost={}", cost);
        return cost;
    }

    /**
    * N-Best解の取得
    * @param N N-Bestの数（デフォルト=1）
    * @return N個の解を格納したArray
    */
    void Lattice::enumNBestAsString(Vector<String>& result) const {
        auto addSolution = [&]() {
            result.push_back(toString());
        };

        int minCost = INT_MAX;
        int nbestMax = (nBest == 0) ? 4 : (int)nBest;
        LOG_INFO(L"ENTER: nbestMax={}, nBest={}", nbestMax, nBest);
        //if (!nbestGenerator) {
        //    nbestGenerator = NBestGenerator::Create(eosNode());
        //}
        for (int i = 0; i < nbestMax; ++i) {
            int cost = nbestGenerator()->next();
            if (cost >= MAX_COST) {
                break;
            }
            if (nBest == 0) {
                // 同じコストのものだけを表示
                if (cost <= minCost) {
                    minCost = cost;
                    addSolution();
                } else {
                    break;
                }
            } else {
                addSolution();
            }
        }
        LOG_INFO(L"LEAVE");

        // make a dummy node for EON (End-Of-NBest)
        //if (writer != null) {
        //    val eon_node = createEONnode()
        //        writer.writeNode(this, eon_node) match {
        //        case Some(str) = > result += str
        //            case None = > warnings += writer.whatLog.str // some error occurred
        //    }
        //}

    }

    /**
     * BOS/EOSを除いたノードリストを返す
     */
    Vector<Node*> Lattice::nodeList() const {
        Vector<Node*> result;
        Node* node = dummyBosNode()->next();     // BOSの次
        if (node && node->stat() == NodeType::DUMMY_BOS_NODE) {
            // Dummy BOSノードをスキップ
            node = node->next();
        }
        while (node && node->next()) {
            result.push_back(node);
            node = node->next();
        }
        return result;
    }

    /**
    * BOS/EOSを含むノードリストを返す
    */
    Vector<Node*> Lattice::nodeListWithSentinel() const {
        Vector<Node*> result;
        Node* node = dummyBosNode();
        while (node) {
            result.push_back(node);
            node = node->next();
        }
        return result;
    }

    /**
     * 同じ位置で同じ長さのノードを集める
     */
    Vector<Node*> Lattice::gatherSameLenNodes(size_t pos, int len, StringRef myHinshi) const {
        Vector<Node*> result;
        if (pos < begin_nodes.size()) {
            for (auto node : begin_nodes[pos]) {
                if (node->length() == len) {
                    result.push_back(node);
                }
            }
        }
        // コスト順にソート
        std::sort(result.begin(), result.end(), [myHinshi](const Node* a, const Node* b) {
            int acost = a->wcost() - (utils::startsWith(a->feature(), myHinshi) ? 10000 : 0);   // 品詞が同じなら優先
            int bcost = b->wcost() - (utils::startsWith(b->feature(), myHinshi) ? 10000 : 0);
            return acost < bcost;
            });

        return result;
    }

    /**
    * 解析結果を文字列で返す
    */
    String Lattice::toString() const {
        LOG_INFO(L"CALLED");
        if (writer) {
            return writer->write(*this);
        } else {
            return writeLattice();
        }
    }

    String Lattice::toMazeString() const {
        LOG_INFO(L"CALLED");
        if (writer) {
            return writer->writeMaze(*this);
        } else {
            return writeLattice();
        }
    }

    String Lattice::writeLattice() const {
        LOG_INFO(L"CALLED");
        String result;
        auto node = dummyBosNode()->next();
        while (node->next()) {  // node.next == null なのは EOS
            result.append(node->surface()->toString());
            result.append(1, '\t').append(node->feature());
            if (result.back() != L'\n') result.append(1, '\n');
            node = node->next();
        }
        result.append(L"EOS");
        return result;
    }

    String Lattice::writeLattice(const Lattice& lattice) {
        LOG_INFO(L"CALLED");
        return lattice.toString();
      }

    /**
     * 最良解のコストを取得する
    */
    int Lattice::bestCost() const {
        const Node* p = eosNode();
        if (!p) return INT_MAX;
        // EOSの前のノード
        //p = p->prev();
        //if (!p || p->isBOS()) return INT_MAX;

        int accumCost = p->accumCost();
        int conCost = 0;
        //Node* prev = p->prev.get();
        //while (prev && !prev->isBOS()) {
        //    if (prev->length() > 0) {
        //        if (utils::is_hiragana(prev->surface->charAt(0))) {
        //            // ひらがなの接続コストが正値なら、それを10倍して加算する⇒接続しにくいひらがなにはペナルティを与える
        //            int cc = p->accumCost2 - p->wcost - prev->accumCost2;
        //            if (cc > 0) conCost += cc * 10;
        //        }
        //    }
        //    p = prev;
        //    prev = p->prev.get();
        //}
        int bestCost = accumCost + conCost;
        LOG_INFO(L"bestCost={}", bestCost);
        return bestCost;
    }

} // namespace analyzer