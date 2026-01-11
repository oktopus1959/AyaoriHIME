#include "string_utils.h"
#include "path_utils.h"
#include "OptHandler.h"
#include "Logger.h"
#include "exception.h"

#include "dict/Dictionary.h"
#include "Tokenizer.h"
#include "Lattice.h"

#if 0
#undef LOG_DEBUGH
#undef LOG_DEBUG
#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFOH
#endif

namespace analyzer {

    int UNKNOWN_OTHER_COST = 10000;
    int UNKNOWN_KANJI_COST = 5000;
    int UNKNOWN_KANJI_MAX_LEN = 4;

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

    public:
        // Constructor
        Impl(OptHandlerPtr opts) : opts(opts) {
            LOG_INFOH(L"ENTER: ctor: opts");
            verbose = opts->getString(L"verbose");
            debugLevel = utils::parseInt(verbose);

            // 辞書および設定ファイルのオープン
            open_dics();

            LOG_INFOH(L"LEAVE: ctor");
        }


        //val whatLog = WhatLog()

        //Lazy<dict::DictionaryInfo> dictionary_info;

        /**
         * センテンスの指定位置からの辞書引き(これはかなりキモになるメソッド)
         * @param pos 辞書引き対象となる部分文字列の開始位置 (lattice.sentence の部分文字列でなければならない)
         * @param lattice センテンスのNgram解析した結果をラティス構造で管理するオブジェクト
         * @param mazePenalty 交ぜ書きエントリに対するペナルティ(正値なら、2文字以下は x5, 3文字は x3, 4文字以上は x1 とする)
         * @return 候補となるNgramノードのリスト。空白文字しかなくて有効なNgramノードが作成できない場合は、空リストのまま返す。
         */
        Vector<NodePtr> lookup(Lattice& lattice, size_t pos) {
            LOG_DEBUG(L"ENTER: pos={}", pos);

            auto rngStrPtr = lattice.sentence->subString(pos);
            auto end = rngStrPtr->end();

            // 戻値となるノードリスト (空白文字しかなくて有効なNgramノードが作成できない場合は、空リストのまま返す)
            Vector<NodePtr> result_nodes;

            // 非空白文字の開始位置
            auto begin2 = rngStrPtr->skipSpace(pos);

            // ノードを作成してリストに追加するローカル関数
            auto __addNewNode = [&](int wcost, size_t end2, int addCost = 0)
            {
                //auto node = node::Node::Create(sentence->subString(begin2, end2));
                auto nt = node::NodeType::NORMAL_NODE;
                auto node = lattice.newNode(begin2, end2, nt);
                //    node->posid   = token.posId;
                node->setWcost(wcost + addCost);
                node->setRlength((int)(end2 - rngStrPtr->begin()));
                //node->setStat(node::NodeType::NORMAL_NODE);
                //node->isUnknown = isUnk;
                result_nodes.push_back(node);
            };

            // 先頭が 〓
            if (rngStrPtr->charAt(begin2) == GETA_CHAR) {
                LOG_DEBUG(L"BEGIN: geta");
                // 〓を1文字の未知語として切り出す
                __addNewNode(UNKNOWN_OTHER_COST, begin2 + 1);
                LOG_DEBUG(L"END: geta");
            }
            // 辞書引きしてノード作成する
            for (const auto& dic : dics) {
                // 各辞書ごとに辞書引きをして
                for (const auto& [tokens, length] : dic->commonPrefixSearch(rngStrPtr->toString(begin2), false)) {
                    // 辞書引きされた各表層形ごとに
                    for (const auto& token : tokens) {
                        // 同一表層形の各Ngramごとにノードを作成
                        __addNewNode(token->wcost, begin2 + length);
                    }
                }
            }

            // -- ここまでは辞書にある単語、以下は解の候補として未知語も考慮に入れる --

            if (result_nodes.empty()) {
                // 辞書引きしてもNgramが得られなかった
                LOG_DEBUG(L"BEGIN: unk");

                // 未知語長さの候補に対して未知語を切り出してノードリストに追加
                if (utils::is_kanji(rngStrPtr->charAt(begin2))) {
                    // 先頭文字が漢字の場合は、1文字の漢字未知語として切り出す
                    __addNewNode(UNKNOWN_KANJI_COST, begin2 + 1);
                } else {
                    // 先頭文字が漢字以外の場合は、空白、ひらがな、漢字以外の文字の連続を未知語として切り出す
                    size_t len = 1;
                    while (begin2 + len < end) {
                        wchar_t ch = rngStrPtr->charAt(begin2 + len);
                        if (utils::is_space(ch) || utils::is_hiragana(ch) || utils::is_kanji(ch)) break;
                        ++len;
                    }
                    __addNewNode(UNKNOWN_OTHER_COST, begin2 + len);
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
            //LOG_INFOH(L"load Unknown dic: START");
            //unkdic = MakeShared<dict::Dictionary>(opts);
            //unkdic->load(utils::join_path(prefix, UNK_DIC_FILE));
            //LOG_INFOH(L"load Unknown dic: DONE");

            // システム辞書
            sysdic = MakeShared<dict::Dictionary>(opts);
            sysdic->load(utils::join_path(prefix, SYS_DIC_FILE));

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

        //size_t get_max_grouping_size() {
        //    auto size = opts->getInt(L"max-grouping-size", 0);
        //    return size <= 0 ? DEFAULT_MAX_GROUPING_SIZE : (size_t)size;
        //}

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

    Vector<node::NodePtr> Tokenizer::lookup(Lattice& lattice, size_t pos) {
        return pImpl->lookup(lattice, pos);
    }

    /** ユーザー辞書の再ロード */
    void Tokenizer::reload_userdics() {
        pImpl->reset_dics();
        pImpl->reload_userdics();
    }

} // namespace analyzer
