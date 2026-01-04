#include "TextWriter.h"
#include "Lattice.h"

#include "exception.h"
#include "my_utils.h"
using namespace util;

#include "util/xsv_parser.h"

#include "featureDef.h"

namespace analyzer {

    using WriterFunc = Function<String(const Lattice&)>;

    struct WriterFormats {
        DECLARE_CLASS_LOGGER;

        WriterFunc writer;
        String node_format;
        String bos_format;
        String eos_format;
        String unk_format;
        String eon_format;

        WriterFormats() { }

        WriterFormats(WriterFunc wrtr) : writer(wrtr) { }


        ~WriterFormats() {
            LOG_INFOH(L"CALLED: dtor");
        }

        WriterFormats(
            WriterFunc wrtr,
            String node_fmt,
            String bos_fmt,
            String eos_fmt,
            String unk_fmt,
            String eon_fmt)
            :
            writer(wrtr),
            node_format(node_fmt),
            bos_format(bos_fmt),
            eos_format(eos_fmt),
            unk_format(unk_fmt),
            eon_format(eon_fmt)
        { }
    };
    DEFINE_CLASS_LOGGER(WriterFormats);

    class TextWriterImpl : public TextWriter {
        DECLARE_CLASS_LOGGER;

        OptHandlerPtr opts;

    public:
        // コンストラクタ
        TextWriterImpl(OptHandlerPtr opts) : opts(opts) {
            LOG_INFOH(L"ENTER: ctor");
            wfs.reset(makeWriterFormats());
            LOG_INFOH(L"LEAVE: ctor");
        }

        ~TextWriterImpl() {
            LOG_INFOH(L"CALLED: dtor");
        }

        // Node情報を返す
        String writeNode(RangeStringPtr sentence, StringRef format, const node::Node& node) override {
            LOG_INFO(L"CALLED: sentence='{}', format={}", sentence->toString(), format);
            String os;
            writeNodeInternal(sentence, format, node, os);
            return os;
        }

        // Node情報を返す
        String writeNode(const Lattice& lattice, const node::Node& node) override {
            LOG_INFO(L"CALLED");
            auto stat = node.stat();
            String fmt =
                stat == node::NodeType::BOS_NODE ? wfs->bos_format :
                stat == node::NodeType::DUMMY_BOS_NODE ? wfs->bos_format :
                stat == node::NodeType::EOS_NODE ? wfs->eos_format :
                stat == node::NodeType::UNKNOWN_NODE ? wfs->unk_format :
                stat == node::NodeType::NORMAL_NODE ? wfs->node_format :
                stat == node::NodeType::EONBEST_NODE ? wfs->eon_format : L"";
            return writeNode(lattice.sentence, fmt, node);
        }

        // Lattice情報を返す
        String write(const Lattice& lattice) override {
            LOG_INFO(L"CALLED");
            return wfs->writer ? wfs->writer(lattice) : L"";
        }

        // 交ぜ書き情報を返す
        String writeMaze(const Lattice& lattice) override {
            LOG_INFO(L"CALLED");
            return writeMaze3(lattice);
        }

        //String writeMaze(const Lattice& lattice, int mazeType) override {
        //    if (mazeType == 3) {
        //        return writeMaze3(lattice);
        //    } else if (mazeType == 2) {
        //        return writeMaze2(lattice);
        //    } else {
        //        return writeMaze1(lattice);
        //    }
        //}

    private:
        UniqPtr<WriterFormats> wfs;

        // 標準フォーマットによる出力
        String writeLattice(const Lattice& lattice) {
            LOG_INFO(L"CALLED");
            StringStream ss;
            for (auto node : lattice.nodeList()) {
                LOG_INFO(L"node-feature(): {}", node->feature());
                ss << node->surface()->toString();
                ss << TAB << node->feature() << EOL;
            }
            ss << L"EOS";
            return ss.str();
        }

        // 分かち書きによる出力
        String writeWakati(const Lattice& lattice) {
            LOG_INFO(L"CALLED");
            StringStream ss;
            bool bSpace = false;
            for (auto node : lattice.nodeList()) {
                if (bSpace) ss << SPACE;
                bSpace = true;
                ss << node->surface()->toString();
            }
            //ss << EOL;
            return ss.str();
        }

        // 交ぜ書き原形も含む出力
        String writeMaze1(const Lattice& lattice) {
            LOG_INFO(L"CALLED");
            return writeMaze_(lattice, false);
        }

        // 交ぜ書き原形も含む出力(feature付き)
        String writeMaze2(const Lattice& lattice) {
            LOG_INFO(L"CALLED");
            return writeMaze_(lattice, true);
        }

        // 交ぜ書き原形も含む出力(同じ位置で同じ長さの候補を含む)
        String writeMaze3(const Lattice& lattice) {
            LOG_INFO(L"CALLED");
            return writeMaze_(lattice, true, true);
        }

#define MAZE_FEAT_FORMAT L"@{}/{}/{}"

        // 交ぜ書き原形も含む出力 (表層形 TAB 変換形[|別候補]...(or -) TAB 交ぜ書き原形,主要品詞:細品詞[,変換形,MAZE]
        String writeMaze_(const Lattice& lattice, bool includeFeature, bool includeOtherCandidates = false) {
            LOG_INFO(L"ENTER: includeFeature={}, includeOtherCandidates={}", utils::boolToString(includeFeature), utils::boolToString(includeOtherCandidates));
            StringStream ss;
            bool bFirst = true;
            size_t pos = 0;
            for (auto node : lattice.nodeList()) {
                if (!bFirst) ss << EOL;    // セパレータ(改行)
                int len = node->length();
                bFirst = false;
                LOG_INFO(L"node: {}", node->toVerbose());      // 主要品詞:細品詞,表層形,交ぜ書き原形[,変換形,MAZE]
                auto surface = node->surface()->toString();
                ss << surface;
                bool secondItem = false;
                auto features = utils::split(node->feature(), TAB);
                if (features.size() > FEATURE_OFF) {
                    auto feats = utils::parseCSV(features[FEATURE_OFF]);
                    const auto& myHinshi = feats.size() > HINSHI_OFF ? feats[HINSHI_OFF] : L"";
                    if (includeOtherCandidates || (feats.size() > MAZE_OFF && feats[MAZE_OFF] == L"MAZE")) {
                        String kanjiForm;
                        if (feats.size() > XLAT_KF_OFF /* feats.size() > MAZE_OFF && feats[MAZE_OFF] == L"MAZE"*/) {
                            kanjiForm = feats[XLAT_KF_OFF];              // 変換形
                        } else if (feats.size() > XLAT_BASE_OFF) {
                            kanjiForm = feats[XLAT_BASE_OFF];            // 原形
                        } else {
                            kanjiForm = surface;
                        }
                        ss << L"\t" << kanjiForm;
                        secondItem = true;
                        if (includeOtherCandidates) {
                            //ss << std::format(L"@{}:{}:{}", feats[BASE_OFF], myHinshi, feats[XLAT_BASE_OFF]);
                            ss << std::format(MAZE_FEAT_FORMAT, surface, myHinshi, feats.size() > XLAT_BASE_OFF ? feats[XLAT_BASE_OFF] : L"");
                            ss << gatherOtherMazeCandidates(lattice, pos, len, kanjiForm, myHinshi);
                        }
                    }
                }
                if (includeFeature) {
                    if (!secondItem) ss << L"\t-";
                    if (features.size() > FEATURE_OFF) ss << L"\t" << features[FEATURE_OFF];
                }
                pos += len;
            }
            //ss << EOL;
            LOG_INFO(L"LEAVE:\n--------\n{}\n--------", ss.str());
            return ss.str();
        }

        // 「主要品詞:細品詞」を分割して返す
        std::tuple<String, String> getHinshiPair(StringRef hinshi) {
            auto hins = utils::split(hinshi, ':');   // 主要品詞、細品詞
            return {hins[0], hins.size() > 1 ? hins[1] : L""};
        }

#define VERTICAL_VAR '|'

        // 同じ位置で同じ長さの候補を追加 (|候補1|候補2...)
        String gatherOtherMazeCandidates(const Lattice& lattice, size_t pos, int len, StringRef myself, StringRef myHinshi) {
            LOG_INFOH(L"ENTER: pos={}, len={}, mySelf={}, myHinshi={}", pos, len, myself, myHinshi);
            StringStream ss1;
            StringStream ss2;
            StringStream ss3;
            std::set<String> seenSet;
            seenSet.insert(myself);
            auto [myHin1, myHin2] = getHinshiPair(myHinshi);
            for (auto node : lattice.gatherSameLenNodes(pos, len, myHinshi)) {
                LOG_INFO(L"node: {}", node->toVerbose());
                auto features = utils::split(node->feature(), TAB);
                if (features.size() > FEATURE_OFF) {
                    auto feats = utils::parseCSV(features[FEATURE_OFF]);
                    if (feats.size() > XLAT_KF_OFF) {
                        const auto& kanjiForm = feats[XLAT_KF_OFF];               // 変換形
                        if (seenSet.find(kanjiForm) != seenSet.end()) continue;

                        if (kanjiForm != myself) {
                            const auto& hinshi = feats[HINSHI_OFF];
                            auto cand = std::format(L"{}{}" MAZE_FEAT_FORMAT, VERTICAL_VAR, kanjiForm, node->surface()->toString(), hinshi, feats[XLAT_BASE_OFF]);
                            auto [hin1, hin2] = getHinshiPair(hinshi);
                            if (hin1 == myHin1) {
                                if (hin2 == myHin2) {
                                    ss1 << cand;
                                } else {
                                    ss2 << cand;
                                }
                            } else {
                                ss3 << cand;
                            }
                            seenSet.insert(kanjiForm);
                        }
                    }
                }
            }
            if (!ss2.str().empty()) ss1 << ss2.str();
            if (!ss3.str().empty()) ss1 << ss3.str();
            LOG_INFOH(L"LEAVE: {}", ss1.str());
            return ss1.str();
        }

        String writeNone(const Lattice& lattice) {
            return L"";  // do nothing
        }

        // ユーザー定義フォーマットによる出力
        String writeUser(const Lattice& lattice) {
            LOG_INFO(L"CALLED");
            String os;
            if (!writeNodeInternal(lattice.sentence, wfs->bos_format, *lattice.dummyBosNode(), os)) return L"";

            for (auto node : lattice.nodeList()) {
                auto fmt = node->stat() == node::NodeType::UNKNOWN_NODE ? wfs->unk_format : wfs->node_format;
                if (!writeNodeInternal(lattice.sentence, fmt, *node, os)) return L"";
            }
            if (!writeNodeInternal(lattice.sentence, wfs->eos_format, *lattice.eosNode(), os)) return L"";
            return os;
        }

        // ダンプ形式による出力
        String writeDump(const Lattice& lattice) {
            LOG_INFO(L"CALLED");
            StringStream ss;
#if 0
            for (auto node : lattice.nodeListWithSentinel()) {
                ss << node->id() << SPACE;
                if (node->stat() == node::NodeType::BOS_NODE) {
                    ss << L"BOS";
                } else if (node->stat() == node::NodeType::EOS_NODE) {
                    ss << L"EOS";
                } else {
                    ss << node->surface()->toString();
                }

                ss << std::format(L" {} {} {} {} {} {} {} {:.3f} {:.3f} {:.3f} {}",
                    node->surface()->toString(),
                    node->feature(),
                    node->rcAttr(),
                    node->lcAttr(),
                    // append(SPACE).append(node->posid).
                    node->charType(),
                    node::getNodeTypeStr(node->stat()),
                    node->isBest(),
                    node->alpha(),
                    node->beta(),
                    node->prob(),
                    node->accumCost2());

                auto path = node->lpath();
                while (path) {
                    //ss << std::format(L" {}:{}:{:.3f}", path->lnode->id(), path->cost, path->prob);
                    ss << std::format(L" {}:{}", path->lnode->id(), path->cost);
                    path = path->lnext;
                }
                ss << EOL;
            }
#endif
            return ss.str();
        }

        String writeEM(const Lattice& lattice) {
            StringStream ss;
#if 0
            auto min_prob = 0.0001F;
            for (auto node : lattice.nodeListWithSentinel()) {
                if (node->prob() >= min_prob) {
                    ss << L"U\t";
                    if (node->stat() == node::NodeType::BOS_NODE) {
                        ss << L"BOS";
                    } else if (node->stat() == node::NodeType::EOS_NODE) {
                        ss << L"EOS";
                    } else {
                        ss << node->surface()->toString();
                    }
                    //ss << TAB).append(node->feature).append(TAB).append(node->prob()).append(EOL);
                    ss << std::format(L"\t{}\t{:.3f}\n", node->feature(), node->prob());
                }
                auto path = node->lpath();
                while (path) {
                    //if (path->prob >= min_prob) {
                    //    //ss << L"B\t").append(path->lnode.feature).append(L"\t").
                    //    //    append(node->feature).append(TAB).append(path->prob).append(EOL);
                    //    ss << std::format(L"B\t{}\t{}\t{:.3f}\n", path->lnode->feature().c_str(), node->feature().c_str(), path->prob);
                    //}
                    path = path->lnext;
                }
            }
#endif
            return ss.str();
        }

        bool writeNodeInternal(RangeStringPtr sentence, StringRef format, const Node& node, String& os) {
            LOG_INFO(L"CALLED: sentence='{}', format={}, node={}", sentence->toString(), format, node.toVerbose());
            StringStream ss;
            Vector<String> items;
            size_t i = 0;
            wchar_t ch = '\0';

#define ADVANCE()  ++i; if (i < format.size()) { ch = format[i]; } else { LOG_WARN(L"format({}) is short.", format); return false; }

            if (format.empty()) {
                LOG_INFO(L"format is empty.");
                ss << node.feature().substr(0, 4) << '\t' << L"wc:" << node.wcost() << L",cc:0,ac:" << node.accumCost() << EOL;
            } else {
                while (i < format.length()) {
                    auto ch = format[i];
                    if (ch == '\\') {
                        ADVANCE();
                        ss << getEscapedChar(ch);
                    } else if (ch != '%') {
                        ss << ch;
                    } else {
                        // '%' macros
                        ADVANCE();
                        switch (ch) {
                            // input sentence
                        case 'S': ss << sentence->toString(); break;
                            // sentence length
                        case 'L': ss << sentence->length(); break;
                            // morph
                        case 'm': ss << node.surface()->toString(); break;
                        case 'M': ss << node.surface()->toString(node.surface()->end() - node.rlength()); break;
                        case 'h': ss << L"UNDEF"; break;       // Part-Of-Speech ID Undefined
                        case '%': ss << '%'; break;         // %
                        case 'c': ss << node.wcost(); break;  // word cost
                        case 'H': ss << node.feature(); break;
                        case 't': ss << node.charType(); break;
                        case 's': ss << node.getNodeType(); break;
                            //case 'P': ss << node.prob(); break;
                        case 'p': {
                            ADVANCE();
                            switch (ch) {
                                //case 'i': ss << node.id(); break;  // node id
                                    // space
                            case 'S':
                                ss << node.surface()->toString(node.surface()->end() - node.rlength(), node.surface()->begin());
                                break;
                                // start position
                            case 's': ss << node.surface()->begin(); break;
                                // end position
                            case 'e': ss << node.surface()->end(); break;
                                // connection cost
                            case 'C': ss << (node.accumCost2() - node.prev()->accumCost2() - node.wcost()); break;
                            case 'w': ss << (node.wcost()); break;  // word cost
                            case 'c': ss << (node.accumCost2()); break;  // best cost
                            case 'n': ss << (node.accumCost2() - node.prev()->accumCost2()); break;
                                // node cost
                                // * if best path, otherwise SPACE
                            case 'b': ss << ((node.isBest() != 0) ? '*' : SPACE); break;
                                //case 'P': ss << node.prob(); break;
                                //case 'A': ss << node.alpha(); break;
                                //case 'B': ss << node.beta(); break;
                            case 'l': ss << node.length(); break;  // length of morph
                                // length of morph including the spaces
                            case 'L': ss << node.rlength(); break;
                            case 'h': { // Hidden Layer ID
                                ADVANCE();
                                switch (ch) {
                                case 'l': ss << node.lcAttr(); break;   // current
                                case 'r': ss << node.rcAttr(); break;   // prev
                                default:
                                    LOG_WARN(L"lr is required after %ph");
                                    return false;
                                }
                                break;
                            }

                            case 'p': {
                                ADVANCE();
                                auto mode = ch;
                                ADVANCE();
                                auto sep = ch;
                                if (sep == '\\') {
                                    ADVANCE();
                                    sep = getEscapedChar(ch);
                                }
                                if (!node.lpath()) {
                                    LOG_WARN(L"no path information is available");
                                    return false;
                                }
                                auto path = node.lpath();
                                while (path) {
                                    if (path != node.lpath()) ss << sep;
                                    switch (mode) {
                                        //case 'i': ss << path->lnode->id(); break;
                                    case 'c': ss << path->cost(); break;
                                        //case 'P': ss << path->prob; break;
                                    default:
                                        //LOG_WARN(L"[icP] is required after %pp");
                                        return false;
                                    }
                                    path = path->lnext();
                                }
                                break;
                            }
                            default:
                                //LOG_WARN(L"[iseSCwcnblLh] is required after %p");
                                return false;
                            }
                            break;
                        }

                        case 'F': case 'f':
                        {
                            if (node.feature().empty() || node.feature()[0] == '\0') {
                                LOG_WARN(L"no feature() information available");
                                return false;
                            }
                            if (items.empty()) {
                                items = utils::parseCSV(node.feature());
                            }

                            // separator
                            wchar_t separator = TAB;  // default separator
                            if (ch == 'F') {  // change separator
                                ADVANCE();
                                separator = ch;
                                if (separator == '\\') {
                                    ADVANCE();
                                    separator = getEscapedChar(ch);
                                }
                            }
                            ADVANCE();
                            if (ch != '[') {
                                LOG_WARN(L"cannot find '['");
                                return false;
                            }
                            size_t n = 0;
                            bool sep = false;
                            bool isfil = false;

                            bool _continue = true;
                            while (_continue) {
                                ADVANCE();
                                if (ch >= '0' && ch <= '9') {
                                    n = 10 * n + (ch - '0');
                                } else if (ch == ',' || ch == ']') {
                                    if (n >= items.size()) {
                                        LOG_WARN(L"given index is out of range");
                                        return false;
                                    }
                                    isfil = (!items[n].empty() && items[n][0] != '*');
                                    if (isfil) {
                                        if (sep) {
                                            ss << separator;
                                        }
                                        ss << items[n];
                                    }
                                    if (ch == ']') {
                                        _continue = false;
                                    } else {
                                        sep = isfil;
                                        n = 0;
                                    }
                                } else {
                                    LOG_WARN(L"cannot find ']'");
                                    return false;
                                }
                            }
                            break;
                        }
                        default:
                            LOG_WARN(L"unknown meta char: {}", ch);
                            return false;
                        }  // end switch (ch)
                    } // end case '%'

                    ++i;
                } // end while
            }

            os.append(ss.str());
            return true;
        }

        //---------------------------------------------------------------------------
#define MAKE_LAMBDA(F) [this](const Lattice& p) { return F(p);}

        WriterFormats* makeWriterFormats() {
            if (!opts) return new WriterFormats();

            auto ostyle = opts->getString(L"output-format-type");
            LOG_INFO(L"ostyle={}", ostyle);
            if (ostyle == L"wakati") return new WriterFormats(MAKE_LAMBDA(writeWakati));
            if (ostyle == L"maze1") return new WriterFormats(MAKE_LAMBDA(writeMaze1));
            if (ostyle == L"maze2") return new WriterFormats(MAKE_LAMBDA(writeMaze2));
            if (ostyle == L"maze3") return new WriterFormats(MAKE_LAMBDA(writeMaze3));
            if (ostyle == L"none") return new WriterFormats(MAKE_LAMBDA(writeNone));
            if (ostyle == L"dump") return new WriterFormats(MAKE_LAMBDA(writeDump));
            if (ostyle == L"em") return new WriterFormats(MAKE_LAMBDA(writeEM));

            String node_format_key = L"node-format";
            String bos_format_key = L"bos-format";
            String eos_format_key = L"eos-format";
            String unk_format_key = L"unk-format";
            String eon_format_key = L"eon-format";

            // default values
            auto defaultOpts = util::OptHandler::CreateDefaultHandler();
            String node_format = defaultOpts->getValue(node_format_key);    //L"%m\\t%H\\n";
            String unk_format = node_format;                                //L"%m\\t%H\\n";
            String bos_format = defaultOpts->getValue(bos_format_key);      //L"";
            String eos_format = defaultOpts->getValue(eos_format_key);      //L"EOS\\n";
            String eon_format = defaultOpts->getValue(eon_format_key);      //L"";

            if (!ostyle.empty()) {
                node_format_key.append(L"-").append(ostyle);
                bos_format_key.append(L"-").append(ostyle);
                eos_format_key.append(L"-").append(ostyle);
                unk_format_key.append(L"-").append(ostyle);
                eon_format_key.append(L"-").append(ostyle);
                if (opts->getString(node_format_key).empty()) {
                    THROW_RTE(L"unkown format type [{}]", ostyle);
                }
            }

            String node_format_ostyle = opts->getString(node_format_key);
            LOG_INFO(L"node_format_key={}, node_format_ostyle={}", node_format_key, node_format_ostyle);
            String bos_format_ostyle = opts->getString(bos_format_key);
            LOG_INFO(L"bos_format_key={}, bos_format_ostyle={}", node_format_key, bos_format_ostyle);
            String eos_format_ostyle = opts->getString(eos_format_key);
            LOG_INFO(L"eos_format_key={}, eos_format_ostyle={}", node_format_key, eos_format_ostyle);
            String unk_format_ostyle = opts->getString(unk_format_key);
            LOG_INFO(L"unk_format_key={}, unk_format_ostyle={}", node_format_key, unk_format_ostyle);
            String eon_format_ostyle = opts->getString(eon_format_key);
            LOG_INFO(L"eon_format_key={}, eon_format_ostyle={}", node_format_key, eon_format_ostyle);

            bool formatChanged = false;
            if (node_format != node_format_ostyle) { node_format = node_format_ostyle; formatChanged = true; }
            if (bos_format != bos_format_ostyle) { bos_format = bos_format_ostyle; formatChanged = true; }
            if (eos_format != eos_format_ostyle) { eos_format = eos_format_ostyle; formatChanged = true; }
            if (unk_format != unk_format_ostyle) { unk_format = unk_format_ostyle; formatChanged = true; } else { unk_format = node_format; }
            if (eon_format != eon_format_ostyle) { eon_format = eon_format_ostyle; formatChanged = true; }

            LOG_INFO(L"formatChanged={}", formatChanged);
            if (formatChanged) {
                LOG_INFO(L"new WriterFormats(MAKE_LAMBDA(writeUser), ...);");
                return new WriterFormats(
                    MAKE_LAMBDA(writeUser),
                    node_format,
                    bos_format,
                    eos_format,
                    unk_format,
                    eon_format);
            } else {
                return new WriterFormats(MAKE_LAMBDA(writeLattice));
            }
        }

    };

    DEFINE_CLASS_LOGGER(TextWriterImpl);

    DEFINE_CLASS_LOGGER(TextWriter);

    TextWriter::~TextWriter() {
        LOG_INFOH(L"CALLED: dtor");
    }

    TextWriterPtr TextWriter::CreateTextWriter(OptHandlerPtr opts) {
        LOG_INFOH(L"CALLED");
        return MakeShared<TextWriterImpl>(opts);
    }

} // namespace analyzer
