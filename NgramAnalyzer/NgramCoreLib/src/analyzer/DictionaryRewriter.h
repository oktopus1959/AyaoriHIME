#pragma once

#include "string_utils.h"
#include "exception.h"
#include "Lazy.h"
#include "reporting/Logger.h"

namespace analyzer {

    /**
     * Dictionary Rewriter
     */
    class DictionaryRewriter {
        DECLARE_CLASS_LOGGER;
    public:
        class RewritePattern {
        private:
            std::vector<String> spat_;
            std::vector<String> dpat_;

        public:
            bool set_pattern(StringRef src, StringRef dst);

            bool rewrite(const std::vector<String>& input, String& output) const;

            String format(StringRef fmt, const std::vector<String>& input) const;
        };

        class RewriteRules { // extends ArrayBuffer[RewritePattern]
            std::vector<RewritePattern> patterns;

        public:
            void clear() {
                patterns.clear();
            }

            void append(RewritePattern patt) {
                patterns.push_back(patt);
            }

            void append_pattern(StringRef src, StringRef dst);

            bool rewrite(const std::vector<String>& input, String& output);
        };

        // struct FeatureSet
        struct FeatureSet {
            String ufeature;
            String lfeature;
            String rfeature;

            FeatureSet(StringRef uf, StringRef lf, StringRef rf) :
                ufeature(uf), lfeature(lf), rfeature(rf)
            { }
        };

        void append_rewrite_rule(RewriteRules r, StringRef str);

        //bool match_rewrite_pattern(StringRef pat, StringRef str);

    private:
        RewriteRules  unigram_rewrite_;
        RewriteRules  left_rewrite_;
        RewriteRules  right_rewrite_;
        std::map<String, std::shared_ptr<FeatureSet>> cache_;

    public:
        DictionaryRewriter() { }

        DictionaryRewriter(StringRef filename) {
            open(filename);
        }

        void clear();

        bool open(StringRef filename);

        bool rewrite(StringRef feature,
            String& ufeature,
            String& lfeature,
            String& rfeature);

        bool rewrite2(StringRef feature,
            String& ufeature,
            String& lfeature,
            String& rfeature);

    };

    class DictionaryRewriterLazy {
    private:
        String filename;

        // lazy mechanism
        util::Lazy<DictionaryRewriter> writer;

    public:
        DictionaryRewriterLazy(StringRef filename)
            : filename(filename), writer(filename) { }

        void clear() {
            writer()->clear();
        }

        bool rewrite(StringRef feature,
            String& ufeature,
            String& lfeature,
            String& rfeature) {
            return writer()->rewrite(feature, ufeature, lfeature, rfeature);
        }

        bool rewrite2(StringRef feature,
            String& ufeature,
            String& lfeature,
            String& rfeature) {
            return writer()->rewrite2(feature, ufeature, lfeature, rfeature);
        }
    };

    /**
     * POSIDGenerator
     */
    class POSIDGenerator {
        DECLARE_CLASS_LOGGER;
    private:
        String filename;
        DictionaryRewriter::RewriteRules rewrite_;

    public:
        POSIDGenerator(StringRef filename);

        void clear() {
            rewrite_.clear();
        }

        short id(StringRef feature);

    };

} // namespace analyzer