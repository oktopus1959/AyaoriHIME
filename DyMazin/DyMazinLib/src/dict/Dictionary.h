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
    const int NON_TERMINAL_DEFAULT_COST = 10000;

    // 非終端トークンの featurePtr
    const size_t NON_TERMINAL_FEATURE_PTR = String::npos;

    // 非終端feature
    const String nonTerminalFeature = L"非終端,*";
    //const String nonTerminalFeature = L"*\t非終端,*";

    // Dictonary class
    class Dictionary {
        DECLARE_CLASS_LOGGER;

        //OptHandlerPtr opts;

        DictionaryInfo info;
        DoubleArrayPtr dblAry;

        std::vector<analyzer::Token> tokens;
        // 非終端トークン (コストはオプション non-terminal-cost で指定可能)
        analyzer::Token nonTerminalToken = analyzer::Token(1285, 1285, NON_TERMINAL_DEFAULT_COST, NON_TERMINAL_FEATURE_PTR);

        util::PackedString features;

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
            return info;
        }

        const String& filename() const {
            return info.filename;
        }

        String charset() {
            return L"utf-8";
        }

        int version() const {
            return info.version;
        }

        size_t size() const {
            return info.size; // lexsize_
        }

        int getType() const {
            return info.dicType; // type_
        }

        size_t lsize() const {
            return info.lsize;
        }

        size_t rsize() const {
            return info.rsize;
        }

        String feature(const analyzer::Token& t) const;

        std::vector<String> getFeatures() const;

        //var whatLog = new WhatLog

        bool isCompatible(const Dictionary& d) {
            return version() == d.version() && lsize() == d.lsize() && rsize() == d.rsize();
        }

    };

} // namespace dict

using DictionaryPtr = SharedPtr<dict::Dictionary>;

