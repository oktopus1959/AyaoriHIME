#include "string_utils.h"
#include "path_utils.h"
#include "OptHandler.h"
#include "Logger.h"
#include "exception.h"

#include "dict/Dictionary.h"
#include "Tokenizer.h"
#include "Lattice.h"

#if 1
#undef LOG_DEBUGH
#undef LOG_DEBUG
#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFOH
#endif

namespace analyzer {
    //-----------------------------------------------------------------------------
    /**
     * Tokenizer
     */

    class Tokenizer::Impl {
        DECLARE_CLASS_LOGGER;
        using NodePtr = node::NodePtr;

        friend Tokenizer;

    private:
        OptHandlerPtr opts;

        String verbose;
        int debugLevel = 0;

        Vector<DictionaryPtr> dics;         // system and user dictionaries
        DictionaryPtr sysdic;               // system dictionary
        Vector<DictionaryPtr> userdics;     // user dictionaries
        DictionaryPtr unkdic;               // unknown words dictionary
        CharProperty charPproperty;

        //  private val bos_feature = get_bos_feature()
        String unk_feature;
        Vector<Vector<SafePtr<Token>>> unk_tokens;
        size_t max_grouping_size = 0;
        CharInfo SPACE;

    public:
        // Constructor
        Impl(OptHandlerPtr opts) : opts(opts) {
            LOG_INFOH(L"ENTER: ctor: opts");
            verbose = opts->getString(L"verbose");
            debugLevel = utils::parseInt(verbose);

            unk_feature = get_unk_feature();
            max_grouping_size = get_max_grouping_size();

            //dictionary_info.setCreator([this]() { return make_dic_info_chain(); });

            // 辞書および設定ファイルのオープン
            open_dics();

            LOG_INFOH(L"LEAVE: ctor");
        }


        //val whatLog = WhatLog()

        //Lazy<dict::DictionaryInfo> dictionary_info;

        /**
         * センテンスの指定位置からの辞書引き(これはかなりキモになるメソッド)
         * @param pos 辞書引き対象となる部分文字列の開始位置 (lattice.sentence の部分文字列でなければならない)
         * @param lattice センテンスの形態素解析した結果をラティス構造で管理するオブジェクト
         * @param mazePenalty 交ぜ書きエントリに対するペナルティ(正値なら、2文字以下は x5, 3文字は x3, 4文字以上は x1 とする)
         * @return 候補となる形態素ノードのリスト。空白文字しかなくて有効な形態素ノードが作成できない場合は、空リストのまま返す。
         */
        Vector<NodePtr> lookup(Lattice& lattice, size_t pos, int mazePenalty, bool allowNonTerminal) {
            LOG_DEBUG(L"ENTER: pos={}, mazePenalty={}, allowNonTerminal={}", pos, mazePenalty, allowNonTerminal);

            auto rngstr = lattice.sentence->subString(pos);
            auto end = rngstr->end();

            // 戻値となるノードリスト (空白文字しかなくて有効な形態素ノードが作成できない場合は、空リストのまま返す)
            Vector<NodePtr> result_nodes;

            // 空白類以外の文字を探す ⇒ 位置が begin2 に、文字情報が Cinfo に返る
            UniqPtr<CharInfo> cinfo = MakeUniq<CharInfo>();

            // 非空白文字の開始位置
            auto begin2 = charPproperty.seekToOtherType(rngstr, SPACE, cinfo);

            // ノードを作成してリストに追加するローカル関数
            auto __addNewNode = [&](DictionaryPtr dic, const Token& token, size_t end2, node::NodeType stat,
                StringRef feature = L"", int addCost = 0 /*, bool isUnk = false*/)
            {
                //auto node = node::Node::Create(sentence->subString(begin2, end2));
                auto node = lattice.newNode(begin2, end2);
                String surf = node->surface()->toString();
                node->setLcAttr(token.lcAttr);
                node->setRcAttr(token.rcAttr);
                //    node->posid   = token.posId;
                node->setWcost(token.wcost + addCost);
                String feat = !feature.empty() ? feature : dic->feature(token);
                if (stat == node::NodeType::UNKNOWN_NODE) {
                    feat.append(std::format(L",{},{},{}", surf, surf, surf));
                }
                node->setFeature(feat);
                node->setRlength((int)(end2 - rngstr->begin()));
                node->setStat(stat);
                node->setCharType(cinfo->primaryType());
                LOG_DEBUG(L"__addNewNode: ENTER: surf={}, feat={}, wcost={}", surf, feat, node->wcost());
                // 交ぜ書きエントリに対するペナルティ
                if (mazePenalty != 0) {
                    String feat = node->feature();
                    if (utils::endsWith(feat, L"MAZE")) {
                        int factor = 1;
                        if (mazePenalty > 0) {
                            size_t len = end2 > begin2 ? end2 - begin2 : 0;
                            factor = len <= 2 ? 5 : len == 3 ? 3 : len == 4 ? 1 : 0;
                        }
                        if (utils::startsWith(feat, L"名詞:固有名詞")) factor += 2;
                        LOG_DEBUG(L"__addNewNode: MAZE penalty={}, factor={}", mazePenalty, factor);
                        node->addWcost(mazePenalty * factor);
                    } else if (mazePenalty < 0) {
                        if (utils::contains(feat, L",非MAZE") || utils::contains(feat, L",非交")) {
                            // 非交ぜ書きエントリに対するペナルティ(負値の場合のみ)
                            LOG_DEBUG(L"__addNewNode: add non-maze penalty: feat={}, penalty={}", feat, 1000000);
                            node->addWcost(1000000);
                        }
                    }
                }
                //node->isUnknown = isUnk;
                LOG_DEBUG(L"__addNewNode: LEAVE: node={}", node->toVerbose());
                result_nodes.push_back(node);
            };

            // 辞書引きしてノード作成する
            for (const auto& dic : dics) {
                // 各辞書ごとに辞書引きをして
                for (const auto& [tokens, length] : dic->commonPrefixSearch(rngstr->toString(begin2), allowNonTerminal)) {
                    // 辞書引きされた各表層形ごとに
                    for (const auto& token : tokens) {
                        // 同一表層形の各形態素ごとにノードを作成
                        __addNewNode(dic, *token, begin2 + length, node::NodeType::NORMAL_NODE);
                    }
                }
            }

            // -- ここまでは辞書にある単語、以下は解の候補として未知語も考慮に入れる --

            if (result_nodes.empty() || cinfo->invoke()) {
                // 辞書引きしても形態素が得られなかったか、追加の未知語処理の起動が設定されている場合
                LOG_DEBUG(L"BEGIN: unk");

                // 切り出す未知語の長さの候補リストを得るローカル関数 (辞書引きされたエントリと同じ長さのものも含む)
                auto __getUnkLenCandidates = [&]() {
                    Vector<size_t> ary;
                    auto otherPos = charPproperty.seekToOtherType(rngstr->subString(begin2), cinfo->primarize());
                    auto maxlen = std::min(otherPos - begin2, max_grouping_size);
                    auto limit = std::max(std::min((size_t)cinfo->spanLimit(), maxlen), size_t(0));  // math.max で 0 以上を保証する
                    for (size_t i = 1; i <= limit; ++i) ary.push_back(i);
                    if (limit < maxlen && cinfo->group()) ary.push_back(maxlen);
                    LOG_DEBUG(L"unk array={}", utils::join_primitive(ary, L", "));
                    return ary;
                };

                // 未知語ノード追加用のローカル関数
                auto __addUnknown = [&](size_t end2) {
                    size_t primType = cinfo->primaryType();
                    if (primType < unk_tokens.size()) {
                        // 文字数に応じた未知語の追加コスト
                        // auto addCost = UNK_INITIAL_COST + ONE_CHAR_COST_FACTOR * (end2 - begin2 - 1)
                        auto addCost = 0;

                        for (const auto token : unk_tokens[primType]) {
                            __addNewNode(unkdic, *token, end2, node::NodeType::UNKNOWN_NODE,
                                         L"", addCost /*, true*/);
                        }
                    }
                };

                // 未知語長さの候補に対して未知語を切り出してノードリストに追加
                for (auto len : __getUnkLenCandidates()) {
                    __addUnknown(begin2 + len);
                }
                LOG_DEBUG(L"END: unk");
            }

            if (Reporting::Logger::IsDebugEnabled()) showResultNodes(result_nodes);

            LOG_DEBUG(L"LEAVE: result_nodes.size={}", result_nodes.size());

            std::reverse(result_nodes.begin(), result_nodes.end());
            return result_nodes;
        }


    private:
        void showResultNodes(const Vector<NodePtr>& nodes) {
            String msg;
            msg.append(L"\n.\n--------------------------------------------------\n");
            msg.append(L"Lookup results:\n");
            for (const auto& node : nodes) { msg.append(node->toVerbose()).append(L"\n"); };
            msg.append(L"--------------------------------------------------\n");
            LOG_INFO(msg);
        }


        // システム辞書のみを再設定
        void reset_dics() {
            dics.clear();
            dics.push_back(sysdic);
        }

        /**
         * 辞書および設定ファイルのオープン
         */
        void open_dics() {
            LOG_INFOH(L"ENTER");
            auto prefix = opts->getString(L"dicdir", L".");

            // 未知語辞書
            LOG_INFOH(L"load Unknown dic: START");
            unkdic = MakeShared<dict::Dictionary>(opts);
            unkdic->load(utils::join_path(prefix, UNK_DIC_FILE));
            LOG_INFOH(L"load Unknown dic: DONE");

            if (!charPproperty.loadBinary(opts)) THROW_RTE(L"Tokenizer::open: cannot open property");

            // システム辞書
            sysdic = MakeShared<dict::Dictionary>(opts);
            sysdic->load(utils::join_path(prefix, SYS_DIC_FILE));
            if (sysdic->getType() != dict::DictionaryInfo::SYSTEM_DIC) THROW_RTE(L"Tokenizer::open: not a system dictionary: {}", prefix);
            charPproperty.set_charset(sysdic->charset());

            unk_tokens = get_unk_tokens();
            SPACE = charPproperty.getCharInfo(L"SPACE");

            reset_dics();

            // ユーザー辞書
            reload_userdics();

            LOG_INFOH(L"LEAVE");
        }

        /** ユーザー辞書の再ロード */
        void reload_userdics() {
            auto userdic = opts->getString(L"userdic");
            LOG_INFOH(L"ENTER: userdics={}", userdic);
            userdics.clear();
            if (!userdic.empty()) {
                for (const auto& fname : utils::reSplit(userdic, L" *, *")) {
                    try {
                        auto d = MakeShared<dict::Dictionary>(opts);
                        // 辞書のロード
                        d->load(fname);
                        if (d->getType() != dict::DictionaryInfo::USER_DIC) THROW_RTE(L"Tokenizer::open: not a user dictionary: {}", fname);
                        if (!sysdic->isCompatible(*d)) THROW_RTE(L"Tokenizer::open: incompatible dictionary: {}", fname);
                        dics.push_back(d);
                        userdics.push_back(d);
                    }
                    catch (const RuntimeException& ex) {
                        LOG_ERROR(L"cannot open user dictionary {}: {}({}): {}", fname, ex.getFile(), ex.getLine(), ex.getCause());
                    }
                    catch (...) {
                        LOG_ERROR(L"unknow error: user dictionary {}", fname);
                    }
                }
            }
            LOG_INFOH(L"LEAVE: userdics.size={}", userdics.size());
        }

        Vector<Vector<SafePtr<Token>>> get_unk_tokens() {
            Vector<Vector<SafePtr<Token>>> result;
            for (const auto& name : charPproperty.names()) {
                result.push_back(unkdic->exactMatchSearch(name));
            }
            LOG_DEBUG(L"unk_tokens.size={}", result.size());
            return result;
        }

        String get_unk_feature() {
            return opts->getString(L"unk-feature");
        }

        size_t get_max_grouping_size() {
            auto size = opts->getInt(L"max-grouping-size", 0);
            return size <= 0 ? DEFAULT_MAX_GROUPING_SIZE : (size_t)size;
        }

        //SharedPtr<dict::DictionaryInfo> make_dic_info_chain() {
        //    auto dic_info : dict::DictionaryInfo = null;
        //    dics.reverse.foreach{ dic = >
        //      dic_info = dict::DictionaryInfo(dic.info, dic_info);
        //    }
        //    dic_info;
        //}

    }; // Tokenizer::Impl
    DEFINE_CLASS_LOGGER(Tokenizer::Impl);

    DEFINE_CLASS_LOGGER(Tokenizer);

    // Constructor
    Tokenizer::Tokenizer(OptHandlerPtr opts) : pImpl(MakeUniq<Impl>(opts)) {
        LOG_INFOH(L"CALLED: ctor");
    }

    // Destructor
    Tokenizer::~Tokenizer() {
        // ここで pImpl が解放される
    }

    const Vector<DictionaryPtr>& Tokenizer::getDictionaries() const {
        return pImpl->dics;
    }

    Vector<node::NodePtr> Tokenizer::lookup(Lattice& lattice, size_t pos, int mazePenalty, bool allowNonTerminal) {
        return pImpl->lookup(lattice, pos, mazePenalty, allowNonTerminal);
    }

    /** ユーザー辞書の再ロード */
    void Tokenizer::reload_userdics() {
        pImpl->reset_dics();
        pImpl->reload_userdics();
    }

} // namespace analyzer
