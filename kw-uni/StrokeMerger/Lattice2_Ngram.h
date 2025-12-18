#pragma once

//#include "Lattice2_CandidateString.h"
#include "utils/string_type.h"

namespace lattice2 {

    // コストとNgramファイルの読み込み
    void loadCostAndNgramFile(bool systemNgramFile = true, bool realtimeNgramFile = true);

    // リアルタイムNgramの保存
    void saveRealtimeNgramFile();

    // リアルタイムNgramの更新
    void updateRealtimeNgram(const MString& str);

    // リアルタイムNgramの蒿上げ
    void increaseRealtimeNgram(const MString& str, bool bByGUI = false);

    // リアルタイムNgramの抑制
    void decreaseRealtimeNgram(const MString& str, bool bByGUI = false);

    // 候補選択による、リアルタイムNgramの蒿上げと抑制
    void updateRealtimeNgramForDiffPart(const MString& oldCand, const MString& newCand);

    // Ngramコストの取得
    int getNgramCost(const MString& str, const std::vector<MString>& morphs);

} // namespace lattice2

