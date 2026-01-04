#pragma once

#include "std_utils.h"
#include "misc_utils.h"
#include "string_utils.h"
#include "transform_utils.h"
#include "exception.h"
#include "constants/Constants.h"

#include "DictionaryRewriter.h"

#include "node/LearnerNode.h"
#include "node/LearnerPath.h"

#include "OptHandler.h"

namespace analyzer {

    //-----------------------------------------------------------------------------
    // 素性テンプレートのインデックス部("?[n]")にマッチさせる正規表現
    const std::wregex reTemplIndex = std::wregex(LR"("(\?)?\[(\d+)\]")");

    struct FpAlpha {
        uint64_t fp;
        double alpha;

        FpAlpha(uint64_t fp, double alpha) : fp(fp), alpha(alpha) { }

        inline bool operator<(const FpAlpha& rhs) const { return fp < rhs.fp; }
    };

    /**
     * バイナリ版の FeatureIndex クラス
     */
    class FeatureIndexBin {
        DECLARE_CLASS_LOGGER;
    public:
        Vector<uint64_t> fingerprints;
        Vector<double> alphas;
        String charset;

    public:
        FeatureIndexBin(
            const Vector<uint64_t>& fingerprints,
            const Vector<double>& alphas,
            StringRef charset = L"utf-8")
            : charset(charset)
        {
            assert(fingerprints.size() == alphas.size());
            utils::append(this->fingerprints, fingerprints);
            utils::append(this->alphas, alphas);
        }

        FeatureIndexBin(
            const Vector<FpAlpha>& fpAlphas,
            StringRef charset = L"utf-8")
            : charset(charset)
        {
            utils::transform_append(fpAlphas, this->fingerprints, [](const auto& fpAlpha) { return fpAlpha.fp;});
            utils::transform_append(fpAlphas, this->alphas, [](const auto& fpAlpha) { return fpAlpha.alpha;});
        }


        size_t find(size_t lb, size_t ub, uint64_t fp) {
            if (lb + 1 >= ub || fingerprints[lb] >= fingerprints[ub - 1]) {
                return lb;
            } else {
                size_t p = (lb + ub) / 2;
                return (fingerprints[p] <= fp) ? find(p, ub, fp) : find(lb, p, fp);
            }
        }

        int findFP(uint64_t fp) {
            size_t n = find(0, fingerprints.size(), fp);
            return (n < fingerprints.size() && fingerprints[n] == fp) ? (int)n : -1;
        }

        double alpha(size_t pos) {
            assert(pos < alphas.size());
            return alphas[pos];
        }

        // read compiled binary
        static SharedPtr<FeatureIndexBin> deserialize(StringRef path);

        // write compiled binary
        void serialize(StringRef path);
    };

    inline bool operator==(const FeatureIndexBin& lhs, const FeatureIndexBin& rhs) {
        return lhs.fingerprints == rhs.fingerprints && lhs.alphas == rhs.alphas && lhs.charset == rhs.charset;
    }

    //-----------------------------------------------------------------------------
    /**
     * FeatureIndex クラス
     */
    class FeatureIndex {
        DECLARE_CLASS_LOGGER;
    public:
        const size_t NPOS = 0xffffffff;

    private:
        // 素性テンプレートのインデックス部("?[n]")にマッチさせる正規表現
        //static const std::wregex reTemplIndex = std::wregex(LR"("(\ ? ) ? \[(\d + )\]")");

    public:
        /**
         * テキスト版確率モデルファイルをバイナリ版に変換
         */
        static SharedPtr<FeatureIndexBin> convert(StringRef text_filename);

    protected:
        Vector<String> unigram_templs;
        Vector<String> bigram_templs;
        SharedPtr<DictionaryRewriter> rewriter = MakeShared<DictionaryRewriter>();
        SharedPtr<FeatureIndexBin> probModel;  // 確率モデル

        // abstract
        virtual size_t id(StringRef key) = 0;

        virtual void clear() = 0;
        virtual void close() = 0;
        virtual bool buildFeature(LearnerPathPtr path) = 0;

    public:
        /**
         * モデルのコンパイル
         * @param text_filename = モデルの .def ファイル名 (ソース)
         * @param binary_filename = モデルの .bin ファイル名 (ターゲット)
         */
        static bool compile(OptHandlerPtr opts, StringRef text_filename, StringRef binary_filename);

        /**
         * uni-gram 素性（単語素性）の構築<br/>
         * 素性テンプレートの "%F[n]" などを辞書に記述されている素性値で置換する。
         */
        bool buildUnigramFeature(LearnerPathPtr path, StringRef ufeature);

        /**
         * bi-gram 素性 （単語間の接続素性）の構築
         */
        bool buildBigramFeature(LearnerPathPtr path, StringRef rfeature, StringRef lfeature);

        /**
         * 単語ノード間のリンクコスト計算
         */
        void calcCost(LearnerPathPtr path) const;

        /**
         * 単語ノードのコスト計算
         */
        void calcCost(LearnerNodePtr node) const;

    protected:
        /**
         * 生素性の取得 （"%F[n]" などの形式で指定される n番目の生素性を取得）
         * @param templ 素性テンプレート文字列
         * @param pos templの中の処理位置
         * @param features 辞書ソースなどに記述されている生素性列
         * @return (次の処理位置, 取得した素性)
         */
        std::pair<size_t, String> getRawFeature(StringRef templ, size_t pos, const Vector<String>& features);

    protected:
        /**
         * 素性テンプレート定義ファイル (feature.def) のロード
         */
        bool openTemplate(OptHandlerPtr opts);

    };

    //-----------------------------------------------------------------------------
    /**
     * EncoderFeatureIndex クラス （学習時に使われる）
     */
    class EncoderFeatureIndex : public FeatureIndex {
        DECLARE_CLASS_LOGGER;
    private:
        Map<String, size_t> dic_;

        struct FeatVecPair {
            Vector<size_t> fvec;
            size_t count = 0;

            FeatVecPair() { }

            FeatVecPair(const Vector<size_t>& fv, size_t cnt) : fvec(fv), count(cnt) { }
        };

        Map<String, SharedPtr<FeatVecPair>> feature_cache;

    protected:
        size_t id(StringRef key) override  {
            auto iter = dic_.find(key);
            if (iter == dic_.end()) {
                size_t v = dic_.size();
                dic_[key] = v;
                return v;
            } else {
                return iter->second;
            }
        }

    public:
        // 素性テンプレート定義ファイル (feature.def) をロードする
        bool open(OptHandlerPtr opts) {
            return openTemplate(opts);
        }

        void close() {
            dic_.clear();
            feature_cache.clear();
        }

        void clear() { }

        size_t size() {
            return  dic_.size();
        }

        // モデルファイルをロードする
        bool loadFile(StringRef filename, Vector<double>& alphas, OptHandlerPtr opts);

        // モデルファイルの保存
        bool save(StringRef filename, StringRef header);

        // 出現した feature を、出現頻度が freq 以上のものに縮減する。 freq <= 1 ならそのまま。
        void shrink(size_t freq, Vector<double>& observed);

        bool buildFeature(LearnerPathPtr path);

        void clearcache() {
            feature_cache.clear();
            rewriter->clear();
        }

        // 確率モデルの作成
        void createProbModel(const Vector<double>& alphas) {
            probModel = MakeShared<FeatureIndexBin>(Vector<uint64_t>(alphas.size()), alphas);
        }

    };

    //-----------------------------------------------------------------------------
    /**
     * DecoderFeatureIndex クラス （辞書生成時に使われる）
     */
    class DecoderFeatureIndex : public FeatureIndex {
        DECLARE_CLASS_LOGGER;
    public:
          /**
           * 確率モデルファイルと素性テンプレート定義ファイルをロードする。
           * @param opts : OptHandler = "model" プロパティにより確率モデルファイルを指定する。
           */
        bool open(OptHandlerPtr opts);

        void clear() override {}

        void close() override {}

        bool buildFeature(LearnerPathPtr path) override;

    private:
        // バイナリモデルファイルのロード
        bool openBinaryModel(StringRef modelfile);

        // テキストモデルファイルのロード
        bool openTextModel(StringRef modelfile);

        // 文字列からフィンガープリント値（ハッシュ値）を生成
    protected:
        size_t id(StringRef key) override;
    };
} // namespace analyzer
