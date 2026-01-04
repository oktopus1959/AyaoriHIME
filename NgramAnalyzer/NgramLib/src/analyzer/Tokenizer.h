#pragma once

#include "std_utils.h"
#include "string_utils.h"
#include "OptHandler.h"
//#include "Lazy.h"

#include "node/Node.h"
#include "dict/Dictionary.h"

namespace analyzer {
    class Lattice;

    //-----------------------------------------------------------------------------
    /**
     * Tokenizer
     */
    class Tokenizer {
        DECLARE_CLASS_LOGGER;
    private:
        class Impl;
        UniqPtr<Impl> pImpl;

    public:
        Tokenizer(OptHandlerPtr opts);
        ~Tokenizer();   // これを省略すると、ここに暗黙のDestructorを生成しようとして、「Implの定義がない」というエラーになる

        //val whatLog = WhatLog()

        //Lazy<dict::DictionaryInfo> dictionary_info;
        const Vector<DictionaryPtr>& getDictionaries() const;

        /**
         * センテンスの指定位置からの辞書引き(これはかなりキモになるメソッド)
         * @param pos 辞書引き対象となる部分文字列の開始位置 (lattice.sentence の部分文字列でなければならない)
         * @param lattice センテンスの形態素解析した結果をラティス構造で管理するオブジェクト
         * @return 候補となる形態素ノードのリスト。空白文字しかなくて有効な形態素ノードが作成できない場合は、空リストのまま返す。
         */
        Vector<node::NodePtr> lookup(Lattice& lattice, size_t pos);

        /** ユーザー辞書の再オープン */
        void reload_userdics();

    }; // Tokenizer

} // namespace analyzer
