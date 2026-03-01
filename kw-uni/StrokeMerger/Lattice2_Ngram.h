#pragma once

#include "utils/string_type.h"
#include "settings.h"

namespace lattice2 {

    // リアルタイムNgram辞書のパラメータ設定
    void setRealtimeDictParameters();

    struct SelectedNgramPairBonus {
        MString ngramPair;       // <Positive Ngram>|<Negative Ngram>
        int bonusPoint = 0;

        bool isValid() const {
            return !ngramPair.empty() && bonusPoint != 0;
        }

        bool operator<(const SelectedNgramPairBonus& other) const {
            if (ngramPair < other.ngramPair) return true;
            if (other.ngramPair < ngramPair) return false;
            return bonusPoint < other.bonusPoint;
        }

        String debugString() const;
    };

    inline int calcNgramBonus(int bonusPoint) {
        // ボーナス点数をボーナス値に変換
        return bonusPoint * SETTINGS->ngramBonusPointFactor;
    }

    // コストとNgramファイルの読み込み
    void loadNgramFiles();

    // リアルタイムNgramの保存
    void saveRealtimeNgramFile();

    // リアルタイムNgramの更新
    void updateRealtimeNgram(const MString& str);

    // 候補選択による、SelectedNgramの更新
    void updateSelectedNgramByUserSelect(const MString& oldCand, const MString& newCand);

    // 指定された文字列に対して、そこに含まれるNgramに対応する選択Ngramペアボーナスを取得 (Ngramは、2~5文字)
    const std::set<SelectedNgramPairBonus> findNgramPairBonus(const MString& str);

    // Ngramコストの取得
    int getNgramCost(const MString& str, const std::vector<MString>& morphs, bool bUseGeta = true);

} // namespace lattice2

