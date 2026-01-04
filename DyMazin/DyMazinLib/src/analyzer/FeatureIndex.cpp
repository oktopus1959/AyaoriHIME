#include "FeatureIndex.h"

#include "path_utils.h"
#include "file_utils.h"
#include "xsv_parser.h"
#include "OptHandler.h"

#include "my_utils.h"

namespace analyzer {
    //-----------------------------------------------------------------------------
    // FeatureIndexBin : バイナリ版の FeatureIndex クラス

    DEFINE_CLASS_LOGGER(FeatureIndexBin);

    // read compiled binary
    SharedPtr<FeatureIndexBin> FeatureIndexBin::deserialize(StringRef path) {
        utils::IfstreamReader reader(path, true);
        CHECK_OR_THROW(reader.success(), L"can't open feature index binary file for read: {}", path);

        Vector<uint64_t> fingerprints;
        Vector<double> alphas;
        String charset;

        reader.read(fingerprints);
        reader.read(alphas);
        reader.read(charset);

        return MakeShared<FeatureIndexBin>(fingerprints, alphas, charset);
    }

    // write compiled binary
    void FeatureIndexBin::serialize(StringRef path) {
        utils::OfstreamWriter writer(path, true, false);
        CHECK_OR_THROW(writer.success(), L"can't open feature index binary file for write: {}", path);

        writer.write(fingerprints);
        writer.write(alphas);
        writer.write(charset);
    }

    //-----------------------------------------------------------------------------
    // FeatureIndex クラス
    DEFINE_CLASS_LOGGER(FeatureIndex);

    /**
     * モデルのコンパイル
     * @param text_filename = モデルの .def ファイル名 (ソース)
     * @param binary_filename = モデルの .bin ファイル名 (ターゲット)
     */
    bool FeatureIndex::compile(OptHandlerPtr opts, StringRef text_filename, StringRef binary_filename) {
        FeatureIndex::convert(text_filename)->serialize(binary_filename);
        return true;
    }

    /**
     * テキスト版確率モデルファイルをバイナリ版に変換
     */
    SharedPtr<FeatureIndexBin> FeatureIndex::convert(StringRef text_filename) {
        Vector<FpAlpha> dic;

        if (!text_filename.empty()) {
            CHECK_OR_THROW(utils::exists_path(text_filename), L"no such file or directory: {}", text_filename);

            utils::IfstreamReader reader(text_filename);
            CHECK_OR_THROW(reader.success(), L"can't open model file: {}", text_filename);

            bool header = true;
            while (true) {
                auto [ln, eof] = reader.getLine();
                if (eof) break;

                auto line = utils::strip(ln);
                if (header) {
                    if (!line.empty()) {
                        auto items = util::tokenizeN(line, ':', 2);
                        CHECK_OR_THROW(items.size() == 2, L"format error: {}", line);
                    } else {
                        header = false;
                    }
                } else if (!line.empty() && line[0] != '#') {
                    auto items = util::tokenizeN(line, '\t', 2);
                    CHECK_OR_THROW(items.size() == 2, L"format error: {}", line);
                    StringRef feature = items[1];
                    uint64_t fp = util::fingerprint(utils::utf8_byte_encode(feature));
                    double alpha = utils::strToDouble(items[0]);
                    dic.push_back(FpAlpha(fp, alpha));
                }
            }
        }

        //val sortedDic = dic.sortBy(_.fp).toArray;
        std::sort(dic.begin(), dic.end());
        return MakeShared<FeatureIndexBin>(dic);
    }

    /**
     * uni-gram 素性（単語素性）の構築<br/>
     * 素性テンプレートの "%F[n]" などを辞書に記述されている素性値で置換する。
     */
    bool FeatureIndex::buildUnigramFeature(LearnerPathPtr path, StringRef ufeature) {

        Vector<String> rawFeats = utils::parseCSV(ufeature);

        for (StringRef templ : unigram_templs) { // for example, templ == "U1:%F[0]"
            bool isValidFeat = true;
            size_t p = 0;
            String os;
            while (p < templ.length()) {
                auto ch = templ[p];
                switch (ch) {
                case '\\':
                    p += 1;
                    if (p < templ.length()) os.append(1, util::getEscapedChar(ch));
                    break;
                case '%':
                    p += 1;
                    if (p < templ.length()) {
                        switch (ch) {
                        case 'F':
                        {
                            auto [x, f] = getRawFeature(templ, p + 1, rawFeats);
                            p = x;
                            if (f.empty()) { isValidFeat = false; goto NEXT; }
                            os.append(f);
                        }
                        break;
                        case 't':
                            os.append(utils::to_wstring(path->rnode->charType()));
                            break;
                        case 'u':
                            os.append(ufeature);
                            break;
                        case 'w':
                            if (path->rnode->stat() == node::NodeType::NORMAL_NODE) {
                                path->rnode->surface->appendTo(os);
                            }
                            break;
                        default:
                            THROW_RTE(L"unknown meta char: {}", ch);
                            break;
                        }
                    }
                default:
                    os.append(1, ch);
                    break;
                }
                p += 1;
            }
        NEXT:
            if (isValidFeat) {
                auto _id = id(os);
                if (_id != NPOS) path->rnode->fvector.push_back(_id);
            }
        }

        return true;
    }

    /**
     * bi-gram 素性 （単語間の接続素性）の構築
     */
    bool FeatureIndex::buildBigramFeature(LearnerPathPtr path, StringRef rfeature, StringRef lfeature) {
        //import scala.util.control.Breaks
        auto leftFeats = utils::parseCSV(rfeature);
        auto rightFeats = utils::parseCSV(lfeature);

        for (const auto& templ : bigram_templs) {
            String os;
            bool isValidFeat = true;
            size_t p = 0;
            while (p < templ.length()) {
                auto ch = templ[p];
                switch (ch) {
                case '\\':
                    p += 1;
                    if (p < templ.length()) os.append(1, util::getEscapedChar(ch));
                    break;
                case '%':
                    p += 1;
                    switch (ch) {
                    case 'L':
                    {
                        auto [x, f] = getRawFeature(templ, p + 1, leftFeats);
                        p = x;
                        if (f.empty()) { isValidFeat = false; goto NEXT; } // goto NEXT;
                        else os.append(f);
                    }
                    break;
                    case 'R':
                    {
                        auto [x, f] = getRawFeature(templ, p + 1, rightFeats);
                        p = x;
                        if (f.empty()) { isValidFeat = false; goto NEXT; } // goto NEXT;
                        else os.append(f);
                    }
                    break;
                    case 'l':
                        os.append(lfeature);
                        break;
                    case 'r':
                        os.append(rfeature);
                        break;
                    default:
                        THROW_RTE(L"unknown meta char: {}", ch);
                        break;
                    }
                    break;
                default:
                    os.append(1, ch);
                    break;
                }
                p += 1;
            }
        NEXT:
            if (isValidFeat) {
                size_t _id = id(os);
                if (_id != NPOS) path->fvector.push_back(_id);
            }
        }

        return true;
    }

    /**
     * 単語ノード間のリンクコスト計算
     */
    void FeatureIndex::calcCost(LearnerPathPtr path) const {
        if (path->nonEmpty()) {
            path->dcost = path->rnode->wdcost;
            for (auto p : path->fvector) {
                path->dcost += probModel->alpha(p);
            }
        }
    }

    /**
     * 単語ノードのコスト計算
     */
    void FeatureIndex::calcCost(LearnerNodePtr node) const {
        node->wdcost = 0.0;
        if (!node->isEOS()) {
            for (auto p : node->fvector) {
                node->wdcost += probModel->alpha(p);
            }
        }
    }

    /**
     * 生素性の取得 （"%F[n]" などの形式で指定される n番目の生素性を取得）
     * @param templ 素性テンプレート文字列
     * @param pos templの中の処理位置
     * @param features 辞書ソースなどに記述されている生素性列
     * @return (次の処理位置, 取得した素性)
     */
    std::pair<size_t, String> FeatureIndex::getRawFeature(StringRef templ, size_t pos, const Vector<String>& features) {
        size_t x = templ.find(']', pos);
        String f;
        if (x >= templ.size()) {
            return { templ.length(), L"" };
        } else {
            auto items = utils::reScan(templ.substr(pos, x + 1 - pos), reTemplIndex);
            if (items.size() == 2) {
                StringRef undef = items[0];
                StringRef num = items[1];
                size_t idx = (size_t)utils::strToInt(num);
                if (idx < features.size()) {
                    StringRef feat = features[idx];
                    if (undef.empty() || (!feat.empty() && feat != L"*")) f = feat;
                } else {
                    THROW_RTE(L"invalid feature template: {}", templ);
                }
            }
        }
        return { x, f };
        //    var p = pos + 1
        //
        //    val undef = templ(p) == '?'  // 未定義フラグ
        //    if (undef) p += 1
        //
        //    CHECK_OR_THROW(templ(p) =='[', "getIndex(): unmatched '['")
        //
        //    var n = 0
        //    p += 1
        //
        //    while (p < templ.length()) {
        //      if (templ(p) >= '0' && templ(p) <= '9') {
        //        n = 10 * n + (templ(p) - '0');
        //      } else if (templ(p) == ']') {
        //        if (n >= features.size) {
        //          return (p, null)
        //        }
        //
        //        val feat = features(n)
        //        return (p, if (undef && (feat.isEmpty || feat == "*")) null else feat)
        //      } else {
        //        THROW_RTE(L"unmatched '[': " + templ)
        //      }
        //      p += 1
        //    }
        //
        //    (p, null)
    }

    /**
     * 素性テンプレート定義ファイル (feature.def) のロード
     */
    bool FeatureIndex::openTemplate(OptHandlerPtr opts) {
        //val filename = join_path(opts.get("dicdir"), FEATURE_FILE);
        String filename = FEATURE_FILE;
        CHECK_OR_THROW(utils::exists_path(filename), L"no such file or directory: {}", filename);

        unigram_templs.clear();
        bigram_templs.clear();

        size_t num = 0;

        for (StringRef line : utils::readAllLines(filename)) {
            //make_path(filename).lines().foreach
            num += 1;
            if (!line.empty() && line[0] != '#' && line[0] != ' ') {
                auto items = util::tokenizeN(line, L"\t ", 2);
                CHECK_OR_THROW(items.size() == 2, L"format error: {}", filename);

                if (items[0] == L"UNIGRAM") {
                    unigram_templs.push_back(items[1]); // strdup(items[1])
                } else if (items[0] == L"BIGRAM") {
                    bigram_templs.push_back(items[1]); // strdup(items[1])
                } else {
                    THROW_RTE(L"format error: {} ({}:{})", line, filename, num);
                }
            }
        }
        // second, load rewrite rules
        //rewriter.load(join_path(opts.get("dicdir"), constants.REWRITE_FILE));
        rewriter->open(REWRITE_FILE);

        return true;
    }

    //-----------------------------------------------------------------------------
    // EncoderFeatureIndex クラス
    DEFINE_CLASS_LOGGER(EncoderFeatureIndex);

    bool EncoderFeatureIndex::buildFeature(LearnerPathPtr path) {
        path->rnode->wdcost = 0.0;
        path->dcost = 0.0;

        String ufeature1;
        String lfeature1;
        String rfeature1;
        String ufeature2;
        String lfeature2;
        String rfeature2;

        CHECK_OR_THROW(rewriter->rewrite2(path->lnode->feature(), ufeature1, lfeature1, rfeature1),
            L" cannot rewrite pattern: {}", path->lnode->feature());

        CHECK_OR_THROW(rewriter->rewrite2(path->rnode->feature(), ufeature2, lfeature2, rfeature2),
            L" cannot rewrite pattern: {}", path->rnode->feature());

        {
            String key = utils::wconcatAny(ufeature2, ' ', path->rnode->charType());
            auto iter = feature_cache.find(key);
            if (iter != feature_cache.end()) {
                path->rnode->fvector = iter->second->fvec;
                iter->second->count += 1;
            } else {
                if (!buildUnigramFeature(path, ufeature2)) return false;
                feature_cache[key] = MakeShared<FeatVecPair>( path->rnode->fvector, 1);
            }
        }

        {
            String key = utils::wconcatAny(rfeature1, ' ', lfeature2);
            auto iter = feature_cache.find(key);
            if (iter != feature_cache.end()) {
                path->fvector = iter->second->fvec;
                iter->second->count += 1;
            } else {
                if (!buildBigramFeature(path, rfeature1, lfeature2)) return false;
                feature_cache[key] = MakeShared<FeatVecPair>(path->fvector, 1);
            }
        }

        CHECK_OR_THROW(!path->fvector.empty(), L"fvector is empty");
        CHECK_OR_THROW(!path->rnode->fvector.empty(), L"fevector is empty");

        return true;
    }

    // モデルファイルをロードする
    bool EncoderFeatureIndex::loadFile(StringRef filename, Vector<double>& alphas, OptHandlerPtr opts) {
        close();
        if (!utils::exists_path(filename)) return false;

        bool header = true;
        for (StringRef ln : utils::readAllLines(filename)) {
            auto line = utils::strip(ln);
            if (line.empty()) {
                header = false;
                alphas.clear();
                CHECK_OR_THROW(dic_.empty(), L"dic_ is empty");
                continue;
            }
            if (header) {
                // ヘッダー部の処理 (最初の空行まで)
                auto items = util::tokenizeN(line, ':', 2);
                CHECK_OR_THROW(items.size() == 2, L"format error: {}", line);
                if (items[0] != L"charset") {
                    //opts.set(items[0], items[1]);
                }
            } else {
                // 本体部の処理
                if (!line.empty()) {
                    auto items = util::tokenizeN(line, '\t', 2);
                    CHECK_OR_THROW(items.size() == 2, L"format error: %s{}", line);
                    StringRef alpha = items[0];
                    StringRef feature = items[1];
                    dic_[feature] = alphas.size();
                    alphas.push_back(utils::strToDouble(alpha));
                }
            }
        }
        return true;
    }

    // モデルファイルの保存
    bool EncoderFeatureIndex::save(StringRef filename, StringRef header) {
        utils::OfstreamWriter writer(filename);
        if (writer.success()) {
            writer.writeLine(header + L"\n");// 一行あける

            for (const auto& entry : dic_) {
                writer.writeLine(std::format(L"{:.16f}\t{}", probModel->alphas[entry.second], entry.first));
            }
            return true;
        }
        return false;
    }

    // 出現した feature を、出現頻度が freq 以上のものに縮減する。 freq <= 1 ならそのまま。
    void EncoderFeatureIndex::shrink(size_t freq, Vector<double>& observed) {
        Vector<size_t> freqv(dic_.size());
        // count fvector
        //freqv++ = new Array[Int](dic_.size);
        for (const auto& e : feature_cache) {
            for (auto f : e.second->fvec) {
                freqv[f] += e.second->count;  // freq
            }

        }
        if (freq <= 1) return;

        // TODO: not yet tested below
        // make old2new map
        Map<size_t, size_t> old2new;
        for (size_t i = 0; i < freqv.size(); ++i) {
            if (freqv[i] >= freq) {
                old2new[i] = old2new.size();  // maxid_++
            }
        }

        // update dic_
        for (const auto& entry : dic_) {
            StringRef key = entry.first;
            auto iter = old2new.find(entry.second);
            if (iter != old2new.end()) {
                dic_[key] = iter->second;
            } else {
                dic_.erase(key);
            }
        }

        // update all fvector
        for (const auto& entry : feature_cache) {
            StringRef key = entry.first;
            const auto& pair = entry.second;
            Vector<size_t> to;
            for (size_t f : pair->fvec) {
                auto iter = old2new.find(f);
                if (iter != old2new.end()) {
                    to.push_back(iter->second);
                }
            }
            feature_cache[key] = MakeShared<FeatVecPair>(to, pair->count);
        }

        // update observed vector
        Vector<double> observed_new(old2new.size());
        for (size_t i = 0; i < observed.size(); ++i) {
            auto iter = old2new.find(i);
            if (iter != old2new.end()) {
                observed_new[iter->second] = observed[i];
            }
        }

        // copy
        observed = observed_new;
    }

    //-----------------------------------------------------------------------------
    // DecoderFeatureIndex クラス （辞書生成時に使われる）
    DEFINE_CLASS_LOGGER(DecoderFeatureIndex);

    /**
     * 確率モデルファイルと素性テンプレート定義ファイルをロードする。
     * @param opts : OptHandler = "model" プロパティにより確率モデルファイルを指定する。
     */
    bool DecoderFeatureIndex::open(OptHandlerPtr opts) {
        auto modelfile = opts->getString(L"model");
        if (!modelfile.empty()) {
            CHECK_OR_THROW(utils::exists_path(modelfile), L"no such model file: {}", modelfile);
            // バイナリのモデルファイル (model.bin) をロードする
            if (!openBinaryModel(modelfile)) {
                LOG_ERROR(L"{} is not a binary model. reopen it as text mode...", modelfile);
                //      Console.err.flush()
                // バイナリモデルファイルが無ければテキストモデルファイルをロードする
                CHECK_OR_THROW(openTextModel(modelfile), L"can't read model file: {}", modelfile);
            }
        } else {
            probModel = convert(L"");
        }

        // 素性テンプレート定義ファイル (feature.def) をロードする
        if (!openTemplate(opts)) {
            close();
            return false;
        }

        return true;
    }

    bool DecoderFeatureIndex::buildFeature(LearnerPathPtr path) {
        path->rnode->wdcost = 0.0;
        path->dcost = 0.0;

        String ufeature1;
        String lfeature1;
        String rfeature1;
        String ufeature2;
        String lfeature2;
        String rfeature2;

        CHECK_OR_THROW(rewriter->rewrite2(path->lnode->feature(), ufeature1, lfeature1, rfeature1),
            L" cannot rewrite pattern: {}", path->lnode->feature());

        CHECK_OR_THROW(rewriter->rewrite2(path->rnode->feature(), ufeature2, lfeature2, rfeature2),
            L" cannot rewrite pattern: {}", path->rnode->feature());

        if (!buildUnigramFeature(path, ufeature2)) return false;

        if (!buildBigramFeature(path, rfeature1, lfeature2)) return false;

        return true;
    }

    // バイナリモデルファイルのロード
    bool DecoderFeatureIndex::openBinaryModel(StringRef modelfile) {
        try {
            probModel = FeatureIndexBin::deserialize(modelfile);
            return true;
        }
        catch (...) {
            return false;
        }
    }

    // テキストモデルファイルのロード
    bool DecoderFeatureIndex::openTextModel(StringRef modelfile) {
        try {
            probModel = convert(modelfile);
            return true;
        }
        catch (...) {
            return false;
        }
    }

    // 文字列からフィンガープリント値（ハッシュ値）を生成
    size_t DecoderFeatureIndex::id(StringRef key) {
        auto fp = util::fingerprint(key);
        return probModel->findFP(fp);
    }

} // namespace analyzer
