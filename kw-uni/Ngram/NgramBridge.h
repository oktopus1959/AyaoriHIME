#pragma once

#include "string_utils.h"

namespace NgramBridge {
    void ngramInitialize();

    void ngramFinalize();

    void ngramReopenUserDics();

    int ngramCompileAndLoadUserDic(StringRef dicDir, StringRef filePath);

    void ngramSetLogLevel(int logLevel);

    void ngramSaveLog();

    // リアルタイムNgram辞書のパラメータ設定
    // @param minNgramLen 最小N-gram長
    // @param maxNgramLen 最大N-gram長
    // @param maxBonusPoint N-gramに与えるボーナスポイントの最大値
    // @param bonusPointFactor ボーナスポイントに対するボーナス係数
    void setRealtimeDictParameters(size_t minNgramLen, size_t maxNgramLen, int maxBonusPoint, int bonusPointFactor);

    // リアルタイムNgram辞書のロード
    int loadRealtimeDict(StringRef ngramFilePath);

    // ユーザーNgram辞書のロード
    int loadUserNgram(StringRef ngramFilePath);

    // リアルタイムNgramファイルの保存
    void saveRealtimeDict(StringRef ngramFilePath, int rotationNum);

    // リアルタイムNgramエントリの更新
    int updateRealtimeEntry(const String& word, int delta);

    int ngramCalcCost(const MString& str, const std::vector<MString>& tempDictEntries, std::vector<MString>& ngrams, bool needNgrams);
}
