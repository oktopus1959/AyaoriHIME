#pragma once

#include "string_utils.h"
#include "file_utils.h"
#include "OptHandler.h"
#include "Logger.h"
#include "exception.h"

#if 0
using namespace util;

#include "node/Node.h"

using Node = node::Node;

namespace analyzer {
    /**
     * 属性間接続コスト表のクラス (Connector)
     * @param filename 接続コストマトリクスファイル (matrix.bin)をロードする。 必須。これが無いと例外を送出する。
     */
    class Connector {
        DECLARE_CLASS_LOGGER;

        struct Matrix;              // 属性間接続コスト行列
        UniqPtr<Matrix> pMatrix;

        //OptHandlerPtr opts;

    public:
        //static Matrix createMatrix(size_t lsiz, size_t rsiz) {
        //    return Matrix{ std::vector<short>(lsiz * rsiz), lsiz, rsiz };
        //}

        //static void compile(StringRef ifile, StringRef pfile, StringRef ofile);

        // compile の補助関数; UnitTest用に public に置いてある
        //static SharedPtr<Connector> _compile(StringRef ifile, StringRef pfile);

        static int _getLeftAttributesWithEOSConnectionPenalty(StringRef pfile, std::set<size_t>& attrs);

        static std::tuple<size_t, size_t> readMatrixSize(StringRef filename);

    private:
        Connector(size_t lsize, size_t rsize);

    public:
        Connector();

        Connector(StringRef filename);

        Connector(OptHandlerRef opts);

        // Matrix が宣言のみのため、pMatrix の破棄を実装部でやるために必要
        ~Connector();

        void deserialize(StringRef path);

        void serialize(StringRef path);


        // val whatLog = WhatLog()

        void close() {}
        void clear() {}

        // 左側ノードの右接続属性の数を取得
        size_t left_size() const;

        // 右側ノードの左接続属性の数を取得
        size_t right_size() const;

        // 接続コストを取得
        int connectionCost(node::WordType leftWordType, node::WordType rigthWordType) const;

        // 接続コストを取得(lNode: 左側ノード, rNode: 右側ノード)
        int cost(const Node& lNode, const Node& rNode) const;

        bool is_valid(int lid, int rid) const;

        bool equalsTo(const Connector& cc) const;
    };

} // namespace analyzer
#endif
