#pragma once
#include "ptr_utils.h"
#include "Token.h"

namespace analyzer {
    namespace RealtimeDict {

        // リアルタイムNgram辞書のパラメータ設定
        // @param minNgramLen 最小N-gram長
        // @param maxNgramLen 最大N-gram長
        // @param maxBonusPoint N-gramに与えるボーナスポイントの最大値
        // @param bonusPointFactor ボーナスポイントに対するボーナス係数
        void setParameters(size_t minNgramLen, size_t maxNgramLen, int maxBonusPoint, int bonusPointFactor);

        // リアルタイムNgramエントリの更新
        // @param delta 加算する値
        // @return 更新後のエントリの値
        int updateEntry(const String& word, int delta);

        // リアルタイムNgramファイルのロード
        // @param ngramFilePath リアルタイムNgramファイルのパス
        // @return ロードされたエントリの数
        int loadNgramFile(StringRef ngramFilePath);

        // リアルタイムNgramファイルの保存
        // @param ngramFilePath リアルタイムNgramファイルのパス
        void saveNgramFile(StringRef ngramFilePath, int rotationNum);

        // 与えられた文字列の先頭部分にマッチするエントリ（複数可）を検索する
        // @param str 検索対象の文字列
        // @param pos 検索開始位置
        // @return マッチしたエントリのリスト。各エントリは、(Ngram長, ボーナス) のタプルで表される
        std::vector<int> commonPrefixSearch(const String& str, size_t pos);
    }
}
