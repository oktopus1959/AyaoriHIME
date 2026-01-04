#pragma once

#include "string_utils.h"
#include "misc_utils.h"
#include "ptr_utils.h"
#include "RangeString.h"
#include "CharInfo.h"
#include "exception.h"
#include "OptHandler.h"
#include "Logger.h"

using namespace util;

namespace analyzer
{
    /**
     *  Serialize/Deserialize されるカテゴリーテーブルなど
     */
    class CategoryTable /* Serializable */ {
    private:
        std::map<String, PVT> catMap;
        std::vector<String> catAry;
        std::vector<PVT> table;

    public:
        CategoryTable() { }

        void setTables(const std::map<String, PVT>& map, const std::vector<String>& cats, const std::vector<PVT>& tbl)
        {
            std::copy(map.begin(), map.end(), std::inserter(catMap, catMap.end()));
            std::copy(cats.begin(), cats.end(), std::inserter(catAry, catAry.end()));
            std::copy(tbl.begin(), tbl.end(), std::inserter(table, table.end()));
        }

        void deserialize(StringRef path);

        void serialize(StringRef path);

        size_t size() const {
            return catAry.size();
        }

        int categoryId(StringRef cat) const {
            return utils::find_index(catAry, cat);
        }

        StringRef categoryName(int id) const {
            return catAry[id];
        }

        const std::vector<String>& categoryNames() const {
            return catAry;
        }

        PVT packed_val(wchar_t ch) const {
            return table[ch];
        }

        CharInfo getCharInfo(wchar_t ch) const {
            return CharInfo(packed_val(ch));
        }

        CharInfo getCharInfo(StringRef name) const {
            return CharInfo(utils::safe_get(catMap, name, (PVT)0));
        }

        // for unit test
        const std::map<String, PVT>& getMap() const {
            return catMap;
        }

        const std::vector<PVT>& getTable() const {
            return table;
        }
    };

    /**
     * Character property class
     */
    class CharProperty {
        DECLARE_CLASS_LOGGER;

        CategoryTable _catMapTbl;

    public:
        const CategoryTable& getCategoryMapTable() const {
            return _catMapTbl;
        }

        /**
         * 文字の最大値
         */
        static const size_t MAX_CHAR = 0xffff;

        /**
         * Range class
         */
        struct CharRange {
            int low = 0;
            int high = 0;
            std::vector<String> cats;
        };

        String whatLog() {
            return _T("not implemented"); //WhatLog();
        }
        void close() {}

        size_t size() {
            return _catMapTbl.size();
        }

        // this function must be rewritten.
        void set_charset(StringRef charset) { }

        int id(StringRef s) {
            return _catMapTbl.categoryId(s);
        }

        const std::vector<String>& names() {
            return _catMapTbl.categoryNames();
        }

        StringRef name(int i) {
            return _catMapTbl.categoryName(i);
        }

        /**
         * 異なる型の字種が見つかるまで探索する
         */
        size_t seekToOtherType(RangeStringPtr str, CharInfo c, SafePtr<CharInfo> fail = 0) {
            return seekToOtherType2(str, c.packed_val, fail);
        }

        /**
         * 異なる型の字種が見つかるまで探索する
         */
        size_t seekToOtherType2(RangeStringPtr str, PVT pv, SafePtr<CharInfo> fail = 0) {
            CharInfo tmpCi;
            auto ci = fail ? fail : &tmpCi;
            size_t p = str->begin();
            for (; p < str->end(); ++p) {
                auto ch = str->charAt(p);
                ci->packed_val = _catMapTbl.packed_val(ch);
                if (!CharInfo::isSameKind(pv, ci->packed_val)) break;
            }
            return p;
        }

        bool isSameKind(CharInfo ci, wchar_t ch) {
            return CharInfo::isSameKind(ci.packed_val, _catMapTbl.packed_val(ch));
        }

        CharInfo getCharInfo(wchar_t chr) {
            return _catMapTbl.getCharInfo(chr);
        }

        CharInfo getCharInfo(StringRef name) {
            return _catMapTbl.getCharInfo(name);
        }

        /**
         * Compile char property
         * @param cfile 文字種定義ファイル (e.g. char.def)
         * @param ufile 未知語定義ファイル (e.g. unk.def)
         * @param ofile 出力ファイル (e.g. char.bin)
         */
        static UniqPtr<CategoryTable> compile(StringRef cfile, StringRef ufile, StringRef ofile);

        /**
         * Read binary category table
         */
        bool loadBinary(OptHandlerPtr opts);

        /**
         * Read binary category table
         */
        bool loadBinary(StringRef path = _T(""));
    };

} // namespace analyzer