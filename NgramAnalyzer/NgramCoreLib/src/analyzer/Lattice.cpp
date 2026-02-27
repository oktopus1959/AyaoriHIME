#include "Lattice.h"
#include "util/string_utils.h"

namespace analyzer {
    DEFINE_NAMESPACE_LOGGER(analyzer);
    DEFINE_CLASS_LOGGER(Lattice);

    // Constructor
    Lattice::Lattice(
        RangeStringPtr sentence,
        StringRef sentinelFeature,
        size_t nBest)
        : sentence(sentence), sentinelFeature(sentinelFeature), nBest(nBest)
    {
        LOG_INFO(L"CALLED: ctor");
        //CHECK_OR_THROW(sentence->length() > 0, L"Lattice: sentence must not be null");
        CHECK_OR_THROW(nBest >= 0 && nBest < NBEST_MAX, L"Lattice: nBest must be >= 0 and <= {}", NBEST_MAX);

        initialize();
    }

     // Constructor
    Lattice::Lattice(StringRef sentence, StringRef bosEosFeat, size_t nBest)
        : Lattice(RangeString::Create(sentence), bosEosFeat, nBest)
    {
    }

    // Destructor
    Lattice::~Lattice() {
        LOG_INFO(L"CALLED: dtor");
    }

    // Builder
    LatticePtr Lattice::CreateLattice(RangeStringPtr sentence, StringRef sentinelFeature, size_t nBest) {
        return LatticePtr(new Lattice(sentence, sentinelFeature, nBest));
    }

    LatticePtr Lattice::CreateLattice(StringRef sentence, StringRef sentinelFeature, size_t nBest) {
        return LatticePtr(new Lattice(sentence, sentinelFeature, nBest));
    }

    void Lattice::initialize() {
        // +1 はBOSノードの分
        end_nodes.resize(sentence->length() + 1);
        end_nodes[0].push_back(createSentinel(0, NodeType::BOS_NODE));

        // +1 は EOS ノードの分
        begin_nodes.resize(sentence->length() + 1);
        begin_nodes[sentence->end()].push_back(createSentinel(sentence->end(), NodeType::EOS_NODE));

        Path::Reset();

        // lazy
        LAZY_INITIALIZE(nbestGenerator, NBestGenerator::Create(eosNode()));
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
     * 同じ位置で同じ長さのノードを集める
     */
    Vector<Node*> Lattice::gatherSameLenNodes(size_t pos, int len) const {
        Vector<Node*> result;
        if (pos < begin_nodes.size()) {
            for (auto node : begin_nodes[pos]) {
                if (node->length() == len) {
                    result.push_back(node);
                }
            }
        }
        return result;
    }

    /**
    * 解析結果を文字列で返す
    */
    String Lattice::toString() const {
        return writeLattice();
    }

    String Lattice::toMazeString() const {
        return writeLattice();
    }

    String Lattice::writeLattice() const {
        String result;
        auto node = bosNode()->next();
        while (node->next()) {  // node.next == null なのは EOS
            //result.append(node->surface()->toString());
            result.append(node->toVerbose());
            if (result.back() != L'\n') result.append(1, '\n');
            node = node->next();
        }
        result.append(L"EOS").append(node->toVerbose());
        return result;
    }

    String Lattice::writeLattice(const Lattice& lattice) {
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