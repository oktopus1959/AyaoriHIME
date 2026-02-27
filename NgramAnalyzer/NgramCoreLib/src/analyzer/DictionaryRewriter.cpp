#include "string_utils.h"
#include "path_utils.h"
#include "file_utils.h"
#include "xsv_parser.h"
#include "exception.h"

#include "DictionaryRewriter.h"

using namespace util;

namespace analyzer {

    DEFINE_CLASS_LOGGER(DictionaryRewriter);

    bool match_rewrite_pattern(StringRef pat, StringRef str) {
        if (pat.empty() || str.empty()) return false;

        if (pat[0] == '*' || pat == str) return true;

        auto len = pat.length();
        if (len >= 3 && pat[0] == '(' && pat[len - 1] == ')') {
            for (const auto& col : utils::split(pat.substr(1, len - 2), '|')) {
                if (str == col) return true;
            }
        }
        return false;
    }

    /**
     * Dictionary Rewriter companion object
     */
    bool DictionaryRewriter::RewritePattern::set_pattern(StringRef src, StringRef dst) {
        spat_ = utils::parseCSV(src);
        dpat_ = utils::parseCSV(dst);
        return !spat_.empty() && !dpat_.empty();
    }

    bool DictionaryRewriter::RewritePattern::rewrite(const std::vector<String>& input, String& output) const {
        if (spat_.size() > input.size()) return false;

        for (size_t i = 0; i < spat_.size(); ++i) {
            if (!match_rewrite_pattern(spat_[i], input[i])) return false;
        }

        output.clear();

        for (size_t i = 0; i < dpat_.size(); ++i) {
            const auto& dp = dpat_[i];
            output.append(utils::escape_csv_element(format(dp, input)));
            if (i + 1 < dpat_.size()) output.append(1, ',');
        }
        return true;
    }

    String DictionaryRewriter::RewritePattern::format(StringRef fmt, const std::vector<String>& input) const {
        std::wregex reDec(LR"("^(.*?)\$(\d+)(.*)$")");
        String res;
        String tail = fmt;
        while (!tail.empty()) {
            auto items = utils::reScan(tail, reDec);
            if (items.size() == 3) {
                size_t n = utils::strToInt(items[1]);
                CHECK_OR_THROW(n > 0 && (n - 1) < input.size(), L"out of range: [{}] {}", fmt, n);
                res.append(items[0]);
                res.append(input[n - 1]);
                tail = items[2];
            } else {
                res.append(tail);
                break;
            }
        }
        return res;
    }


    void DictionaryRewriter::RewriteRules::append_pattern(StringRef src, StringRef dst) {
        RewritePattern patt;
        if (patt.set_pattern(src, dst)) append(patt);
    }

    bool DictionaryRewriter::RewriteRules::rewrite(const std::vector<String>& input, String& output) {
        for (const auto& patt : patterns) {
            if (patt.rewrite(input, output)) return true;
        }
        return false;
    }

    void DictionaryRewriter::append_rewrite_rule(RewriteRules r, StringRef str) {
        auto items = utils::reSplit(str, 2, L"[ \t]+");
        CHECK_OR_THROW(items.size() == 2, L"format error: {}", str);
        r.append_pattern(items[0], items[1]);
    }

    //bool DictionaryRewriter::match_rewrite_pattern(StringRef pat, StringRef str) {
    //    if (pat(0) == '*' || pat == str) return true;

    //    val len = pat.length();
    //    if (len >= 3 && pat(0) == '(' && pat(len - 1) == ')') {
    //        pat.substring(1, len - 1).split("""\|""").foreach{ col = > if (str == col) return true }
    //    }
    //    return false;
    //}

// public:
    void DictionaryRewriter::clear() {
        unigram_rewrite_.clear();
        left_rewrite_.clear();
        right_rewrite_.clear();
        cache_.clear();
    }

    bool DictionaryRewriter::open(StringRef filename) {
        CHECK_OR_THROW(utils::isFileExistent(filename), L"no such file or directory: {}", filename);
        int append_to = 0;
        int num = 0;
        try {
            for (const auto& ln : utils::readAllLines(filename)) {
                num += 1;
                String line = utils::strip(ln);
                if (!line.empty() && line[0] != '#') {
                    if (line == L"[unigram rewrite]") {
                        append_to = 1;
                    } else if (line == L"[left rewrite]") {
                        append_to = 2;
                    } else if (line == L"[right rewrite]") {
                        append_to = 3;
                    } else {
                        CHECK_OR_THROW(append_to != 0, L"no sections found");
                        switch (append_to) {
                        case 1: append_rewrite_rule(unigram_rewrite_, line); break;
                        case 2: append_rewrite_rule(left_rewrite_, line); break;
                        case 3: append_rewrite_rule(right_rewrite_, line); break;
                        }
                    }
                }
            }
            return true;
        }
        catch (RuntimeException ex) {
            //std::wcerr << ex.getCause() << L" (at " << filename << L":" << num << L")" << std::endl;
            LOG_ERROR(L"{} (at {}:{})", ex.getCause(), filename, num);
            //ex.getStackTrace.slice(0, 10).foreach{ x = > Console.err.println(x.debugString()) };
            return false;
        }
    }

    bool DictionaryRewriter::rewrite(StringRef feature,
        String& ufeature,
        String& lfeature,
        String& rfeature) {
        auto items = utils::parseCSV(feature);
        return (unigram_rewrite_.rewrite(items, ufeature) &&
            left_rewrite_.rewrite(items, lfeature) &&
            right_rewrite_.rewrite(items, rfeature));
    }

    bool DictionaryRewriter::rewrite2(StringRef feature,
        String& ufeature,
        String& lfeature,
        String& rfeature) {
        auto iter = cache_.find(feature);
        if (iter != cache_.end()) {
            ufeature = iter->second->ufeature;
            lfeature = iter->second->lfeature;
            rfeature = iter->second->rfeature;
        } else {
            if (!rewrite(feature, ufeature, lfeature, rfeature)) return false;

            cache_[feature] = std::make_shared<FeatureSet>(ufeature, lfeature, rfeature);
        }

        return true;
    }

    DEFINE_CLASS_LOGGER(POSIDGenerator);

    POSIDGenerator::POSIDGenerator(StringRef filename)
        : filename(filename) {
        // load 実行
        if (utils::isFileExistent(filename)) {
            std::wregex reDigits(LR"("\d+")");
            for (const auto& ln : utils::readAllLines(filename)) {
                String line = utils::strip(ln);
                auto items = utils::reSplit(line, 2, L"[ \t]");
                CHECK_OR_THROW(items.size() == 2, L"format error: {}", line);
                CHECK_OR_THROW(std::regex_match(items[1], reDigits), L"not a number: {}", items[1]);
                rewrite_.append_pattern(items[0], items[1]);
            }
        } else {
            std::wcout << filename << L" is not found. minimum setting is used" << std::endl;
            rewrite_.append_pattern(L"*", L"1");
        }

    }

    short POSIDGenerator::id(StringRef feature) {
        String tmp;
        if (rewrite_.rewrite(utils::parseCSV(feature), tmp))
            return (short)utils::strToInt(tmp);
        else
            return -1;
    }

} // namespace analyzer