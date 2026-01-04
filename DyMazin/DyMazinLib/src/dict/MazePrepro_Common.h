#pragma once
 
#include "std_utils.h"
#include "util/string_utils.h"
#include "util/path_utils.h"
#include "util/file_utils.h"
#include "util/regex_utils.h"
#include "util/OptHandler.h"
#include "util/my_utils.h"
#include "util/ptr_utils.h"
#include "util/transform_utils.h"
#include "exception.h"

#include "MazegakiPreprocessor.h"

using namespace util;
using Reporting::Logger;

template <typename T>
using Vector = std::vector<T>;

template <typename T>
using VectorRef = const std::vector<T>&;

template <typename T, typename U>
using Map = std::map<T, U>;

template <typename T, typename U>
using MapRef = const std::map<T, U>&;

template <typename T>
using Set = std::set<T>;

template <typename T>
using SetRef = const std::set<T>&;

typedef VectorRef<String> VectorRefString;
typedef Vector<String> VectorString;
typedef Map<String, String> MapString;
typedef Map<String, int> MapInt;
typedef MapRef<String, String> MapRefString;
typedef Set<String> SetString;
typedef SetRef<String> SetRefString;

namespace {
    inline String replace_suffix(StringRef str, StringRef oldSuffix, StringRef newSuffix) {
        if (str.length() >= oldSuffix.length() && str.substr(str.length() - oldSuffix.length()) == oldSuffix) {
            return str.substr(0, str.length() - oldSuffix.length()) + newSuffix;
        }
        return str;
    }

    inline String _safeGet(VectorRef<String> vec, size_t n) {
        if (n < 0 || n >= vec.size()) {
            return L"";
        }
        return vec[n];
    }

    template<typename T>
    inline T _safeGet(const std::map<String, T>& map, StringRef key) {
        auto it = map.find(key);
        if (it != map.end()) {
            return it->second;
        }
        return T();
    }

    inline bool _reMatch(StringRef str, StringRef pattern) {
        return RegexUtil(pattern).match(str);
    }

    inline bool _reSearch(StringRef str, StringRef pattern) {
        return utils::reSearch(str, pattern);
    }

    inline String _replaceTail(StringRef str, size_t len, StringRef tgt) {
        return utils::replace_tail(str, len, tgt);
    }

    inline VectorString _reMatchScan(StringRef yomi, StringRef patt) {
        return utils::reScan(yomi, L"^" + patt + L"$");
    }

    inline String _tr(StringRef str, StringRef p, StringRef t, size_t len = String::npos) {
        return utils::tr(str, p, t, len);
    }

    inline String _kataka2hiragana(StringRef str) {
        String result = str;
        for (wchar_t& ch : result) {
            if (ch >= L'ァ' && ch <= L'ヶ') {
                ch -= 0x60;
            }
        }
        return result;
    }
}

namespace MazegakiPreprocessor {
    void loadFiles(StringRef joyoKanjiFile, StringRef kanjiBigramFile);

    VectorString findYomi(StringRef yomi, StringRef k1, StringRef h1, StringRef k2, StringRef h2, StringRef k3, StringRef h3, StringRef k4);
}

