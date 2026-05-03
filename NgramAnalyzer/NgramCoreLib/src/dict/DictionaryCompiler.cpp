#include "std_utils.h"
#include "util/string_utils.h"
#include "util/path_utils.h"
#include "util/OptHandler.h"
#include "util/my_utils.h"
#include "util/ptr_utils.h"
#include "util/transform_utils.h"
#include "util/xsv_parser.h"
#include "exception.h"
#include "analyzer/Token.h"
#include "analyzer/Connector.h"
#include "analyzer/DictionaryRewriter.h"
#include "analyzer/RangeString.h"
#include "darts/DoubleArray.h"
#include "Dictionary.h"

using namespace util;
using namespace analyzer;
using Reporting::Logger;

namespace {
#if false
    //using IdPair = dict::IdPair;
    using DictionaryInfo = dict::DictionaryInfo;

    // 属性IDのペア (属性サイズにも使う)
    struct IdPair {
        size_t left = 0;
        size_t right = 0;

        IdPair() { }

        IdPair(size_t l, size_t r) : left(l), right(r) { }
    };
#endif

#if false
    //---------------------------------------------------------------------------
    // 辞書ソースにコストが設定されていなかった場合の単語コスト計算
    class CostCalculator {
        DECLARE_CLASS_LOGGER;
    private:
        OptHandlerPtr _opts;
        SharedPtr<DictionaryRewriterLazy> _rewriter;
        SharedPtr<DecoderFeatureIndex> _fi;
        SharedPtr<CharProperty> _prop;

        SharedPtr<DecoderFeatureIndex>  createFI(OptHandlerPtr opts) {
            auto fi = MakeShared<DecoderFeatureIndex>();
            if (!fi->open(opts)) THROW_RTE(L"cannot open feature index");
            return fi;
        }

        SharedPtr<CharProperty> createProp(OptHandlerPtr opts) {
            auto prop = MakeShared<CharProperty>();
            if (!prop->loadBinary(opts)) THROW_RTE(L"cannot open property");
            // prop.set_charset(from.c_str());
            return prop;
        }

    public:
        int calc(StringRef key, StringRef feature) {
            auto path = MakeShared<node::LearnerPath>();
            auto rnode = MakeShared<node::LearnerNode>();
            auto lnode = MakeShared<node::LearnerNode>();
            rnode->stat(node::NodeType::NORMAL_NODE);
            lnode->stat(node::NodeType::NORMAL_NODE);
            rnode->rpath = path;
            lnode->lpath = path;
            path->lnode = lnode;
            path->rnode = rnode;

            auto cinfo = property()->getCharInfo(key.empty() ? '\0' : key[0]);
            path->rnode->charType((int)cinfo.primaryType());

            String ufeature;
            String lfeature;
            String rfeature;
            _rewriter->rewrite2(feature, ufeature, lfeature, rfeature);
            fi()->buildUnigramFeature(path, ufeature);
            fi()->calcCost(rnode);
            return util::toCost(rnode->wdcost, factor);
        }

    public:
        int factor = 0;

        CostCalculator() {}

        CostCalculator(OptHandlerPtr opts, SharedPtr<DictionaryRewriterLazy> rewriter) : _opts(opts), _rewriter(rewriter) {
            factor = opts->getInt(L"cost-factor", 0);
            if (factor <= 0) THROW_RTE(L"cost factor needs to be positive value");
        }

        SharedPtr<DecoderFeatureIndex> fi() {
            if (!_fi) _fi = createFI(_opts);
            return _fi;
        }
        SharedPtr<CharProperty> property() {
            if (!_prop) _prop = createProp(_opts);
            return _prop;
        }

    };
    DEFINE_CLASS_LOGGER(CostCalculator);
#endif

#if false
    //---------------------------------------------------------------------------
    // Feature Rewirter
    class FeatureRewriter {
        DECLARE_CLASS_LOGGER;

        SharedPtr<TextWriter> writer;

        Lazy<ContextIDMapper> lzCid;

    private:
        ContextIDMapper* createCID() {
            auto cid = new ContextIDMapper();
            cid->open(left_id_file, right_id_file); //, &config_iconv);
            if (cid->left_size() != attrRng.left && cid->right_size() == attrRng.right) {
                THROW_RTE(L"Context ID files({} or {} may be broken", left_id_file, right_id_file);
            }
            return cid;
        }

    public:
        String node_format;
        SharedPtr<analyzer::DictionaryRewriterLazy> rewriter;
        String left_id_file;
        String right_id_file;
        IdPair attrRng;

        // コンストラクタ
        FeatureRewriter(
            StringRef node_fmt,
            SharedPtr<analyzer::DictionaryRewriterLazy> rewrtr,
            StringRef left_id_f,
            StringRef right_id_f,
            IdPair attr)
            :
            node_format(node_fmt),
            rewriter(rewrtr),
            left_id_file(left_id_f),
            right_id_file(right_id_f),
            attrRng(attr)
        {
            LOG_INFO((L"node_fmt={}", node_fmt));
            writer = TextWriter::CreateTextWriter();
            LAZY_INITIALIZE(lzCid, createCID());
        }

        //    private val node = new Node()

        String rewrite(StringRef key, StringRef feat, short pid) {
            if (node_format.empty()) {
                return feat;
            } else {
                auto node = MakeUniq<Node>(RangeString::Create(key));
                node->setFeature(feat);
                node->setRlength((int)node->surface()->length());
                // node.posid   = pid
                node->setStat(node::NodeType::NORMAL_NODE);
                CHECK_OR_THROW(writer, L"writer is null");
                String os = writer->writeNode(node->surface(), node_format, *node);
                if (os.empty()) {
                    String msg = std::format(L"conversion error: {} with {}", feat, node_format);
                    LOG_ERROR(msg);
                    THROW_RTE(msg);
                }
                return os;
            }
        }

        IdPair makeIdPair(StringRef feature) {
            String ufeature;
            String lfeature;
            String rfeature;
            if (!rewriter->rewrite(feature, ufeature, lfeature, rfeature)) {
                THROW_RTE(L"rewrite failed: {}", feature);
            }
            return IdPair(lzCid()->lid(lfeature), lzCid()->rid(rfeature));
        }
    };
    DEFINE_CLASS_LOGGER(FeatureRewriter);
#endif

    //---------------------------------------------------------------------------
    DEFINE_NAMESPACE_LOGGER(LOCAL);

    inline int to_int(StringRef str) { return utils::parseInt(str, INT_MAX); }

    inline short to_short(StringRef str) { return utils::parseInt(str, SHRT_MIN); }

#if false
    using ShortPair = std::pair<short, short>;

    bool is_valid_attr_index(const IdPair& idxPair, const IdPair& validRange) {
        return (idxPair.left >= 0 && idxPair.right >= 0 && idxPair.left < validRange.left&& idxPair.right < validRange.right);
    }

    bool is_valid_attr_index(ShortPair idxPair, ShortPair validRange) {
        return (idxPair.first >= 0 && idxPair.second >= 0 && idxPair.first < validRange.first && idxPair.second < validRange.second);
    }

    // 左右接続属性のサイズを取得
    IdPair get_id_range(OptHandlerPtr opts) {
        const auto& dicdir = opts->getValue(_T("dicdir"));
        auto matrix_file = utils::joinPath(dicdir, MATRIX_DEF_FILE);

        auto [lsize, rsize] = Connector::readMatrixSize(matrix_file);
        if (lsize > 0 && rsize > 0) {
            return IdPair(lsize, rsize);
        } else {
            try {
                auto connector = std::make_unique<Connector>(opts->getString(L"model"));
                return IdPair(connector->left_size(), connector->right_size());
            }
            catch (...) {
                return IdPair(1, 1);
            }
        }
    }
#endif

    /**
     *  見出し語とToken データのマップを作成する。
     *  返値: (見出し語配列, Token 配列)
     */
    void compile_tokens(
        OptHandlerPtr opts,
        int dicType,
        const Vector<String>& dics,
        Vector<String>& keys,       // 出力: 見出し語の配列
        Vector<Token>& tokens       // 出力: Tokenの配列
    ) {
        auto dicdir = opts->getString(L"dicdir");
        LOG_INFOH(L"ENTER: dicdir={}", dicdir);

        //auto rewriter = MakeShared<DictionaryRewriterLazy>(utils::joinPath(dicdir, REWRITE_FILE));

        using ElementType = std::pair<String, Token>;
        Vector<UniqPtr<ElementType>> dictionary;

        for (const auto& dic : dics) {
            LOG_INFOH(L"reading {} ... ", dic);
            std::wcout << L"reading " << dic << L" ... " << std::flush;
            Vector<String> dic_lines;
            if (utils::exists_path(dic)) {
                for (const auto& ln : utils::readAllLines(dic)) {
                    String line = utils::strip(ln);
                    if (!line.empty() && !utils::startsWith(line, L"#")) dic_lines.push_back(line); // '#' で始まる見出し語は '"' で囲むこと
                }
            } else {
                THROW_RTE(L"no such file or directory: {}", dic);
            }

            size_t count = 0;

            for (const auto& line : dic_lines) {
                auto items = utils::reSplit(line, L"[ \\t]");
                if (items.size() < 2) continue;

                // 0: 表層形
                // 1: 単語コスト
                const auto& key = items[0];
                if (key.empty() || key[0] == '#') {
                    // 空行またはコメント行は無視
                    continue;
                } else {
                    int cost = to_int(items[1]);
                    dictionary.push_back(MakeUniq<ElementType>(key, Token(cost)));

                    count += 1;
                }
            }

            LOG_INFOH(L"count = {}", count);
            std::cout << count << std::endl;    // 辞書に含まれていたエントリ数を表示
        }

        Vector<SafePtr<ElementType>> sorted;
        utils::transform_append(dictionary, sorted, [](const auto& p) { return SafePtr(p);});
        std::sort(sorted.begin(), sorted.end(), [](const auto& l, const auto& r) { return l->first < r->first; });

        // (見出し配列, Token配列) を返す
        keys.clear();
        tokens.clear();
        for (const auto p : sorted) {
            keys.emplace_back(p->first);
            tokens.emplace_back(p->second);
        }

        LOG_INFOH(L"LEAVE");
    }

    void compile_tokens_from_lines(
        OptHandlerPtr opts,
        int dicType,
        const Vector<String>& dic_lines,
        Vector<String>& keys,           // 出力: 見出し語の配列
        Vector<Token>& tokens           // 出力: Tokenの配列
    ) {
        auto dicdir = opts->getString(L"dicdir");
        LOG_INFOH(L"ENTER: dicdir={}", dicdir);

        //auto rewriter = MakeShared<DictionaryRewriterLazy>(utils::joinPath(dicdir, REWRITE_FILE));

        using ElementType = std::pair<String, Token>;
        Vector<UniqPtr<ElementType>> dictionary;

        size_t count = 0;

        for (const auto& line : dic_lines) {
            auto items = utils::reSplit(line, L"[ \\t]");
            if (items.size() < 2) continue;

            // 0: 表層形
            // 1: 単語コスト
            const auto& key = items[0];
            if (key.empty() || key[0] == '#') {
                // 空行またはコメント行は無視
                continue;
            } else {
                int cost = to_int(items[1]);
                dictionary.push_back(MakeUniq<ElementType>(key, Token(cost)));

                count += 1;
            }
        }

        LOG_INFOH(L"count = {}", count);

        // val sorted = dictionary.sortBy(_._1)  // TODO: is stable sort better?
        Vector<SafePtr<ElementType>> sorted;
        utils::transform_append(dictionary, sorted, [](const auto& p) { return SafePtr(p);});
        std::sort(sorted.begin(), sorted.end(), [](const auto& l, const auto& r) { return l->first < r->first; });

        // (見出し配列, Token配列) を返す
        keys.clear();
        tokens.clear();
        for (const auto p : sorted) {
            keys.emplace_back(p->first);
            tokens.emplace_back(p->second);
        }

        LOG_INFOH(L"LEAVE");
    }

}

namespace dict {
    DEFINE_NAMESPACE_LOGGER(dict);

    //---------------------------------------------------------------------------
    void logErrorCause(DoubleArrayBuildHelperPtr helper) {
        String cause;
        switch (helper->error()) {
        case darts::ERROR_NO_ENTRY: cause = _T("no entries found"); break;
        case darts::ERROR_NOT_SORTED: cause = _T("Disordered entry"); break;
        case darts::ERROR_DUP_ENTRY: cause = _T("Duplicated entry"); break;
        // case darts::ERROR_DUP_ENTRY => "Duplicated entry"
        default: cause = _T("Unknown"); break;
        }
        String errMsg = std::format(L"{}: : error={}, index={}", cause, helper->error(), helper->errorIndex());
        LOG_ERROR(L"{}: : error={}, index={}", cause, helper->error(), helper->errorIndex());
    }

    // ユニークなエントリのみをインデクシングして DoubleArray を構築
    SharedPtr<darts::DoubleArray> build_double_array(Vector<String>& keys) {
        Vector<String> strAry;
        Vector<int> idxAry;

        size_t idx = 0;
        size_t dupnum = 1;
        for (size_t nextIdx = 1; nextIdx <= keys.size(); ++nextIdx) {
            if (nextIdx < keys.size() && keys[idx] == keys[nextIdx]) {
                dupnum += 1;
            } else {
                strAry.push_back(keys[idx]);
                idxAry.push_back((int)((idx << 8) + (dupnum & 0xff)));    // 上位23bitが位置、下位8bitが重複エントリの個数を表す
                dupnum = 1;
                idx = nextIdx;
            }
        }

        //CHECK_OR_THROW_MSG(strAry.size() == idxAry.size(), L"");

        auto helper = darts::DoubleArrayBuildHelper::createHelper();
        auto dblAry = helper->createDoubleArray(strAry, idxAry);
        if (!dblAry) {
            String cause;
            switch (helper->error()) {
            case darts::ERROR_NO_ENTRY: cause = _T("no entries found"); break;
            case darts::ERROR_NOT_SORTED: cause = _T("Disordered entry"); break;
            case darts::ERROR_DUP_ENTRY: cause = _T("Duplicated entry"); break;
                // case darts::ERROR_DUP_ENTRY => "Duplicated entry"
            default: cause = _T("Unknown"); break;
            }
            String errMsg = std::format(L"{}: : error={}, index={}", cause, helper->error(), helper->errorIndex());
            LOG_ERROR(errMsg);
            THROW_RTE(errMsg);
        }
        return dblAry;
    }

    //--------------------------------------------------------------
    void progress_bar_darts(size_t current, size_t total) {
        util::progress_bar(L"emitting double-array", current, total);
    }

    //---------------------------------------------------------------------------
    /**
     * compile dictionary
     */
    void Dictionary::compile(OptHandlerPtr opts, const Vector<String>& dics, StringRef outputfile) {

        LOG_INFOH(L"opts:\n{}", opts->dump());

        // コンパイルされた情報を格納する辞書
        Dictionary dict(opts);

        int dicType = opts->getInt(_T("type"), 0);

        // 辞書ソースを Token 列にコンパイルする (ソート済み)
        Vector<String> keys;
        compile_tokens(opts, dicType, dics, keys, dict._tokens);

        if (Logger::IsDebugEnabled()) {
            String keyTokens;
            for (size_t i = 0; i < 50 && i < keys.size(); ++i) {
                keyTokens.append(std::format(L"keys={}, _tokens={}\n", keys[i], dict._tokens[i].debugString()));
            }
            LOG_INFO(L"keys.size()={}, _tokens.size()={}\n{}", keys.size(), dict._tokens.size(), keyTokens.substr(0, 1024));
        }
        CHECK_OR_THROW(dict._tokens.size() > 0, L"no dictionary entries");
        CHECK_OR_THROW(dict._tokens.size() == keys.size(), L"keys.size({}) and _tokens.size({}) are different", keys.size(), dict._tokens.size());

        // DoubleArray 構築
        std::wcout << _T("building double array (keys=" << keys.size() << L")... ") << std::flush;
        dict._dblAry = build_double_array(keys);
        std::wcout << _T("done") << std::endl;

        // 辞書情報
        dict._info = DictionaryInfo(
            dics.front(),           // filename: filename of dictionary
            dict._tokens.size(),     // size: How many words are registered in this dictionary.
            dicType);               // dicType: dictionary type (USER_DIC, SYSTEM_DIC, or UNKNOWN_DIC)

        // シリアライズ
        LOG_INFOH(L"serializing dict: {}: START", outputfile);
        std::wcout << _T("serializing dict: ") << outputfile << _T(" ... ") << std::flush;
        dict.serialize(outputfile);
        std::wcout << _T("done\n") << std::endl;
        LOG_INFOH(L"serializing dict: DONE");
    }

    // ユーザ辞書のコンパイル
    // (dic_lines で与えられた行を辞書ソースとして扱う)
    void Dictionary::compileUserDict(OptHandlerPtr opts, const Vector<String>& dic_lines, StringRef outputfile) {

        // コンパイルされた情報を格納する辞書
        Dictionary dict(opts);

        int dicType = opts->getInt(_T("type"), 0);

        // 辞書ソースを Token 列にコンパイルする (ソート済み)
        Vector<String> keys;
        compile_tokens_from_lines(opts, dicType, dic_lines, keys, dict._tokens);

        // DoubleArray 構築
        LOG_INFOH(_T("building double array ... "));
        dict._dblAry = build_double_array(keys);
        LOG_INFOH(_T("done"));

        // 辞書情報
        dict._info = DictionaryInfo(
            L"user.csv",            // filename: filename of dictionary
            dict._tokens.size(),     // size: How many words are registered in this dictionary.
            dicType);               // dicType: dictionary type (USER_DIC, SYSTEM_DIC, or UNKNOWN_DIC)

        // シリアライズ
        LOG_INFOH(L"serializing dict: {}: START", outputfile);
        dict.serialize(outputfile);
        LOG_INFOH(L"serializing dict: DONE");
    }
    // end of compile()

} // namespace dict