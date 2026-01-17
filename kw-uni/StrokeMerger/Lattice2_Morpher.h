#pragma once

#include "Lattice2_CandidateString.h"

//#include "Lattice.h"

#define MAZE_TRANSLATION_FEATURE_DELIM   '|'
#define MAZE_MORPH_FEAT_DELIM   '/'

namespace lattice2 {

    // 交ぜ書き優先辞書の読み込み
    void loadMazegakiPrefFile();

    // 交ぜ書き優先度の取得
    int getMazegakiPreferenceBonus(const MString& mazeFeat);

    // 交ぜ書き優先度の更新
    void updateMazegakiPreference(const CandidateString& raised, const CandidateString& lowered);

    // 交ぜ書き優先辞書の書き込み
    void saveMazegakiPrefFile();

    // 非終端形態素か(Trieの中間ノードに相当)
    bool isNonTerminalMorph(const MString& morph);

    // 形態素解析コストの計算
    int calcMorphCost(const MString& s, std::vector<MString>& morphs);

} // namespace lattice2

