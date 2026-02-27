#pragma once

#include "std_utils.h"
#include "ptr_utils.h"
#include "util/PackedString.h"
#include "darts/DoubleArray.h"
#include "DictionaryInfo.h"
#include "analyzer/Token.h"
#include "OptHandler.h"
#include "Logger.h"

namespace dict {

    // 非終端トークンのコスト
    const int NON_TERMINAL_DEFAULT_COST = 5000;

    // Dictonary class
    class Dictionary {
        DECLARE_CLASS_LOGGER;

        //OptHandlerPtr opts;

        DictionaryInfo _info;
        DoubleArrayPtr _dblAry;

        std::vector<analyzer::Token> _tokens;

        // 非終端トークン (コストはオプション non-terminal-cost で指定可能)
        analyzer::Token nonTerminalToken = analyzer::Token(NON_TERMINAL_DEFAULT_COST);

        std::vector<SafePtr<analyzer::Token>> getTokenSeq(int resultVal);

        void serialize(const String& file) const ;

        void deserialize(const String& file);

    public:
        // Constructor
        Dictionary(OptHandlerPtr opts);

    public:
        /**
         * compile dictionary
         */
        static void compile(OptHandlerPtr opts, const Vector<String>& dics, StringRef outputfile);
        static void compileUserDict(OptHandlerPtr opts, const Vector<String>& dic_lines, StringRef outputfile);

    public:
        /**
         * load dictionary
         */
        void load(const String& filepath);

        /**
         * 与えられた文字列の先頭部分にマッチするエントリ（複数可）を検索する
         */
        std::vector<std::tuple<std::vector<SafePtr<analyzer::Token>>, size_t>> commonPrefixSearch(const String& key, bool allowNonTerminal = false);

        /**
         *  与えられた文字列にマッチするエントリを検索する
         */
        std::vector<SafePtr<analyzer::Token>> exactMatchSearch(const String& key);

        // デバッグ用検索
        Vector<String> debugSearch(StringRef key);

        const DictionaryInfo& getInfo() const {
            return _info;
        }

        const String& filename() const {
            return _info.filename;
        }

        String charset() {
            return L"utf-8";
        }

        int version() const {
            return _info.version;
        }

        size_t size() const {
            return _info.size; // lexsize_
        }

        int getType() const {
            return _info.dicType; // type_
        }

    };

} // namespace dict

using DictionaryPtr = SharedPtr<dict::Dictionary>;

