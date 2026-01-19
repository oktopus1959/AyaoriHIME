#include "Logger.h"
#include "file_utils.h"

#include "settings.h"
#include "StateCommonInfo.h"

#include "Lattice.h"
#include "Lattice2_Common.h"
#include "Lattice2_CandidateString.h"
#include "Lattice2_Kbest.h"
#include "Lattice2_Morpher.h"
#include "Lattice2_Ngram.h"

namespace lattice2 {
    DEFINE_LOCAL_LOGGER(lattice);

    //--------------------------------------------------------------------------------------
    // Lattice
    class LatticeImpl : public Lattice2 {
        DECLARE_CLASS_LOGGER;
    private:
        // 打鍵開始位置
        int _startStrokeCount = 0;

        // K-best な文字列を格納する
        std::unique_ptr<KBestList> _kBestList;

        // 前回生成された文字列
        MString _prevOutputStr;

        // 前回生成された文字列との共通する先頭部分の長さ
        size_t calcCommonPrefixLenWithPrevStr(const MString& outStr) {
            LOG_DEBUGH(_T("ENTER: outStr={}, prevStr={}"), to_wstr(outStr), to_wstr(_prevOutputStr));
            size_t n = 0;
            while (n < outStr.size() && n < _prevOutputStr.size()) {
                if (outStr[n] != _prevOutputStr[n]) break;
                ++n;
            }
            LOG_DEBUGH(_T("LEAVE: commonLen={}"), n);
            return n;
        }

        Deque<String> _debugLogQueue;

        String formatStringOfWordPieces(const std::vector<WordPiece>& pieces) {
            return utils::join(utils::select<String>(pieces, [](WordPiece p){return p.debugString();}), _T("|"));
        }

        // すべての単語素片が1文字で、それが漢字・ひらがな・カタカナ以外か
        bool areAllPiecesNonJaChar(const std::vector<WordPiece>& pieces) {
            for (const auto iter : pieces) {
                MString s = pieces.front().getString();
                //LOG_INFO(_T("s: len={}, str={}"), s.size(), to_wstr(s));
                if (s.size() != 1 || utils::is_japanese_char_except_nakaguro(s.front())) {
                    //LOG_INFO(_T("FALSE"));
                    return false;
                }
            }
            return true;
        }

    public:
        // コンストラクタ
        LatticeImpl() : _kBestList(KBestList::createKBestList()) {
            _LOG_DETAIL(_T("CALLED: Constructor"));
        }

        // デストラクタ
        ~LatticeImpl() override {
            _LOG_DETAIL(_T("CALLED: Destructor"));
            clear();
        }

        void clearAll() override {
            _LOG_DETAIL(_T("ENTER"));
            _startStrokeCount = 0;
            _prevOutputStr.clear();
            _kBestList->clearKbests(true);
            _LOG_DETAIL(_T("LEAVE"));
        }

        void syncBaseString(const MString& base) override {
            _LOG_DETAIL(_T("ENTER: base={}"), to_wstr(base));
            _startStrokeCount = 0;
            _prevOutputStr = base;
            _kBestList->setBaseString(base);
            _LOG_DETAIL(_T("LEAVE"));
        }

        void clear() override {
            _LOG_DETAIL(_T("ENTER"));
            _startStrokeCount = 0;
            _prevOutputStr.clear();
            _kBestList->clearKbests(false);
            _LOG_DETAIL(_T("LEAVE"));
        }

        void removeOtherThanKBest() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->removeOtherThanKBest();
        }

        void removeOtherThanFirst() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->removeOtherThanFirst();
        }

        void setKanjiXorHiraganaPreferredNextCands(bool bKanji) override {
            _LOG_DETAIL(_T("ENTER"));
            _kBestList->setKanjiXorHiraganaPreferredNextCands(bKanji);
            if (_startStrokeCount == 1) _startStrokeCount = 0;  // 先頭での漢字優先なら、0 クリアしておく(この後、clear()が呼ばれるので、それと状態を合わせるため)
            _LOG_DETAIL(_T("LEAVE"));
        }

        void clearKanjiXorHiraganaPreferredNextCands() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->clearKanjiXorHiraganaPreferredNextCands();
        }

        bool isEmpty() override {
            //_LOG_DETAIL(_T("CALLED: isEmpty={}"), _kBestList->isEmpty());
            return _kBestList->isEmpty();
        }

        // 先頭候補を取得する
        MString getFirst() const override {
            _LOG_DETAIL(_T("CALLED"));
            return _kBestList->getFirst();
        }

        // 先頭候補を最優先候補にする
        void selectFirst() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->selectFirst();
        }

        // 次候補を選択する
        void selectNext() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->selectNext();
        }

        // 前候補を選択する
        void selectPrev() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->selectPrev();
        }

        // 候補選択による、リアルタイムNgramの蒿上げと抑制
        void raiseAndLowerByCandSelection() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->raiseAndLowerByCandSelection();
        }

        // 部首合成
        void updateByBushuComp() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->updateByBushuComp();
        }

        // 交ぜ書き変換
        void updateByMazegaki() override {
            _LOG_DETAIL(_T("CALLED"));
            _kBestList->updateByMazegaki();
        }

        bool hasMultiCandidates() const override {
            if (_kBestList->selectedCandPos() >= 0) return true;

            std::vector<MString> candStrings = _kBestList->getCandStringsInSelectedBlock();
            return candStrings.size() > 1;
        }

    public:
        // 単語素片リストの追加(単語素片が得られなかった場合も含め、各打鍵ごとに呼び出すこと)
        // 単語素片(WordPiece): 打鍵後に得られた出力文字列と、それにかかった打鍵数
        LatticeResult addPieces(const std::vector<WordPiece>& pieces, bool useMorphAnalyzer, bool strokeBack, bool bKatakanaConversion) override {
            _LOG_DETAIL(_T("ENTER: pieces: {}, bMulti={}, useMorphAnalyzer={}"), formatStringOfWordPieces(pieces), SETTINGS->multiCandidateMode, useMorphAnalyzer);
            int totalStrokeCount = (int)(STATE_COMMON->GetTotalDecKeyCount());
            if (_startStrokeCount == 0) _startStrokeCount = totalStrokeCount;
            int currentStrokeCount = totalStrokeCount - _startStrokeCount + 1;

            //LOG_DEBUGH(_T("ENTER: currentStrokeCount={}, pieces: {}\nkBest:\n{}"), currentStrokeCount, formatStringOfWordPieces(pieces), _kBestList->debugString());
            _LOG_DETAIL(_T("INPUT: _kBestList.size={}, _origFirstCand={}, totalStroke={}, currentStroke={}, strokeBack={}, rollOver={}, pieces: {}"),
                _kBestList->size(), _kBestList->origFirstCand(), totalStrokeCount, currentStrokeCount, strokeBack, STATE_COMMON->IsRollOverStroke(), formatStringOfWordPieces(pieces));

            _LOG_DETAIL(L"CURRENT:\nkBest:\n{}", _kBestList->debugCandidates(10));

            if (pieces.empty()) {
                // pieces が空になるのは、同時打鍵の途中の状態などで、文字が確定していない場合
                _LOG_DETAIL(L"LEAVE: emptyResult");
                return LatticeResult::emptyResult();
            }

            //if (pieces.size() == 1) {
            //    auto s = pieces.front().getString();
            //    if (s.size() == 1 && utils::is_punct(s[0])) {
            //        // 前回の句読点から末尾までの出力文字列に対して Ngram解析を行う
            //        _LOG_DETAIL(L"CALL lattice2::updateRealtimeNgram()");
            //        lattice2::updateRealtimeNgram();
            //    }
            //}
            // フロントエンドから updateRealtimeNgram() を呼び出すので、ここではやる必要がない

            //if (kanjiPreferredNext) {
            //    _LOG_DETAIL(L"KANJI PREFERRED NEXT");
            //    // 先頭候補だけを残す
            //    _kBestList->removeOtherThanFirst();
            //    // 次のストロークを漢字優先とする
            //    _kBestList->setKanjiPreferredNextCands();
            //    if (_startStrokeCount == 1) _startStrokeCount = 0;  // 先頭での漢字優先なら、0 クリアしておく(この後、clear()が呼ばれるので、それと状態を合わせるため)
            //}

            _LOG_DETAIL(L"_kBestList.size={}", _kBestList->size());

            //// すべての単語素片が1文字で、それが漢字・ひらがな・カタカナ以外だったら、現在の先頭候補を優先させる
            //if (!pieces.empty() && areAllPiecesNonJaChar(pieces)) {
            //    _LOG_DETAIL(L"ALL PIECES NON JA CHAR");
            //    selectFirst();
            //}
            //_LOG_DETAIL(L"_kBestList.size={}", _kBestList->size());

            // 候補リストの更新
            _kBestList->updateKBestList(pieces, useMorphAnalyzer, currentStrokeCount, strokeBack, bKatakanaConversion);

            //LOG_DEBUGH(L"G:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            //LOG_DEBUGH(_T(".\nresult kBest:\n{}"), pKBestList->debugString());
            size_t numBS = 0;
            MString outStr = _kBestList->getTopString();
            size_t commonLen = calcCommonPrefixLenWithPrevStr(outStr);
            numBS = _prevOutputStr.size() - commonLen;
            _prevOutputStr = outStr;
            outStr = utils::safe_substr(outStr, commonLen);

            _LOG_DETAIL(_T("OUTPUT: {}, numBS={}\n\n{}"), to_wstr(outStr), numBS, _kBestList->debugKBestString());
            if (IS_LOG_DEBUGH_ENABLED) {
                while (_debugLogQueue.size() >= 10) _debugLogQueue.pop_front();
                _debugLogQueue.push_back(std::format(L"========================================\nENTER: currentStrokeCount={}, pieces: {}\n",
                    currentStrokeCount, formatStringOfWordPieces(pieces)));
                if (pieces.back().numBS() <= 0) {
                    _debugLogQueue.push_back(std::format(L"\n{}\nOUTPUT: {}, numBS={}\n\n", _kBestList->debugKBestString(SETTINGS->multiStreamBeamSize), to_wstr(outStr), numBS));
                }
            }

            //LOG_DEBUGH(L"H:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            // 解候補を仮想鍵盤に表示する
            //std::vector<MString> candStrings = _kBestList->getTopCandStrings();
            std::vector<MString> candStrings = _kBestList->getCandStringsInSelectedBlock();
            if (candStrings.size() > 1 || _kBestList->selectedCandPos() >= 0) {
                STATE_COMMON->SetVirtualKeyboardStrings(VkbLayout::MultiStreamCandidates, EMPTY_MSTR, candStrings);
                STATE_COMMON->SetWaitingCandSelect(_kBestList->selectedCandPos());
            }
            //LOG_DEBUGH(L"I:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));

            _LOG_DETAIL(_T("LEAVE:\nkanjiPreferredNextCands={}\nkBest:\n{}"), _kBestList->kanjiXorHiraganaPreferredNextCandsDebug(),  _kBestList->debugCandidates(10));
            return LatticeResult(outStr, numBS);
        }

        // 融合候補の表示
        void saveCandidateLog() override {
            LOG_INFO(_T("ENTER"));
            String result;
            while (!_debugLogQueue.empty()) {
                result.append(_debugLogQueue.front());
                _debugLogQueue.pop_front();
            }
            LOG_INFO(L"result: {}", result);
            utils::OfstreamWriter writer(utils::joinPath(SETTINGS->rootDir, SETTINGS->mergerCandidateFile));
            if (writer.success()) {
                writer.writeLine(utils::utf8_encode(result));
                LOG_INFO(_T("result written"));
            }
            LOG_INFO(_T("LEAVE"));
        }

    };
    DEFINE_CLASS_LOGGER(LatticeImpl);

} // namespace lattice2

//--------------------------------------------------------------------------------------
DEFINE_CLASS_LOGGER(Lattice2);

std::unique_ptr<Lattice2> Lattice2::Singleton;

void Lattice2::createLattice() {
    LOG_INFOH(L"ENTER");
    lattice2::loadNgramFiles();
    lattice2::loadMazegakiPrefFile();
    Singleton.reset(new lattice2::LatticeImpl());
    LOG_INFOH(L"LEAVE");
}

void Lattice2::reloadNgramFiles() {
    lattice2::loadNgramFiles();
}

void Lattice2::reloadUserCostFile() {
    //lattice2::loadCostAndNgramFile();
}

void Lattice2::updateRealtimeNgram(const MString& str) {
    lattice2::updateRealtimeNgram(str);
}

void Lattice2::increaseRealtimeNgramByGUI(const MString& str) {
    lattice2::increaseRealtimeNgram(str, true);
}

void Lattice2::decreaseRealtimeNgramByGUI(const MString& str) {
    lattice2::decreaseRealtimeNgram(str, true);
}

void Lattice2::saveRealtimeNgramFile() {
    lattice2::saveRealtimeNgramFile();
}

void Lattice2::saveLatticeRelatedFiles() {
    lattice2::saveRealtimeNgramFile();
    lattice2::saveMazegakiPrefFile();
}

void Lattice2::reloadGlobalPostRewriteMapFile() {
    lattice2::readGlobalPostRewriteMapFile();
}
