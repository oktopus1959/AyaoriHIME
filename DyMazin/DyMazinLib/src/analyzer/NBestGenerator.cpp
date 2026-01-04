#include "NBestGenerator.h"

#include "node/Path.h"
#include "constants/Constants.h"
#include "Lattice.h"

#if 1 || defined(_DEBUG)
#define _LOG_DEBUGH_FLAG true
#undef LOG_DEBUGH
#define LOG_DEBUGH LOG_INFOH
#else
#define _LOG_DEBUGH_FLAG false
#endif

namespace analyzer
{
    /**
     * EOSからBOSに向かってPathをたどりながら、A*アルゴリズムでN-best解を探索する。
     */
    class NBestGeneratorImpl : public NBestGenerator {
        DECLARE_CLASS_LOGGER;
    private:
        /**
         * A* algorithm によって次々に NBest解を探索するためのキュー要素。
         * あるノードを開始点とし、rElm によって、そこからEOSまでの経路を保持する。
         *
         * キューには fx() の小さい順に格納される。
         * hx(heuristic cost)は、順方向探索の際に計算された、BOSから注目Pathの左接ノードまでのコスト。
         * gxは、EOSから注目Pathまでたどる際に実際に計算して算出したコスト。
         */
        class QueueElement {
            int _id;
            NodePtr _rnode;
            int _lcost = 0;                  // link cost to rElm node
            SharedPtr<QueueElement> _rElm;   // 右隣接ノードを開始点とする要素。これをたどるとEOSに至る。
            int _hx = 0;                     // h(x) : heuristic cost
            int _gx = 0;                     // g(x) : 当nodeまでのコスト

        public:
            bool operator<(const QueueElement& other) const {
                return fx() < other.fx();
            }

            bool operator>(const QueueElement& other) const {
                return fx() > other.fx();
            }

            QueueElement(
                NodePtr np,
                int id = 0,
                int lc = 0,
                SharedPtr<QueueElement> nxt = 0,
                int h = 0,
                int g = 0)
                :
                _id(id),
                _rnode(np),
                _lcost(lc),
                _rElm(nxt),
                _hx(h),
                _gx(g)
            { }

            // 右隣接ノードを開始点とする要素を取得。これをたどるとEOSに至る。
            inline const SharedPtr<QueueElement>& rElement() const {
                return _rElm;
            }

            inline const NodePtr& rnode() const {
                return _rnode;
            }

            inline int linkCost() const {
                return _lcost;
            }

            // f(x) = h(x) + g(x): cost function for A* search
            inline int fx() const {
                return _hx + _gx;           
            }

            // h(x) : BOSから当nodeまでのheuristic cost
            inline int hx() const {
                return _hx;
            }

            // g(x) : 当nodeからEOSまでのコスト
            inline int gx() const {
                return _gx;
            }

            String debugString() const {
                return std::format(L"id={}, fx={}, hx={}, gx={}, rNode=<{}>, rrNode=<{}>", _id, fx(), _hx, _gx,
                    _rnode ? _rnode->toVerbose() : L"null",
                    _rElm && _rElm->rnode() ? _rElm->rnode()->toVerbose() : L"null");
            }
        };

        struct CompareElementPtr {
            bool operator()(const SharedPtr<QueueElement>& a, const SharedPtr<QueueElement>& b) const {
                return a->fx() > b->fx(); // 小さい方を優先する（ヒープ内では "大きい" 扱い）
            }
        };

        // fx()の小さい順に解候補を並べたキュー
        PriorityQueue<SharedPtr<QueueElement>, CompareElementPtr>  agenda;

    public:
        //NBestGeneratorImpl() {
        //}

        // EOSノードから開始する
        NBestGeneratorImpl(NodePtr eos_node) {
            agenda.push(MakeShared<QueueElement>(eos_node));
        }

        /**
         * 次のNBest解を取得する。
         * 解は、 BOSノードから  rElm メンバーによりリンクされたリストの形で得られる。
         */
        int next() override {
            LOG_DEBUGH(L"ENTER: agenda.size={}", agenda.size());
            while (!agenda.empty()) {
                auto top = agenda.top();
                agenda.pop();

                LOG_DEBUGH(L"top: {}", top->debugString());

                if (top->rnode()->isBOS()) {
                    // BOSからの next, prev リンクを書き換える
                    LOG_DEBUGH(L"top is BOS");
                    auto elm = top;
                    int acost = elm->linkCost();
                    while (elm->rElement()) {
                        const auto& nextnode = elm->rElement()->rnode();
                        elm->rnode()->setNext(nextnode);   // change next & prev
                        nextnode->setPrev(elm->rnode());
                        acost += nextnode->wcost();
                        nextnode->setAccumCost2(acost);
                        LOG_DEBUGH(L"nextNode: accumCost2={}: {}", nextnode->accumCost2(), nextnode->toVerbose());
                        elm = elm->rElement();
                        acost += elm->linkCost();
                    }
                    LOG_DEBUGH(L"LEAVE: found NBest: cost:fx={}", top->fx());
                    return top->fx();
                }

                auto rnode = top->rnode();
                auto path = rnode->lpath();
                LOG_DEBUGH(L"rnode->lpath={}", path ? path->debugString() : L"null");
                while (path) {
                    auto elm = MakeShared<QueueElement>(
                        path->lnode(),
                        path->getId(),
                        path->cost(),
                        top,
                        path->lnode()->accumCost(),
                        path->cost() + rnode->wcost() + top->gx()
                    );
                    LOG_DEBUGH(L"push: {}", elm->debugString());
                    agenda.push(elm);
                    path = path->lnext();
                    LOG_DEBUGH(L"path->lnext={}", path ? path->debugString() : L"null");
                }
            }

            LOG_DEBUGH(L"LEAVE: no more NBest: MAX_COST");
            return MAX_COST;
        }

    };
    DEFINE_CLASS_LOGGER(NBestGeneratorImpl);

    SharedPtr<NBestGenerator> NBestGenerator::Create(NodePtr eos_node) {
        return MakeShared<NBestGeneratorImpl>(eos_node);
    }
} // namespace analyzer