#include "Logger.h"

#include "SimpleDicStateBase.h"

#include "Settings.h"
#include "StrokeMerger/Merger.h"

#if 0
#undef _LOG_DEBUGH
#define _LOG_DEBUGH LOG_INFOH
#endif

namespace {

    // -------------------------------------------------------------------
    // 簡易辞書検索状態基底クラス
    class SimpleDicStateBaseImpl : public SimpleDicStateBase {
        DECLARE_CLASS_LOGGER;

        const Node* pNode_ = 0;

        String BaseName;

    protected:
        // 履歴入力候補のリスト
        //SimpleDicCandidates histCands;

    public:
        // コンストラクタ
        SimpleDicStateBaseImpl(const Node* pN)
            : pNode_(pN), BaseName(logger.ClassNameT()) {
            LOG_DEBUGH(_T("CALLED"));
        }

        ~SimpleDicStateBaseImpl() override { };

    public:
        // 履歴検索文字列の遡及ブロッカーをセット
        void setBlocker() override {
            _LOG_DEBUGH(_T("CALLED: {}"), BaseName);
            STATE_COMMON->SetAppendBackspaceStopperFlag();
            STATE_COMMON->SetHistoryBlockFlag();
            STATE_COMMON->ClearDecKeyCount();
        }

        // 選択された履歴候補を出力(これが呼ばれた時点で、すでにキーの先頭まで巻き戻すように plannedNumBS が設定されていること)
        void setOutString(const SimpleDicResult& result, MStringResult& resultStr) override {
            _LOG_DEBUGH(_T("ENTER: result.OrigKey={}, result.Key={}, result.Word={}, keyLen={}, wildKey={}, prevOutStr={}, prevKey={}, plannedNumBS={}"), \
                to_wstr(result.OrigKey), to_wstr(result.Key), to_wstr(result.Word), result.KeyLen(), result.WildKey, \
                to_wstr(STROKE_MERGER_NODE->GetPrevOutString()), to_wstr(STROKE_MERGER_NODE->GetPrevKey()), resultStr.numBS());

            MString outStr = result.Word;
            MString outKey = result.Key;
            if (outStr.empty()) {
                // 未選択状態だったら、出力文字列を元に戻す
                outKey = STROKE_MERGER_NODE->GetPrevKey();
                outStr = STROKE_MERGER_NODE->GetPrevOutString();
                if (outStr.empty()) outStr = outKey;
            } else {
                size_t pos = outStr.find(VERT_BAR);     // '|' を含むか
                _LOG_DEBUGH(_T("pos={}, histMapKeyMaxLength={}"), pos, SETTINGS->histMapKeyMaxLength);
                if (pos <= SETTINGS->histMapKeyMaxLength) {
                    // histMap候補
                    if (pos + 1 < outStr.size() && outStr[pos + 1] == VERT_BAR) ++pos;  // '||' だったら1つ進める(HistoryDicで既に対処済みなので、多分、ここでは不要のはず)
                    if (pos + 1 < outStr.size() && outStr[pos + 1] == HASH_MARK) ++pos;  // '|#' だったら1つ進める(# はローマ字変換の印)
                    outStr = utils::safe_substr(outStr, pos + 1);
                    _LOG_DEBUGH(_T("histMap: outStr={}, outKey={}"), to_wstr(outStr), to_wstr(outKey));
                }
                if (outKey.size() < result.OrigKey.size()) {
                    // 変換キーが元キーよりも短い場合(「あわなだ」が元キーで「わなだ」が変換キーのケース)
                    auto leadStr = result.OrigKey.substr(0, result.OrigKey.size() - outKey.size());
                    outStr = leadStr + outStr;
                    outKey = leadStr + outKey;
                    _LOG_DEBUGH(_T("histMap: leadStr Appended: leadStr={}"), to_wstr(leadStr));
                }
            }
            _LOG_DEBUGH(_T("outStr={}, outKey={}"), to_wstr(outStr), to_wstr(outKey));

            resultStr.setResult(outStr);
            STROKE_MERGER_NODE->SetPrevHistState(outStr, outKey);

            //_LOG_DEBUGH(_T("prevOutString={}, isPrevHistKeyUsed={}"), to_wstr(STROKE_MERGER_NODE->GetPrevOutString()), STROKE_MERGER_NODE->IsPrevHistKeyUsed());
            _LOG_DEBUGH(_T("LEAVE: prevOutString={}"), to_wstr(STROKE_MERGER_NODE->GetPrevOutString()));
        }

        // 前回の履歴検索の出力と現在の出力文字列(改行以降)の末尾を比較し、同じであれば前回の履歴検索のキーを取得する
        // この時、出力スタックは、キーだけを残し、追加出力部分は巻き戻し予約される(numBackSpacesに値をセット)
        // 前回が空キーだった場合は、返値も空キーになるので、STROKE_MERGER_NODE->PrevKeyLen == 0 かどうかで前回と同じキーであるか否かを判断すること
        // ここに来る場合には、以下の3つの状態がありえる:
        // ①まだ履歴検索がなされていない状態
        // ②検索が実行されたが、出力文字列にはキーだけが表示されている状態
        // ③横列のどれかの候補が選択されて出力文字列に反映されている状態
        MString getLastHistKeyAndRewindOutput(MStringResult& resultStr) override {
            // 前回の履歴検索の出力
            //bool bPrevHistUsed = STROKE_MERGER_NODE->IsPrevHistKeyUsed();
            const auto& prevKey = STROKE_MERGER_NODE->GetPrevKey();
            const auto& prevOut = STROKE_MERGER_NODE->GetPrevOutString();
            //_LOG_DEBUGH(_T("isPrevHistUsed={}, prevOut={}, prevKey={}"), bPrevHistUsed, to_wstr(prevOut), to_wstr(prevKey));
            _LOG_DEBUGH(_T("prevOut={}, prevKey={}"), to_wstr(prevOut), to_wstr(prevKey));

            if (prevKey.empty()) {
                // ①まだ履歴検索がなされていない状態
                // empty key を返す
                _LOG_DEBUGH(_T("NOT YET HIST USED"));
            } else if (prevOut.empty()) {
                // ②検索が実行されたが、出力文字列にはキーだけが表示されている状態
                _LOG_DEBUGH(_T("CURRENT: SetOutString(str={}, numBS={})"), to_wstr(prevKey), prevKey.size());
                resultStr.setResult(prevKey, (int)(prevKey.size()));
                STROKE_MERGER_NODE->SetPrevHistState(prevKey, prevKey);
                _LOG_DEBUGH(_T("CURRENT: prevKey={}"), to_wstr(prevKey));
            } else {
                // ③横列のどれかの候補が選択されて出力文字列に反映されている状態
                _LOG_DEBUGH(_T("REVERT and NEW HIST: SetOutString(str={}, numBS={})"), to_wstr(prevKey), prevOut.size());
                resultStr.setResult(prevKey, (int)(prevOut.size()));
                STROKE_MERGER_NODE->SetPrevHistState(prevKey, prevKey);
                _LOG_DEBUGH(_T("REVERT and NEW HIST: prevKey={}"), to_wstr(prevKey));
            }

            _LOG_DEBUGH(_T("last Japanese key={}"), to_wstr(prevKey));
            return prevKey;
        }

        // 簡易辞書候補を横列鍵盤にセットする
        void setCandidatesVKB(const std::vector<MString>& cands, const MString& key) override {
            _LOG_DEBUGH(_T("ENTER: cands.size()={}, key={}"), cands.size(), to_wstr(key));
            auto mark = pNode_->getString();
            std::vector<MString> words;
            size_t q = std::min(cands.size(), SETTINGS->histHorizontalCandMax);
            for (size_t i = 0; i < q; ++i) {
                words.push_back(utils::str_shrink(cands[i], 20));
            }
            STATE_COMMON->SetVirtualKeyboardStrings(VkbLayout::Horizontal, mark + utils::str_shrink(key, 5), words);

            if (SIMPLE_DIC_CAND->GetSelectPos() >= 0) STATE_COMMON->SetDontMoveVirtualKeyboard();

            _LOG_DEBUGH(_T("LEAVE"));
        }

    };
    DEFINE_CLASS_LOGGER(SimpleDicStateBaseImpl);
} // namespace

SimpleDicStateBase* SimpleDicStateBase::createInstance(const Node* pN) {
    return new SimpleDicStateBaseImpl(pN);
}
