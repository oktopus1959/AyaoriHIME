#pragma once

#include "string_utils.h"

#include "StateCommonInfo.h"    // for VkbLayout
#include "SimpleDicCandidates.h"     // for SimpleDicResult
#include "Node.h"
#include "MStringResult.h"

// -------------------------------------------------------------------
// 簡易辞書検索状態基底クラス
class HistoryStateBase {
public:
    virtual ~HistoryStateBase() { };

public:
    // 履歴検索文字列の遡及ブロッカーをセット
    virtual void setBlocker() = 0;

    // 選択された履歴候補を出力(これが呼ばれた時点で、すでにキーの先頭まで巻き戻すように plannedNumBS が設定されていること)
    virtual void setOutString(const SimpleDicResult& result, MStringResult& resultStr) = 0;

    // 前回の履歴検索の出力と現在の出力文字列(改行以降)の末尾を比較し、同じであれば前回の履歴検索のキーを取得する
    virtual MString getLastHistKeyAndRewindOutput(MStringResult& resultStr) = 0;

    // 簡易辞書候補を横列鍵盤にセットする
    virtual void setCandidatesVKB(const std::vector<MString>& cands, const MString& key) = 0;

public:
    static HistoryStateBase* createInstance(const Node* pN);
};
