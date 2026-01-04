#pragma once

#include "std_utils.h"
#include "file_utils.h"

namespace darts
{
    const int ERROR_NO_ENTRY = -1;
    const int ERROR_DUP_ENTRY = -2;
    const int ERROR_NOT_SORTED = -3;
    const int ERROR_NEG_VALUE = -4;
    const int ERROR_UNKNOWN = -5;

    const size_t ALIGN_SIZE = 1024;

    const bool DUP_ENTRY_NOT_ALLOWED = false;

    // ルート開始位置
    const int ROOT_INDEX = 1;

    // 重複エントリ末尾
    const int LAST_ENTRY = INT_MIN;

    // 結果を格納する
    struct ResultPair {
        int value;
        int length;

    public:
        ResultPair(int val = 0, int len = 0)
            : value(val), length(len) {
        }

        void set(int val = 0, int len = 0) {
            value = val;
            length = len;
        }
    };

    /**
     * ダブル配列 (Double Array) インタフェース
     */
    class DoubleArray {
    public:
        virtual size_t size() = 0;
        virtual size_t nonzero_size() = 0;
        virtual int resize(int pos) = 0;

        // 完全一致検索
        virtual ResultPair exactMatchSearch(String key) = 0;

        // 先頭部分一致検索
        virtual std::vector<ResultPair> commonPrefixSearch(String key, bool allowNonTerminal = false) = 0;

        virtual void serialize(utils::OfstreamWriter& writer) = 0;

        static UniqPtr<DoubleArray> deserialize(utils::IfstreamReader& reader);
    };

    using ProgressFunc = Function<void(size_t, size_t)>;

    // DoubleArray 構築用ヘルパー
    class DoubleArrayBuildHelper {
    public:
        static std::shared_ptr<DoubleArrayBuildHelper> createHelper();

    public:
        virtual std::shared_ptr<DoubleArray> createDoubleArray(
            const std::vector<String>& keys,
            const std::vector<int>& values) = 0;

        virtual const std::vector<int>& baseArray() = 0;
        virtual const std::vector<int>& checkArray() = 0;
        virtual int error() = 0;
        virtual int errorIndex() = 0;
    };

    //SharedPtr<DoubleArray> createDoubleArray(
    //    const Vector<String>& keys,
    //    const Vector<int>& values,
    //    const bool dbg = false);
}

#define DoubleArrayPtr SharedPtr<darts::DoubleArray>
#define DoubleArrayBuildHelperPtr SharedPtr<darts::DoubleArrayBuildHelper>
