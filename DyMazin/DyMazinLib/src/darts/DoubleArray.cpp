#include "string_type.h"
#include "misc_utils.h"
#include "Logger.h"

#include "constants/Constants.h"
#include "DoubleArray.h"
//#include "DoubleArrayTestHelper.h"

// 参考サイト : http://d.hatena.ne.jp/takeda25/20120219/1329634865

namespace darts
{
    //template<class T> struct Property {
    //    T& r;
    //    const T& v() { return r; }
    //    operator T() { return r; }
    //    void operator =(const T v) { r = v; }
    //};

//#define DUP_ENTRY_NOT_ALLOWED false

//#define ERROR_NO_ENTRY      -1
//#define ERROR_DUP_ENTRY     -2
//#define ERROR_NOT_SORTED    -3
//#define ERROR_NEG_VALUE     -4
//#define ERROR_UNKNOWN       -5

//#define ALIGN_SIZE          1024

    // ルート開始位置
//#define ROOT_INDEX          1

// 重複エントリ末尾
//#define LAST_ENTRY          INT_MIN

     /**
      * ダブル配列 (Double Array) クラス
      * baseAry: シフト量(次の深さのテーブルを指す)または検索返却値(単語終端の場合)
      * checkAry: 親INDEX (正値) または次の重複終端単語位置(負値; 最後は Int.MinValue)
      * dupAllowed: 重複エントリを許可するか (デフォルトは許可しない)
      */
    class DoubleArrayImpl : public DoubleArray {
        friend class DoubleArrayBuilder;
        friend class DoubleArrayBuildHelperImpl;

    private:
        std::vector<int> baseArray;
        std::vector<int> checkArray;

    //public:
        //std::vector<int>& baseArray() { return _baseAry; }
        //std::vector<int>& checkArray() { return _checkAry; }
        bool dupAllowed;

    public:
        void serialize(utils::OfstreamWriter& writer) override {
            writer.write(dupAllowed);
            writer.write(baseArray);
            writer.write(checkArray);
        }

        void deserialize(utils::IfstreamReader& reader) {
            reader.read(dupAllowed);
            reader.read(baseArray);
            reader.read(checkArray);
        }

    private:
        // 同じプレフィックス系列か
        bool is_prefix_matched(int idx, int pid) {
            return idx > ROOT_INDEX && idx < (int)size() && pid == checkArray[idx];
        }

    public:
        DoubleArrayImpl(bool dupAllowed = DUP_ENTRY_NOT_ALLOWED)
            : dupAllowed(dupAllowed) {
        }

        DoubleArrayImpl(
            const std::vector<int>& baseAry,
            const std::vector<int>& checkAry,
            bool dupAllowed = DUP_ENTRY_NOT_ALLOWED)
            : dupAllowed(dupAllowed)
        {
            assert(baseAry.size() == checkAry.size());
            std::copy(baseAry.begin(), baseAry.end(), std::back_inserter(baseArray));
            std::copy(checkAry.begin(), checkAry.end(), std::back_inserter(checkArray));
        }

        DoubleArrayImpl(
            const DoubleArrayImpl& da,
            bool dupAllowed = DUP_ENTRY_NOT_ALLOWED)
            : DoubleArrayImpl(da.baseArray, da.checkArray, dupAllowed)
        {
        }

        size_t size() override {
            return baseArray.size();
        }

        size_t nonzero_size() override {
            return std::count_if(checkArray.begin(), checkArray.end(), [](int x) { return x != 0; });
        }

        // resize : pos がarrayの範囲内になるようにバッファの大きさをリサイズする
        int resize(int pos) override {
            if (pos >= (int)size()) {
                size_t alignedSize = ((pos + ALIGN_SIZE) / ALIGN_SIZE) * ALIGN_SIZE;
                baseArray.resize(alignedSize);
                checkArray.resize(alignedSize);
                //      valueAry ++= new Array[Int](delta)
            }
            return (int)size();
        }

        // 完全一致検索
        ResultPair exactMatchSearch(String key) override {
            if (!key.empty()) {
                int pIdx = ROOT_INDEX;          // 親のインデックス
                int base = baseArray[pIdx];      // 開始位置

                bool bFound = true;
                for (auto ch : key) {
                    int idx = base + ch;
                    if (!is_prefix_matched(idx, pIdx)) {
                        // 親インデックスと一致しなかった
                        bFound = false;
                        break;
                    }
                    pIdx = idx;
                    base = baseArray[pIdx];
                }
                if (bFound && is_prefix_matched(base, pIdx)) {
                    // 単語終端だった
                    int pv = baseArray[base];
                    if (dupAllowed && pv < 0) {
                        // 重複エントリを許可する場合で負値なら、正値に反転してそのインデックスの位置にある値を返す
                        pv = baseArray[-pv];
                    }
                    return ResultPair(pv, (int)key.length());
                }
            }
            return ResultPair();
        }

        // 先頭部分一致検索
        std::vector<ResultPair> commonPrefixSearch(String key, bool allowNonTerminal) override {
            std::vector<ResultPair> results;
            int pIdx = ROOT_INDEX;
            int base = baseArray[pIdx];

            int i = 0;
            bool matched = true;
            while (matched && (size_t)i < key.size()) {
                wchar_t ch = key[i++]; //i += 1
                int idx = base + ch;
                if (!is_prefix_matched(idx, pIdx)) {
                    matched = false;
                } else {
                    // 親インデックスと一致
                    pIdx = idx;
                    base = baseArray[pIdx];
                    // 単語終端か
                    if (is_prefix_matched(base, pIdx)) {    // ここの base は base + '\0'(終端) と見なされる
                        int pv = baseArray[base];
                        if (pv >= 0) {
                            results.push_back(ResultPair(pv, i));
                        } else {
                            int pb = -pv;
                            int sh = 0;
                            while (sh != LAST_ENTRY) {
                                results.push_back(ResultPair(baseArray[pb], i));
                                sh = checkArray[pb];
                                pb = pb + sh;
                            }
                        }
                    } else if (allowNonTerminal) {
                        if (i > 1 && i == key.size()) {
                            // keyが 2文字以上で、末尾になったが続きがある⇒非終端トークン(pos=-1)として扱う
                            results.emplace_back(NON_TERMINAL_POS, i);
                            //results.push_back(ResultPair(-1, i));
                        }
                    }
                }
            }
            return results;
        }

    };

    // deserialize Double Array
    UniqPtr<DoubleArray> DoubleArray::deserialize(utils::IfstreamReader& reader) {
        auto ptr = MakeUniq<DoubleArrayImpl>();
        ptr->deserialize(reader);
        return ptr;
    }

    /**
     * Builder が使用するダブル配列クラス (mutable)
     */
    //class MutableDoubleArray : public DoubleArrayImpl<int> {
    //public:
    //    // resize : pos がarrayの範囲内になるようにバッファの大きさをリサイズする
    //    int resize(int pos) {
    //        return _resize(pos);
    //    }

    //    std::unique_ptr<DoubleArrayImpl> ToImmutable() {
    //        return std::make_unique<DoubleArrayImpl>(this->baseArray, this->checkArray, this->dupAllowed);
    //    }

    //};

    //-----------------------------------------------------------------------------------
    // dblArray を構築する際に補助的に使われるノード構造体
    struct TrieNode {
        wchar_t code;       // 文字コードが格納される
        short depth;        // ルートノードから深さ(キーの長さと比較して終端ノードかどうかの判定に用いる)
        int left;           // 最左キーのインデックス
        int right;          // 最右キーのインデックス

        int index = 0;      // ダブル配列中でのインデックス

        TrieNode() : code(0), depth(0), left(0), right(0), index(0) { }

        TrieNode(int c, int d, int l, int r, int x)
            : code(c), depth(d), left(l), right(r), index(x)
        { }

    };

    //-----------------------------------------------------------------------------------
    /**
     * DoubleArrayBuilder class
     */
    class DoubleArrayBuilder {
        DECLARE_CLASS_LOGGER;

        friend class DoubleArrayBuildHelperImpl;

        bool debug;

        std::shared_ptr<DoubleArrayImpl> dblArray;
        int  firstUnchecked = 0;
        std::vector<String> keyArray;
        std::vector<int> valArray;
        //size_t  progress;
        //ProgressFunc showProgress;

        // for debug
        TrieNode curNode;
        String  curLeft;
        String  curRight;

        int error = 0;
        int errorIndex = 0;

    public:
        DoubleArrayBuilder(bool dbg = false)
            : debug(dbg) {
        }

        std::shared_ptr<DoubleArrayImpl> array() {
            return dblArray;
        }

        size_t size() {
            return dblArray->size();
        }

        size_t nonzero_size() {
            return dblArray->nonzero_size();
        }

        //-----------------------------------------------------------------------------------
        /**
         * build Double Array
         */
        std::shared_ptr<DoubleArray> build(
            const std::vector<String>& keys,
            const std::vector<int>& values)
        {
            if (keys.empty()) {
                error = ERROR_NO_ENTRY;
                LOG_WARN(L"dblArray NO Entry");
            } else {
                error = 0;

                dblArray.reset(new DoubleArrayImpl());

                keyArray = keys;
                valArray = values;
                //progress = 0;
                //showProgress = progress_func;

                // 使用できない部分
                dblArray->resize(ROOT_INDEX + 1);
                for (int i = 0; i <= ROOT_INDEX; ++i) { dblArray->checkArray[i] = -1; }
                firstUnchecked = ROOT_INDEX + 1;

                // dblArray の構築
                // 最初はキー全体を覆う root_node から始める。
                // キー文字列の先頭から1文字進めるたびに木の深さを1つ増やすことを繰り返して、再帰的に処理を進める
                TrieNode root_node(
                    /*code  = */ 0xffff,
                    /*depth = */ 0,
                    /*left  = */ 0,
                    /*right = */ (int)keys.size(),
                    /*index = */ ROOT_INDEX);
                putNode(root_node);

                LOG_INFOH(L"DONE: dblArray SIZE={}", dblArray->size());
            }

            return error == 0 ? dblArray : std::shared_ptr<DoubleArray>();
        }

        //-----------------------------------------------------------------------------------
    private:
        // dblArray を埋めていく
        // siblings から始めて、再帰的にノードを挿入していく。ノード木を作りつつ、dblArray を埋めていくという感じか。
        void putNode(const TrieNode& node) {
            if (error < 0) return;

            if (debug) {
                curNode = node;
                curLeft = keyArray[node.left];
                curRight = keyArray[node.right - 1];
            }

            if (node.code == 0) {
                // 終端ノード列の挿入
                putTermNodes(node);
            } else {
                // 子ノード列の挿入
                putSiblingsNode(node);
            }
        }

        // 子ノード(列)は単語終端になっている
        // checkAry には 負値により逆向きのポインタを格納し、複数エントリのチェーンを実現する
        // baseAry には検索返却値を入れる
        // 複数エントリの数だけ、空きセルを検索してリストで返す
        void putTermNodes(const TrieNode& node) {
            if (node.right == node.left + 1) {
                // 唯一の終端単語なら、そこに値を入れる (値は非負値のみとする)
                int v = valArray[node.left];
                if (v >= 0 || !dblArray->dupAllowed) {
                    dblArray->baseArray[node.index] = valArray[node.left];
                } else {
                    // 重複エントリを許可するモードの場合、負値は扱えない
                    error = ERROR_NEG_VALUE;
                    errorIndex = node.left + 1;
                }
            } else if (dblArray->dupAllowed) {
                // 複数ある場合は、さらに別のセルを用意。そこのインデックスは正負を反転させて負値とする
                auto freelist = find_free_cells(node.right - node.left, firstUnchecked);
                std::reverse(freelist.begin(), freelist.end());
                int beg = freelist.front();
                firstUnchecked = beg + 1;
                dblArray->baseArray[node.index] = -beg;
                dblArray->baseArray[beg] = valArray[node.left];
                int prev = beg;
                int i = -node.left + 1;
                assert(i >= 0);
                if (i < 0) LOG_WARN(L"NEGATIVE i = {}", i);
                int j = 0;
                for (size_t j = 0; j < freelist.size() && i < node.right; ++j) {
                    int idx = freelist[j];
                    dblArray->checkArray[prev] = idx - prev;
                    dblArray->baseArray[idx] = valArray[i++];
                    prev = idx;
                }
                dblArray->checkArray[prev] = LAST_ENTRY;
            } else {
                // 複数エントリは許可しない
                error = ERROR_DUP_ENTRY;
                errorIndex = node.left + 1;
            }
        }

        // まだ単語の先が延びている子ノード列の挿入
        void putSiblingsNode(const TrieNode& node) {
            auto oSiblings = expandNode(node);
            if (!oSiblings) return;  // エラー

            auto& siblings = oSiblings.value();

            // 子ノード列用の空きエリアを探す
            int begin = find_first_room(siblings);

            // DoubleArrayImpl._used(begin) = true
            dblArray->baseArray[node.index] = begin;

            // 全子ノードに対して親インデックスをセットする
            dblArray->resize(begin + siblings.back().code);
            for (auto& sib : siblings) {
                sib.index = begin + sib.code;
                dblArray->checkArray[sib.index] = node.index;
            }
            // 各子ノードを挿入する
            for (auto& sib : siblings) {
                putNode(sib);
            }
        }

        //--
        // parent ノードが被覆するキー列に対し、対応するノード列を作成して返す。(キー列はソートされている必要がある)
        std::optional<std::vector<TrieNode>> expandNode(const TrieNode& parent) {
            if (error < 0) return std::nullopt;

            std::vector<TrieNode> siblings;
            int prevCode = 0;

            auto depth = parent.depth;
            for (int i = parent.left; i < parent.right; ++i) {
                auto& key = keyArray[i];

                if (depth <= (int)key.length()) {
                    int curCode = (depth < (int)key.length()) ? key[depth] : 0;

                    if (prevCode > curCode) {
                        // キーがソートされていなかった
                        error = ERROR_NOT_SORTED;
                        errorIndex = i;
                        return std::nullopt;
                    }

                    if (curCode != prevCode || siblings.empty()) {
                        if (!siblings.empty()) siblings.back().right = i;
                        TrieNode tmp_node(
                            /*code  = */ curCode,
                            /*depth = */ (short)(parent.depth + 1),
                            /*left  = */ i,
                            /*right = */ 0,
                            /*index = */ 0);
                        siblings.push_back(tmp_node);
                    }

                    prevCode = curCode;
                }
            }

            if (siblings.empty()) return std::nullopt;

            // 子ノードの右端の次を親ノードの右隣につなげる
            siblings.back().right = parent.right;
            return std::make_optional(siblings);
        }

        // 指定位置から最初にある未使用セルを探す
        int find_uncheck_pos(int idx) {
            int p = idx;
            dblArray->resize(p);
            while (dblArray->checkArray[p] != 0) {
                p += 1;
                dblArray->resize(p);
            }
            return p;
        }

      // 複数エントリの数だけ、空きセルを検索してリストで返す
        std::vector<int> find_free_cells(int num, int idx) {
            std::vector<int> list;
            for (int i = 0; i < num; ++i) {
                int next = find_uncheck_pos(idx);
                list.push_back(next);
                idx = next + 1;
            }
            return list;
        }

        // siblings を格納できるだけの空きスペースを見つける
        int find_first_room(const std::vector<TrieNode>& sibs) {

            firstUnchecked = find_uncheck_pos(firstUnchecked);

            int nextUnchecked = firstUnchecked;
            int begin = nextUnchecked - sibs.front().code;    // siblings の先頭の文字を空きエリアの開始位置とするため、begin をずらしている
            size_t ni = 1;                                     // node index (先頭はすでに空きであることがわかっているので 1 から始める)
            while (ni < sibs.size()) {
                int pos = begin + sibs[ni].code;
                if (pos < 0) LOG_WARN(L"NEGATIVE pos = {}", pos);
                dblArray->resize(pos);
                if (dblArray->checkArray[pos] == 0) {
                    ni += 1;
                } else {
                    nextUnchecked = find_uncheck_pos(nextUnchecked + 1);
                    begin = nextUnchecked - sibs.front().code;
                    ni = 1;
                }
            }

            // freeCheckPos から begin までの間で空いているセルの数が少なくなってきたら、
            // freeCheckPos を begin に移動する
            int beg_pos = begin + sibs.front().code;
            if (beg_pos - firstUnchecked >= 200) {               // ad hoc. 200個以上離れている場合に
                int req_num = (beg_pos - firstUnchecked) / 20;      // ad hoc. 5% 以上の空きを必要とする
                int free_num = 0;
                int pos = firstUnchecked;
                while (req_num > 0 && pos + req_num < beg_pos) {
                    if (dblArray->checkArray[pos] == 0) req_num -= 1;
                    pos += 1;
                }
                if (req_num > 0) firstUnchecked = beg_pos;
            }

            return begin;
        }

    };
    DEFINE_CLASS_LOGGER(DoubleArrayBuilder);

    //// DoubleArray インスタンスの作成
    //SharedPtr<DoubleArray> createDoubleArray(
    //    const Vector<String>& keys,
    //    const Vector<int>& values,
    //    const bool dbg)
    //{
    //    DoubleArrayBuilder builder(dbg);
    //    return builder.build(keys, values);
    //}

    class DoubleArrayBuildHelperImpl : public DoubleArrayBuildHelper {

        std::vector<int> emptyList;
        std::shared_ptr<DoubleArrayBuilder> builder = std::make_shared<DoubleArrayBuilder>(true);
        std::shared_ptr<DoubleArray> dblArray;

    public:
        std::shared_ptr<DoubleArray> createDoubleArray(
            const std::vector<String>& keys,
            const std::vector<int>& values) override
        {
            dblArray = builder->build(keys, values);
            return dblArray;
        }

        int error() override {
            return builder->error;
        }

        int errorIndex() override {
            return builder->errorIndex;
        }

        const std::vector<int>& baseArray() override {
            auto p = dynamic_cast<DoubleArrayImpl*>(dblArray.get());
            return p ? p->baseArray : emptyList;
        }

        const std::vector<int>& checkArray() override {
            auto p = dynamic_cast<DoubleArrayImpl*>(dblArray.get());
            return p ? p->checkArray : emptyList;
        }
    };

    std::shared_ptr<DoubleArrayBuildHelper> DoubleArrayBuildHelper::createHelper() {
        std::shared_ptr<DoubleArrayBuildHelper> ptr;
        ptr.reset(new DoubleArrayBuildHelperImpl());
        return ptr;
    }

#if 0
    // 以下、Test用
    class DoubleArrayTestHelperImpl : public DoubleArrayTestHelper {

        std::vector<int> emptyList;
        std::shared_ptr<DoubleArrayBuilder> builder = std::make_shared<DoubleArrayBuilder>(true);
        std::shared_ptr<DoubleArray> dblArray;

    public:
        std::shared_ptr<DoubleArray> createDoubleArray(
            const std::vector<String>& keys,
            const std::vector<int>& values) override
        {
            dblArray = builder->build(keys, values);
            return dblArray;
        }

        int error() override {
            return builder->error;
        }

        int errorIndex() override {
            return builder->errorIndex;
        }

        const std::vector<int>& baseArray() override {
            auto p = dynamic_cast<DoubleArrayImpl*>(dblArray.get());
            return p ? p->baseArray : emptyList;
        }

        const std::vector<int>& checkArray() override {
            auto p = dynamic_cast<DoubleArrayImpl*>(dblArray.get());
            return p ? p->checkArray : emptyList;
        }
    };

    std::shared_ptr<DoubleArrayTestHelper> DoubleArrayTestHelper::createHelper() {
        std::shared_ptr<DoubleArrayTestHelper> ptr;
        ptr.reset(new DoubleArrayTestHelperImpl());
        return ptr;
    }
#endif

} // namespace darts
