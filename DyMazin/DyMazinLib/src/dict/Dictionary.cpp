#include "util/transform_utils.h"
#include "util/exception.h"
#include "util/file_utils.h"
#include "Dictionary.h"
#include "constants/Constants.h"

using namespace analyzer;
using namespace darts;

#if 0 || defined(_DEBUG)
#define _LOG_DEBUGH_FLAG true
#undef LOG_INFO
#undef LOG_DEBUGH
#undef LOG_DEBUG
#undef _LOG_DEBUGH
#define LOG_INFO LOG_INFOH
#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFOH
#define _LOG_DEBUGH LOG_INFOH
#endif

#define DEFAULT_MAZE_PENALTY 1000

namespace dict {
    DEFINE_CLASS_LOGGER(Dictionary);

    size_t countDictionaryInfo(const DictionaryInfo& dicInfo) {
        size_t count = 0;
        return count;
    }

    void serializeDictionaryInfo(const DictionaryInfo& dicInfo, utils::OfstreamWriter& writer) {
        writer.write(dicInfo.filename);
        writer.write(dicInfo.size);
        writer.write(dicInfo.dicType);
        writer.write(dicInfo.lsize);
        writer.write(dicInfo.rsize);
        writer.write(dicInfo.version);
        //DictionaryInfo* next;
    }

    void deserializeDictionaryInfo(DictionaryInfo& dicInfo, utils::IfstreamReader& reader) {
        reader.read(dicInfo.filename);
        reader.read(dicInfo.size);
        reader.read(dicInfo.dicType);
        reader.read(dicInfo.lsize);
        reader.read(dicInfo.rsize);
        reader.read(dicInfo.version);
    }

    // Dictonary class

    Dictionary::Dictionary(OptHandlerPtr opts) {
        nonTerminalToken.wcost = opts->getInt(L"non-terminal-cost");
    }

    void Dictionary::serialize(const String& file) const {
        LOG_INFOH(L"ENTER: file={}", file);
        try {
            utils::OfstreamWriter writer(file, true, false);
            if (writer.fail()) {
                LOG_ERROR_AND_THROW_RTE(L"Dictionary::serialize: can't open dictionary file: {}", file);
            }
            serializeDictionaryInfo(info, writer);
            dblAry->serialize(writer);
            writer.writeSerializable(tokens);
            features.serialize(writer);
        } catch (RuntimeException x) {
            LOG_ERROR_AND_THROW_RTE(L"Dictionary::serialize: can't write dictionary file: {}, caused by {}", file, x.getCause());
        }
        LOG_INFOH(L"LEAVE");
    }

    void Dictionary::deserialize(const String& file) {
        LOG_INFOH(L"ENTER: file={}", file);
        try {
            utils::IfstreamReader reader(file, true);
            if (reader.fail()) {
                LOG_ERROR_AND_THROW_RTE(L"can't open dictionary file: {}", file);
            }
            deserializeDictionaryInfo(info, reader);
            dblAry = DoubleArray::deserialize(reader);
            reader.readSerializable(tokens);
            features.deserialize(reader);
            // 交ぜ書きエントリに対するペナルティ付加は Tokenizer.lookup() で行う
            //int penalty = opts->getInt(L"maze-penalty", DEFAULT_MAZE_PENALTY);
            //LOG_INFOH(L"adjust MAZE cost: penalty={}", penalty);
            //for (auto iter = tokens.begin(); iter != tokens.end(); ++iter) {
            //    String feat = feature(*iter);
            //    if (utils::endsWith(feat, L"MAZE")) iter->wcost += penalty;
            //}
        } catch (...) {
            LOG_ERROR_AND_THROW_RTE(L"can't read dictionary file: {}", file);
        }
        LOG_INFOH(L"LEAVE");
    }
    /**
     * load dictionary
     */
    void Dictionary::load(const String& filepath) {
        LOG_INFOH(L"ENTER: filepath={}", filepath);

        deserialize(filepath);
        if (info.version != DIC_VERSION) {
            LOG_ERROR_AND_THROW_RTE(L"Dictionary::load: incompatible version: {}", info.version);
        }

        LOG_INFOH(L"LEAVE");
    }

    /**
     * 与えられた文字列の先頭部分にマッチするエントリ（複数可）を検索する
     */
    std::vector<std::tuple<std::vector<SafePtr<Token>>, size_t>> Dictionary::commonPrefixSearch(const String& key, bool allowNonTerminal) {
        LOG_DEBUG(L"ENTER: key={}", key);
        std::vector<std::tuple<std::vector<SafePtr<Token>>, size_t>> result;
        for (auto res : dblAry->commonPrefixSearch(key, allowNonTerminal)) {
            result.push_back({ getTokenSeq(res.value), (size_t)res.length });
        }
        LOG_DEBUG(L"LEAVE: key={}, result.size()={}", key, result.size());
        return result;
    }

    /**
     *  与えられた文字列にマッチするエントリを検索する
     */
    std::vector<SafePtr<Token>> Dictionary::exactMatchSearch(const String& key) {
        auto res = dblAry->exactMatchSearch(key);
        if (res.length <= 0) {
            LOG_ERROR_AND_THROW_RTE(L"Dictionary::exactMatchSearch: cannot find UNK category: {}", key);
        }
        return getTokenSeq(res.value);
    }

    //DictionaryInfo Dictionary::info() const {
    //    return info;
    //}

    String Dictionary::feature(const Token& t) const {
        LOG_DEBUG(L"featurePtr={}, Feature={}", t.featurePtr, t.featurePtr < NON_TERMINAL_FEATURE_PTR ? features.unpack(t.featurePtr) : nonTerminalFeature);
        return t.featurePtr < NON_TERMINAL_FEATURE_PTR ? features.unpack(t.featurePtr) : nonTerminalFeature;
    }

    std::vector<String> Dictionary::getFeatures() const {
        return features.list();
    }

    std::vector<SafePtr<Token>> Dictionary::getTokenSeq(int resultVal) {
        Vector<SafePtr<Token>> result;
        if (resultVal >= 0) {
            auto num = (unsigned int)resultVal & 0xff;
            auto idx = (unsigned int)resultVal >> 8;
            for (size_t i = 0; i < num && idx + i < tokens.size(); ++i) result.push_back(&tokens[idx + i]);
        } else {
            result.push_back(&nonTerminalToken);
        }
        return result;
    }


        // デバッグ用検索
    Vector<String> Dictionary::debugSearch(StringRef key) {
        Vector<String> result;
        auto tokens = exactMatchSearch(key);
        for (const auto& t : tokens) {
            result.push_back(std::format(L"{},{},{},{},{}", key, t->lcAttr, t->rcAttr, t->wcost, feature(*t)));
        }
        return result;
    }

} // namespace dict

// Dictionary Compiler

