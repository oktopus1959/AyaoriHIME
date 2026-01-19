#include "Logger.h"
#include "string_utils.h"
#include "Settings.h"
#include "ErrorHandler.h"
#include "DeckeyUtil.h"

#include "Eisu.h"
#include "Zenkaku.h"

#include "FunctionNodeManager.h"

#include "History/HistoryStateBase.h"
#include "History/History.h"
#include "Merger.h"
#include "StrokeMergerHistoryResidentState.h"
#include "Lattice.h"

#define _LOG_INFOH LOG_INFO
#define _LOG_DETAIL LOG_DEBUGH
#if 1
#undef _LOG_INFOH
#undef _LOG_DETAIL
#undef LOG_INFO
#undef LOG_DEBUGH
#undef LOG_DEBUG
#define _LOG_INFOH LOG_INFOH
#define _LOG_DETAIL LOG_INFOH
#define LOG_INFO LOG_INFOH
#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFOH
#endif

namespace {

    // -------------------------------------------------------------------
    // 
    class StrokeStream : public State {
        DECLARE_CLASS_LOGGER;

        // 現在のストロークのデコーダキー
        int decKey = -1;

        size_t cntStroke = 0;

        String nextNodeType() const {
            return NODE_NAME(NextNodeMaybe());
        }

        MString convertHiraganaToKatakana(const MString& str) {
            MString result = str;
            for (size_t i = 0; i < result.size(); ++i) {
                mchar_t ch = (wchar_t)result[i];
                if (utils::is_hiragana(ch)) { result[i] = utils::hiragana_to_katakana(ch); }
            }
            return result;
        }

    public:
        // コンストラクタ
        StrokeStream() : cntStroke(STATE_COMMON->GetTotalDecKeyCount()-1) {
            LOG_DEBUGH(_T("CALLED: Default Constructor"));
            Initialize(_T("StrokeStream"), 0);
            MarkNecessary();
        }

        // コンストラクタ
        StrokeStream(StrokeTableNode* pRootNode) : StrokeStream() {
            LOG_DEBUGH(_T("CALLED: Constructor: Newly created RootNode passed"));
            SetNextNodeMaybe(pRootNode);
            CreateNewState();
        }

        // デストラクタ
        ~StrokeStream() {
            LOG_DEBUGH(_T("CALLED: Destructor"));
        }

        int StrokeLength() const {
            return (int)(STATE_COMMON->GetTotalDecKeyCount() - cntStroke);
        }

        int GetDecKey() const {
            return decKey;
        }

        void SetDecKey(int dk) {
            decKey = dk;
        }

        bool IsTailStringNode() const {
            LOG_DEBUGH(_T("ENTER"));
            bool result = StateListWalker([this](const State* p) {
                Node* pNode = p->NextNodeMaybe();
                return pNode && pNode->isStringLikeNode();
            });
            LOG_DEBUGH(_T("LEAVE: result={}"), result);
            return result;
        }

        void AppendWordPiece(std::vector<WordPiece>& pieces, bool /*bExcludeHiragana*/) {
            if (NextState()) {
                LOG_DEBUGH(_T("ENTER"));
                MStringResult result;
                State::GetResultStringChain(result);
                if (!result.isDefault()) {
                    LOG_DEBUGH(_T("ADD WORD: rewriteStr={}, string={}, numBS={}"),
                        to_wstr(result.getRewriteNode() ? result.getRewriteNode()->getString() : EMPTY_MSTR), to_wstr(result.resultStr()), result.numBS());
                    // TODO bKatakanaConversionのサポート
                    int strokeLen = StrokeLength();
                    //if (bKatakanaConversion) {
                    //    MString hs = result.resultStr();
                    //    if (std::all_of(hs.begin(), hs.end(), [](auto ch) {return utils::is_hiragana(ch) || utils::is_katakana(ch);})) {
                    //        MString ks = convertHiraganaToKatakana(hs);
                    //        pieces.push_back(WordPiece(ks, strokeLen, result.rewritableLen(), result.numBS()));
                    //    }
                    //} else {
                        if (result.getRewriteNode()) {
                            pieces.push_back(WordPiece(result.getRewriteNode(), strokeLen));
                        } else {
                            pieces.push_back(WordPiece(result.resultStr(), strokeLen, result.rewritableLen(), result.numBS()));
                        }
                    //}
                } else {
                    LOG_DEBUGH(_T("NOT TERMINAL"));
                }
                LOG_DEBUGH(_T("LEAVE"));
            }
        }

        // 新しい状態作成のチェイン(状態チェーンの末尾でのみ新状態の作成を行う)
        void DoCreateNewStateChain() {
            CreateNewStateChain();
        }

        // チェーンをたどって不要とマークされた後続状態を削除する
        void DoDeleteUnnecessarySuccessorStateChain() {
            DeleteUnnecessarySuccessorStateChain();
            // 後続状態が無くなったら自身も削除する
            if (!NextState()) MarkUnnecessary();
        }
    };
    DEFINE_CLASS_LOGGER(StrokeStream);

    typedef std::unique_ptr<StrokeStream> StrokeStreamUptr;

    // 複数のStrokeStreamを管理するクラス
    // たとえばT-Codeであっても、1ストロークずれた2つの入力ストリームが並存する可能性がある
    class StrokeStreamList {
    private:
        DECLARE_CLASS_LOGGER;

        String name;

        // RootStrokeState用の状態集合
        std::vector<StrokeStreamUptr> strokeStreamList;

        StrokeStreamUptr& addStrokeStream(StrokeTableNode* pRootNode) {
            strokeStreamList.push_back(std::make_unique<StrokeStream>(pRootNode));
            return strokeStreamList.back();
        }

        void forEach(std::function<void(const StrokeStreamUptr&)> func) const {
            for (const auto& pStream : strokeStreamList) {
                func(pStream);
            }
        }

    public:
        StrokeStreamList(StringRef name) : name(name) {
        }

        ~StrokeStreamList() {
            LOG_DEBUGH(_T("CALLED: Destructor: {}"), name);
            //for (auto* p : strokeChannelList) delete p;
        }

        // strokeStreamListのサイズ
        size_t Count() const {
            return strokeStreamList.size();
        }

        // strokeStreamListが空か
        size_t Empty() const {
            return Count() == 0;
        }

        void Clear() {
            strokeStreamList.clear();
        }

        size_t StrokeTableChainLength() const {
            size_t len = 0;
            forEach([&len](const StrokeStreamUptr& pStream) {
                size_t ln = pStream->StrokeTableChainLength();
                if (ln > len) len = ln;
                });
            LOG_DEBUGH(_T("CALLED: {}: Length={}"), name, len);
            return len;
        }

        String ChainLengthString() const {
            std::vector<int> buf;
            std::transform(strokeStreamList.begin(), strokeStreamList.end(), std::back_inserter(buf), [](const StrokeStreamUptr& pState) { return (int)pState->StrokeTableChainLength(); });
            return _T("[") + utils::join(buf, _T(",")) + _T("]");
        }

        Node* GetNonStringNode() {
            for (const auto& pState : strokeStreamList) {
                Node* p = pState->NextNodeMaybe();
                if (p && !p->isStringLikeNode()) {
                    return p;
                }
            }
            return 0;
        }

        // DECKEY 処理の前半部
        void HandleDeckeyProc(StrokeTableNode* pRootNode, int decKey, int comboStrokeCnt, int comboDeckey) {
            LOG_DEBUGH(_T("ENTER: {}: pRootNode={:p}, decKey={}, comboStrokeCnt={}, strokeStateList.Count={}"), name, (void*)pRootNode, decKey, comboStrokeCnt, Count());
            if (pRootNode != nullptr) {
                // 各ストリーム(StrokeTable)に対してDECKEY処理を行う
                bool bRollOverFound = false;
                forEach([decKey, &bRollOverFound, this](const StrokeStreamUptr& pStream) {
                    if (!bRollOverFound) {
                        int prevDecKey = pStream->GetDecKey();
                        LOG_DEBUGH(_T("{}: pStream prevDecKey={}"), name, prevDecKey);
                        pStream->HandleDeckeyChain(decKey);
                        if (DeckeyUtil::is_ordered_combo(prevDecKey) && pStream->IsTailStringNode()) {
                            // ロールオーバー打鍵で有効な文字列が得られた場合は、ここで処理を終了する
                            // (今回のストロークから開始されるストリームも作成しない)
                            // 多ストローク配列では、ロールオーバー打鍵を使用しないようにするのが安全
                            LOG_DEBUGH(_T("{}: RollOver Found: stroke={}:{}"), name, prevDecKey, decKey);
                            bRollOverFound = true;
                        }
                    }
                    });
                // 複数配列の場合は新しいストリームを追加できる
                if (Empty() || (SETTINGS->multiStreamMode && !bRollOverFound)) {
                    // 同時打鍵列に入ったら、同時打鍵の開始か、先頭がロールオーバーの場合だけ、stream を追加できる
                    if (comboStrokeCnt < 1 || (comboStrokeCnt == 1 && DeckeyUtil::is_combo_deckey(decKey)) ||
                        (comboStrokeCnt > 1 && DeckeyUtil::is_ordered_combo(comboDeckey))) {
                        LOG_DEBUGH(_T("{}: add new StrokeStream: decKey={}"), name, decKey);
                        {
                            auto& pStream = addStrokeStream(pRootNode);
                            pStream->SetDecKey(decKey);
                            pStream->HandleDeckeyChain(decKey);
                        }
                        if (SETTINGS->multiStreamMode && comboStrokeCnt == 1 && DeckeyUtil::is_ordered_combo(decKey)) {
                            // ロールオーバー打鍵なら、通常キーのストリームも追加する
                            // (かなテーブルではロールオーバーとして定義されているが、漢字配列ではロールオーバーになっていない可能性を考慮)
                            auto& pStream = addStrokeStream(pRootNode);
                            int dk = DeckeyUtil::unshiftDeckey(decKey);
                            pStream->SetDecKey(dk);
                            pStream->HandleDeckeyChain(dk);
                            LOG_DEBUGH(_T("{}: add unshifted StrokeStream: decKey={}"), name, dk);
                        }
                    } else {
                        LOG_DEBUGH(_T("{}: IN Combo; deckey={} NOT root key"), name, decKey);
                    }
                }
            }
            LOG_DEBUGH(_T("LEAVE: {}: decKey={}, strokeStateList.Count={}"), name, decKey, Count());
        }

        // 新しい状態作成
        void CreateNewStates() {
            LOG_DEBUGH(_T("ENTER: {}: size={}"), name, Count());
            forEach([](const StrokeStreamUptr& pStream) {
                pStream->DoCreateNewStateChain();
            });
            LOG_DEBUGH(_T("LEAVE: {}"), name);
        }

        void DeleteUnnecessaryNextStates() {
            LOG_DEBUGH(_T("ENTER: {}: strokeStateList.Count={}"), name, Count());
            auto iter = strokeStreamList.begin();
            while (iter != strokeStreamList.end()) {
                (*iter)->DoDeleteUnnecessarySuccessorStateChain();
                if ((*iter)->IsUnnecessary()) {
                    iter = strokeStreamList.erase(iter);
                } else {
                    ++iter;
                }
            }
            LOG_DEBUGH(_T("LEAVE: {}: strokeStateList.Count={}"), name, Count());
        }

        // 出力文字を取得する
        void GetWordPieces(std::vector<WordPiece>& pieces, bool bExcludeHiragana) {
            LOG_DEBUGH(_T("ENTER: {}: streamNum={}"), name, Count());
            forEach([&pieces, bExcludeHiragana](const StrokeStreamUptr& pStream) {
                pStream->AppendWordPiece(pieces, bExcludeHiragana);
            });
            LOG_DEBUGH(_T("LEAVE: {}"), name);
        }
       
        int MaxStrokeLength() const {
            int maxlen = 0;
            for (const auto& pStream : strokeStreamList) {
                int len = pStream->StrokeLength();
                if (maxlen < len) maxlen = len;
            }
            return maxlen;
        }

        void DebugPrintStatesChain(StringRef label) {
            if (Reporting::Logger::IsInfoHEnabled()) {
                if (strokeStreamList.empty()) {
                    LOG_DEBUGH(_T("{}: {}=empty"), name, label);
                } else {
                    forEach([label, this](const StrokeStreamUptr& pStream) {
                        LOG_DEBUGH(_T("{}: {}={}"), name, label, pStream->JoinedName());
                    });
                }
            }
        }
    };
    DEFINE_CLASS_LOGGER(StrokeStreamList);

    // -------------------------------------------------------------------
    // ストロークテーブルからの出力のマージ機能
    class StrokeMergerHistoryResidentStateImpl : public StrokeMergerHistoryResidentState {
    private:
        DECLARE_CLASS_LOGGER;

        std::unique_ptr<HistoryStateBase> histBase;

        bool bDualTableMode = false;

        // 同時打鍵中なら1以上で、同時打鍵列の位置を示す
        int _comboStrokeCount = 0;

        // 同時打鍵の開始コード
        int _comboDeckey = -1;

        // 排他的なストロークの起点
        bool _bExclusivePrefix = false;

        int _strokeCountBS = -1;
        int _prevStrokeCountBS = -1;
        int _startStrokeCountBS = -1;

        // ストロークを1つ前に戻す
        bool _strokeBack = false;

        // RootStrokeState1用の状態集合
        StrokeStreamList _streamList1;

        // RootStrokeState2用の状態集合
        StrokeStreamList _streamList2;

        // カタカナ変換モードか
        bool _isKatakanaConversionMode = false;

        void clearStreamLists() {
            _LOG_DETAIL(L"CALLED: Clear streamLists");
            _streamList1.Clear();
            _streamList2.Clear();
            _LOG_DETAIL(L"ClearVkbLayout");
            STATE_COMMON->ClearVkbLayout();
        }

        // とりあえず、無変換、変換、IME ON/OFFキーは単独打鍵扱いにする
        inline static bool isSingleHitKey(int deckey) {
            return deckey == NFER_DECKEY || deckey == XFER_DECKEY || deckey == IME_OFF_DECKEY || deckey == IME_ON_DECKEY;
        }

    public:
        // コンストラクタ
        StrokeMergerHistoryResidentStateImpl(StrokeMergerHistoryNode* pN)
            : histBase(HistoryStateBase::createInstance(pN))
            , _streamList1(_T("streamList1"))
            , _streamList2(_T("streamList2")) {
            LOG_DEBUGH(_T("CALLED: Constructor"));
            Initialize(logger.ClassNameT(), pN);
        }

        ~StrokeMergerHistoryResidentStateImpl() override {
            LOG_DEBUGH(_T("ENTER: Destructor"));
            ClearState();
            LOG_DEBUGH(_T("LEAVE: Destructor"));
        }

        void ClearState() override {
            LOG_DEBUGH(_T("CALLED"));
            WORD_LATTICE->clearAll();
            clearStreamLists();
        }

        // 状態が生成されたときに実行する処理 (特に何もせず、前状態にチェインする)
        void DoProcOnCreated() override {
            LOG_DEBUGH(_T("CALLED: Chained"));
            MarkNecessary();
        }

        // DECKEY 処理の流れ
        void HandleDeckeyChain(int deckey) override {
            _LOG_INFOH(_T("\nENTER: deckey={:x}H({}={}), totalCount={}, statesNum=({},{})"),
                deckey, deckey, DECKEY_TO_CHARS->GetDeckeyNameFromId(deckey), STATE_COMMON->GetTotalDecKeyCount(), _streamList1.Count(), _streamList2.Count());

            resultStr.clear();
            myChar = '\0';

            bDualTableMode = StrokeTableNode::RootStrokeNode1 != nullptr && StrokeTableNode::RootStrokeNode2 != nullptr ;

            LOG_DEBUGH(_T("NextState={}"), STATE_NAME(NextState()));
            if (DeckeyUtil::is_combo_deckey(deckey) && !DeckeyUtil::is_ordered_combo(deckey) && NextState() && NextState()->GetName() == L"EisuState") {
                // 英数モード中の同時打鍵なら、EisuStateを終了する
                LOG_DEBUG(L"deckey is combo and NextState is EisuState: delete it");
                this->DeleteNextState();
            }
            if (NextState()) {
                LOG_DEBUGH(_T("NextState: FOUND"));
                // 後続状態があれば、そちらを呼び出す
                NextState()->HandleDeckeyChain(deckey);
            } else {
                _streamList1.DebugPrintStatesChain(_T("HandleDeckeyChain::BEGIN: streamList1"));
                _streamList2.DebugPrintStatesChain(_T("HandleDeckeyChain::BEGIN: streamList2"));

                bool bHasAnyStroke = !_streamList1.Empty() || !_streamList2.Empty();
                LOG_DEBUGH(_T("\nuseEditBuffer={}, bDualTableMode={}, bHasAnyStroke={}"), SETTINGS->useEditBuffer, bDualTableMode, bHasAnyStroke);

                if (/*deckey != CLEAR_STROKE_DECKEY && */
                    ((deckey >= FUNC_DECKEY_START && deckey < FUNC_DECKEY_END && !isSingleHitKey(deckey)) || deckey >= CTRL_DECKEY_START)) {
                    _LOG_DETAIL(L"Clear streamLists");
                    clearStreamLists();
                    _strokeBack = false;
                    bool doDefault = false;
                    bool multiCands = false;
                    switch (deckey) {
                    case ENTER_DECKEY:
                        multiCands = WORD_LATTICE->hasMultiCandidates();
                        LOG_DEBUGH(_T("EnterKey: clear streamList: useEditBuffer={}, hasMultiCandidates={}"), SETTINGS->useEditBuffer, multiCands);
                        WORD_LATTICE->raiseAndLowerByCandSelection();
                        _isKatakanaConversionMode = false;
                        WORD_LATTICE->clearAll();
                        if (SETTINGS->useEditBuffer || !multiCands) {
                            LOG_DEBUGH(_T("CALL dispatchDeckey"));
                            State::dispatchDeckey(deckey);
                        }
                        break;
                    case MULTI_STREAM_COMMIT_DECKEY:
                        LOG_DEBUGH(_T("MULTI_STREAM_COMMIT"));
                        WORD_LATTICE->raiseAndLowerByCandSelection();
                        _isKatakanaConversionMode = false;
                        WORD_LATTICE->clearAll();
                        //OUTPUT_STACK->setMazeBlocker();
                        //MarkUnnecessary();
                        break;
                    case STROKE_BACK_DECKEY:
                        // 打鍵取消
                        LOG_DEBUGH(_T("STROKE_BACK_DECKEY"));
                        _strokeCountBS = (int)STATE_COMMON->GetTotalDecKeyCount();
                        _strokeBack = true;
                        if (WORD_LATTICE->isEmpty()) State::handleBS();
                        break;
                    case BS_DECKEY:
                        LOG_DEBUGH(_T("BS"));
                        _strokeCountBS = (int)STATE_COMMON->GetTotalDecKeyCount();
                        if (_strokeCountBS != _prevStrokeCountBS + 1) _startStrokeCountBS = _strokeCountBS;
                        if (SETTINGS->strokeBackByBS && (SETTINGS->maxStrokeBackCount <= 0 || _strokeCountBS - _startStrokeCountBS < SETTINGS->maxStrokeBackCount)) {
                            // 打鍵取消
                            LOG_DEBUGH(_T("stroke back by BS"));
                            _strokeBack = true;
                        } else {
                            // 現在の先頭候補を優先する
                            //WORD_LATTICE->selectFirst();
                            WORD_LATTICE->removeOtherThanFirst();
                        }
                        _prevStrokeCountBS = _strokeCountBS;
                        if (!SETTINGS->multiStreamMode && bHasAnyStroke) {
                            LOG_DEBUGH(_T("Single Table Mode and HasAnyKey: Clear streamLists by BS"));
                            _strokeCountBS = -1;   // あとでGetTotalDecKeyCount()と比較して同じだったら、LatticeのBSが呼ばれてしまうので、ここで初期化しておく
                            break;
                        }
                        if (WORD_LATTICE->isEmpty()) State::handleBS();
                        break;
                    case DOWN_ARROW_DECKEY:
                        LOG_DEBUGH(_T("DOWN_ARROW_DECKEY: select next candidate"));
                        if (!WORD_LATTICE->hasMultiCandidates() &&
                            (MERGER_HISTORY_RESIDENT_STATE->IsHistorySelectableByArrowKey() || !SETTINGS->useEditBuffer || WORD_LATTICE->isEmpty())) {
                            //State::handleDownArrow();
                            doDefault = true;
                            break;
                        }
                        [[fallthrough]];
                    case MULTI_STREAM_NEXT_CAND_DECKEY:
                        // 漢直・かな配列の融合時の次候補選択
                        LOG_DEBUGH(_T("MULTI_STREAM_NEXT_CAND: select next candidate"));
                        WORD_LATTICE->selectNext();
                        break;
                    case UP_ARROW_DECKEY:
                        LOG_DEBUGH(_T("UP_ARROW_DECKEY: select prev candidate"));
                        if (!WORD_LATTICE->hasMultiCandidates() &&
                            (MERGER_HISTORY_RESIDENT_STATE->IsHistorySelectableByArrowKey() || !SETTINGS->useEditBuffer || WORD_LATTICE->isEmpty())) {
                            //State::handleUpArrow();
                            doDefault = true;
                            break;
                        }
                        [[fallthrough]];
                    case MULTI_STREAM_PREV_CAND_DECKEY:
                        LOG_DEBUGH(_T("MULTI_STREAM_PREV_CAND: select prev candidate"));
                        WORD_LATTICE->selectPrev();
                        break;
                    case MULTI_STREAM_SELECT_FIRST_DECKEY:
                        // 現在の先頭候補以外を削除する
                        LOG_DEBUGH(_T("MULTI_STREAM_SELECT_FIRST: commit first candidate"));
                        //WORD_LATTICE->selectFirst();
                        WORD_LATTICE->clearKanjiXorHiraganaPreferredNextCands();
                        WORD_LATTICE->removeOtherThanFirst();
                        break;
                    case HISTORY_FULL_CAND_DECKEY:
                        LOG_DEBUGH(_T("HISTORY_FULL_CAND"));
                        if (!NextNodeMaybe()) {
                            SetNextNodeMaybe(HISTORY_NODE);
                        }
                        break;
                    case TOGGLE_ZENKAKU_CONVERSION_DECKEY:
                        LOG_DEBUGH(_T("TOGGLE_ZENKAKU_CONVERSION"));
                        _isKatakanaConversionMode = false;
                        if (!NextNodeMaybe()) {
                            WORD_LATTICE->clearAll();
                            SetNextNodeMaybe(ZENKAKU_NODE);
                        }
                        break;
                    case TOGGLE_KATAKANA_CONVERSION_DECKEY:
                        LOG_DEBUGH(_T("TOGGLE_KATAKANA_CONVERSION"));
                        _isKatakanaConversionMode = !_isKatakanaConversionMode;
                        break;
                    case EISU_MODE_TOGGLE_DECKEY:
                    //case EISU_CONVERSION_DECKEY:
                        LOG_DEBUGH(_T("EISU_MODE_TOGGLE"));
                        _isKatakanaConversionMode = false;
                        if (!NextNodeMaybe()) {
                            WORD_LATTICE->clearAll();
                            EISU_NODE->blockerNeeded = true; // 入力済み末尾にブロッカーを設定する
                            EISU_NODE->eisuExitCapitalCharNum = 0;
                            SetNextNodeMaybe(EISU_NODE);
                        }
                        break;
                    case BUSHU_COMP_DECKEY:
                        LOG_DEBUGH(_T("BUSHU_COMP"));
                        WORD_LATTICE->updateByBushuComp();
                        break;
                    case MAZE_CONVERSION_DECKEY:
                        // 交ぜ書き変換
                        LOG_DEBUGH(_T("MAZE_CONVERSION"));
                        WORD_LATTICE->updateByMazegaki();
                        //if (!NextNodeMaybe()) {
                        //    OUTPUT_STACK->pushNewString(WORD_LATTICE->getFirst());
                        //    WORD_LATTICE->clearAll();
                        //    if (!MAZEGAKI_INFO || !MAZEGAKI_INFO->RevertPrevXfer(resultStr)) {
                        //        SetNextNodeMaybe(MAZEGAKI_NODE);
                        //    }
                        //}
                        break;
                    case MULTI_STREAM_KANJI_PREFERRED_NEXT_DECKEY:
                        // 次の打鍵を漢字のみ通す
                        LOG_DEBUGH(_T("MULTI_STREAM_KANJI_PREFERRED_NEXT_DECKEY"));
                        //WORD_LATTICE->removeOtherThanFirst();
                        WORD_LATTICE->setKanjiXorHiraganaPreferredNextCands(true);
                        break;
                    case MULTI_STREAM_HIRAGANA_PREFERRED_NEXT_DECKEY:
                        // 次の打鍵をひらがなのみ通す
                        LOG_DEBUGH(_T("MULTI_STREAM_HIRAGANA_PREFERRED_NEXT_DECKEY"));
                        //WORD_LATTICE->removeOtherThanFirst();
                        WORD_LATTICE->setKanjiXorHiraganaPreferredNextCands(false);
                        break;
                    case CLEAR_STROKE_DECKEY:
                        _LOG_DETAIL(_T("CLEAR_STROKE_DECKEY: DO NOTHING"));
                        // Do nothing
                        _isKatakanaConversionMode = false;
                        break;
                    default:
                        LOG_DEBUGH(_T("OTHER"));
                        doDefault = true;
                        break;
                    }
                    if (doDefault) {
                        _isKatakanaConversionMode = false;
                        WORD_LATTICE->clearAll();
                        //MarkUnnecessary();
                        LOG_DEBUGH(_T("CALL State::dispatchDeckey({}={})"), deckey, DECKEY_TO_CHARS->GetDeckeyNameFromId(deckey));
                        State::dispatchDeckey(deckey);
                    }
                } else {
                    if (deckey == SETTINGS->exclusivePrefixCode) {
                        if (!_bExclusivePrefix) {
                            _LOG_DETAIL(L"ExclusivePrefix: {:x}H({})", deckey, deckey);
                            clearStreamLists();
                            _bExclusivePrefix = true;
                            _comboStrokeCount = 1;
                            _comboDeckey = deckey;
                            LOG_DEBUGH(_T("_comboStrokeCount={}"), _comboStrokeCount);
                        }
                    } else if (DeckeyUtil::is_combo_deckey(deckey)
                        /* && (!DeckeyUtil::is_ordered_combo(deckey)*/ /* || OUTPUT_STACK->GetLastOutputStackChar() != ' ')*/) {
                        // 順序あり以外の同時打鍵の始まりなので、いったん streamList はクリア
                        // 順序あり同時打鍵は、2ストロークの2打鍵目となることあり; 「使ら」)
                        // また、Spaceの後の順序あり同時打鍵は、裏文字入力の場合あり
                        // 2025/1/10 しかし、順序あり同時打鍵と漢直を両立させようとすると、順序あり同時打鍵を1打鍵ずつバラして扱う必要があるので、
                        //           そうであれば、結局、最初から後置書き換えでよいのではないか。
                        //           ということで、順序あり同時打鍵は、同時打鍵として扱うことにする。
                        // 同時打鍵中は、処理を分岐させない
                        //_LOG_DETAIL(L"Combo Deckey except ORDERED");
                        _LOG_DETAIL(L"Combo Deckey: clearStreamLists: _comboStrokeCount=1");
                        clearStreamLists();
                        _comboStrokeCount = 1;
                        _comboDeckey = deckey;
                    } else if (isSingleHitKey(deckey)) {
                        clearStreamLists();
                    }
                    if (SETTINGS->eisuModeEnabled && _comboStrokeCount == 0
                        && deckey >= SHIFT_DECKEY_START && deckey < SHIFT_DECKEY_END && !STATE_COMMON->IsUpperRomanGuideMode()) {
                        myChar = DECKEY_TO_CHARS->GetCharFromDeckey(deckey);
                        if (myChar >= 'A' && myChar <= 'Z') {
                            // 英数モード
                            LOG_DEBUGH(_T("SetNextNodeMaybe: Eisu"));
                            clearStreamLists();
                            WORD_LATTICE->clearAll();
                            EISU_NODE->blockerNeeded = true; // 入力済み末尾にブロッカーを設定する
                            EISU_NODE->eisuExitCapitalCharNum = SETTINGS->eisuExitCapitalCharNum;
                            SetNextNodeMaybe(EISU_NODE);
                            return;
                        }
                    }
                    // 前処理(ストローク木状態の作成と呼び出し)
                    LOG_DEBUGH(_T("streamList1.HandleDeckeyProc: CALL: doDeckeyPreProc: _comboStrokeCount={}, _comboDeckey={}"), _comboStrokeCount, _comboDeckey);
                    _streamList1.HandleDeckeyProc(StrokeTableNode::RootStrokeNode1.get(), deckey, _comboStrokeCount, _comboDeckey);
                    LOG_DEBUGH(_T("streamList1.HandleDeckeyProc: DONE"));
                    LOG_DEBUGH(_T("streamList2.HandleDeckeyProc: CALL: doDeckeyPreProc: _comboStrokeCount={}, _comboDeckey={}"), _comboStrokeCount, _comboDeckey);
                    _streamList2.HandleDeckeyProc(StrokeTableNode::RootStrokeNode2.get(), deckey, _comboStrokeCount, _comboDeckey);
                    LOG_DEBUGH(_T("streamList2.HandleDeckeyProc: DONE"));
                    if (_comboStrokeCount > 0) ++_comboStrokeCount;     // 同時打鍵で始まった時だけ
                    LOG_DEBUGH(_T("_comboStrokeCount={}"), _comboStrokeCount);
                    if (_bExclusivePrefix) {
                        LOG_DEBUGH(_T("IN exclusive stroke"));
                        if (StrokeTableChainLength() == 0) {
                            LOG_DEBUGH(_T("exclusive stroke end: _comboStrokeCount=0"));
                            _bExclusivePrefix = false;
                            _comboStrokeCount = 0;
                            _comboDeckey = -1;
                        }
                    } else {
                        if (deckey < SHIFT_DECKEY_START) {
                            // 同時打鍵列の終わり
                            LOG_DEBUGH(_T("combo stroke end: _comboStrokeCount=0"));
                            _comboStrokeCount = 0;
                            _comboDeckey = -1;
                        }
                    }
                }
            }

            if (_isKatakanaConversionMode) {
                STATE_COMMON->SetCenterString(L'カ');
            }
            _LOG_INFOH(_T("LEAVE: _comboStrokeCount={}\n"), _comboStrokeCount);
        }

        // 新しい状態作成のチェイン
        void CreateNewStateChain() override {
            _LOG_DETAIL(_T("ENTER: {}"), Name);
            LOG_DEBUGH(_T("streamList1: CreateNewStates"));
            _streamList1.CreateNewStates();
            LOG_DEBUGH(_T("streamList2: CreateNewStates"));
            _streamList2.CreateNewStates();
            State::CreateNewStateChain();
            _LOG_DETAIL(_T("LEAVE: {}"), Name);
        }

        // 出力文字を取得する
        void GetResultStringChain(MStringResult& resultOut) override {
            _LOG_DETAIL(_T("\nENTER: {}, resultStr={}"), Name, resultStr.debugString());
            //LOG_DEBUGH(L"A:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));

            _streamList1.DebugPrintStatesChain(_T("GetResultStringChain::BEGIN: streamList1"));
            _streamList2.DebugPrintStatesChain(_T("GetResultStringChain::BEGIN: streamList2"));

            STATE_COMMON->SetCurrentModeIsMultiStreamInput();

            LOG_DEBUGH(_T("CHECKPOINT-1"));
            if (!resultStr.isModified() && NextState()) {
                LOG_DEBUGH(_T("CALL NextState()->GetResultStringChain"));
                NextState()->GetResultStringChain(resultOut);
                //LOG_DEBUGH(L"C:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            } else {
                LOG_DEBUGH(_T("CHECKPOINT-2"));

                // 単語素片の収集
                std::vector<WordPiece> pieces;
                if (resultStr.isModified()) {
                    // EISUなど、何か他の処理で文字列が得られた場合
                    LOG_DEBUGH(_T("resultStr={}"), resultStr.debugString());
                    WORD_LATTICE->clear();
                    pieces.push_back(WordPiece(resultStr.resultStr(), 1, 0));
                }
                else if ((int)STATE_COMMON->GetTotalDecKeyCount() == _strokeCountBS) {
                    LOG_DEBUGH(_T("add WordPiece for BS."));
                    pieces.push_back(WordPiece::BSPiece());
                    //LOG_DEBUGH(L"D:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
                } else {
                    LOG_DEBUGH(_T("CHECKPOINT-3: GetWordPieces"));
                    LOG_DEBUGH(_T("streamList1: GetWordPieces"));
                    _streamList1.GetWordPieces(pieces, false);
                    LOG_DEBUGH(_T("streamList2: GetWordPieces"));
                    _streamList2.GetWordPieces(pieces, false);

                    LOG_DEBUGH(_T("CHECKPOINT-4: Get Non StringNode"));
                    Node* pNextNode1 = _streamList1.GetNonStringNode();
                    Node* pNextNode2 = _streamList2.GetNonStringNode();
                    if (pNextNode1 || pNextNode2) {
                        // 文字列ノード以外が返ったら、状態をクリアする
                        LOG_DEBUGH(_T("CHECKPOINT-4-A: NON StringNode FOUND. Clear StreamList: pNextNode1={:p}, pNextNode2={:p}"), (void*)pNextNode1, (void*)pNextNode2);
                        clearStreamLists();
                        SetNextNodeMaybe(pNextNode1 ? pNextNode1 : pNextNode2);
                    }

                    LOG_DEBUGH(_T("CHECKPOINT-5: Check pieces.empty()"));
                    if (pieces.empty()) {
                        // ストロークを進めるために、空のpieceを追加する
                        LOG_DEBUGH(_T("CHECKPOINT-5-A: pieces is EMPTY. Add Padding Piece."));
                        pieces.push_back(WordPiece::paddingPiece());
                    }
#if 0
                    LOG_DEBUGH(_T("CHECKPOINT-6"));
                    if (pieces.empty()) {
                        // 両方のStreamで多ストローク列に入った。つまり、当ストロークは何も文字が入力されない真空ストローク。
                        // なので、強制的に排他的ストロークモードに移行する。
                        // すでに第1ストロークは打鍵済みなので、_comboStrokeCount は2から始まる。
                        LOG_DEBUGH(_T("CHECKPOINT-7: pieces is empty. Set _bExclusivePrefix = TRUE."));
                        _bExclusivePrefix = true;
                        if (_comboStrokeCount == 0) {
                            _comboStrokeCount = 2;
                            LOG_DEBUGH(_T("CHECKPOINT-7-A: _comboStrokeCount={}"), _comboStrokeCount);
                        }
                    }
#endif
                }

                LOG_DEBUGH(_T("CHECKPOINT-6: getLatticeResult"));
                int pieceNumBS = pieces.empty() ? 0 : pieces.back().numBS();
                // Lattice処理
                auto result = getLatticeResult(pieces);
                _strokeBack = false;

                if (resultStr.numBS() > 0) result.numBS = resultStr.numBS();

                //LOG_DEBUGH(L"F:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));

                LOG_DEBUGH(_T("CHECKPOINT-7: check result"));
                // 新しい文字列が得られたらそれを返す
                if (!result.outStr.empty() || result.numBS > 0 || pieceNumBS > 0) {
                    LOG_DEBUGH(_T("CHECKPOINT-7-A: setResult: outStr={}, numBS={}, pieceNumBS={}"), to_wstr(result.outStr), result.numBS, pieceNumBS);
                    resultOut.setResult(result.outStr, (int)(result.numBS));
                    SetTranslatedOutString(resultOut);
                    //LOG_DEBUGH(L"G:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
                } else {
                    LOG_DEBUGH(_T("CHECKPOINT-7-B: NO resultOut"));
                }
            }

            LOG_DEBUGH(_T("CHECKPOINT-8: syncOutputStackTail: resultStr=<{}>, numBS={}"), to_wstr(resultOut.resultStr()), resultOut.numBS());
            syncOutputStackTail(resultOut.resultStr(), resultOut.numBS());

            LOG_DEBUGH(_T("CHECKPOINT-9: Check StreamList and WORD_LATTICE"));
            if (_streamList1.Empty() && _streamList2.Empty() && WORD_LATTICE->isEmpty()) {
                LOG_DEBUGH(L"CHECKPOINT-9-A: StreamList and WORD_LATTICE are both EMPTY. CALL WORD_LATTICE->clear()");
                WORD_LATTICE->clear();
                //MarkUnnecessary();
                LOG_DEBUGH(_T("ClearCurrentModeIsMultiStreamInput"));
                STATE_COMMON->ClearCurrentModeIsMultiStreamInput();
            }
            LOG_DEBUGH(_T("CHECKPOINT-10"));

            _streamList1.DebugPrintStatesChain(_T("GetResultStringChain::END: streamList1"));
            _streamList2.DebugPrintStatesChain(_T("GetResultStringChain::END: streamList2"));

            _LOG_DETAIL(_T("LEAVE: {}: resultStr=[{}]\n"), Name, resultOut.debugString());
        }

        // Latticeに WordPiece群を追加して処理する
        LatticeResult getLatticeResult(const std::vector<WordPiece>& pieces) {
            if (!pieces.empty()) {
                MString s = pieces.front().getString();
                if (s.size() >= 4) {
                    size_t pos = s.find(L'!');
                    if (pos <= s.size() - 3 && s[pos + 1] == L'{' && s.find(L'}', pos + 2) < s.size()) {
                        // !{..} が含まれていたら、
                        WORD_LATTICE->clearAll();
                        return LatticeResult(s, 0);
                    }
                }
            }
            //LOG_DEBUGH(L"L:faces={}", to_wstr(STATE_COMMON->GetFaces(), 20));
            bool bUseMorphAnalyzer = SETTINGS->useMorphAnalyzerAlways || StrokeTableNode::RootStrokeNode1 != nullptr && StrokeTableNode::RootStrokeNode2 != nullptr ;
            return WORD_LATTICE->addPieces(pieces, bUseMorphAnalyzer, _strokeBack, _isKatakanaConversionMode);
        }

        // チェーンをたどって不要とマークされた後続状態を削除する
        void DeleteUnnecessarySuccessorStateChain() override {
            LOG_DEBUGH(_T("ENTER: {}, IsUnnecessary={}"), Name, IsUnnecessary());
            LOG_DEBUGH(_T("streamList1: deleteUnnecessaryNextState"));
            _streamList1.DeleteUnnecessaryNextStates();
            LOG_DEBUGH(_T("streamList2: deleteUnnecessaryNextState"));
            _streamList2.DeleteUnnecessaryNextStates();

            _streamList1.DebugPrintStatesChain(_T("DeleteUnnecessary::DONE: streamList1"));
            _streamList2.DebugPrintStatesChain(_T("DeleteUnnecessary::DONE: streamList2"));

            State::DeleteUnnecessarySuccessorStateChain();

            LOG_DEBUGH(_T("LEAVE: {}, NextNode={}"), Name, NODE_NAME(NextNodeMaybe()));
        }

        // ストロークテーブルチェインの長さ(テーブルのレベル)
        size_t StrokeTableChainLength() const override {
            size_t len1 = _streamList1.StrokeTableChainLength();
            size_t len2 = _streamList2.StrokeTableChainLength();
            return len1 < len2 ? len2 : len1;
        }

        String JoinedName() const override {
            auto myName = Name + _T("(") + _streamList1.ChainLengthString() + _T("/") + _streamList2.ChainLengthString() + _T(")");
            if (NextState()) myName += _T("-") + NextState()->JoinedName();
            return myName;
        }

        // -------------------------------------------------------------------
        // 以下、履歴機能
    private:
        int candSelectDeckey = -1;

        /// 今回の履歴候補選択ホットキーを保存
        /// これにより、DoLastHistoryProc() で継続的な候補選択のほうに処理が倒れる
        void setCandSelectIsCalled() { candSelectDeckey = STATE_COMMON->GetDeckey(); }

        // 状態管理のほうで記録している最新ホットキーと比較し、今回が履歴候補選択キーだったか
        bool wasCandSelectCalled() { return candSelectDeckey >= 0 && candSelectDeckey == STATE_COMMON->GetDeckey(); }

        // 呼び出されたのは編集用キーか
        bool isEditingFuncDecKey() {
            int deckey = STATE_COMMON->GetDeckey();
            return deckey >= HOME_DECKEY && deckey <= RIGHT_ARROW_DECKEY;
        }

        // 呼び出されたのはENTERキーか
        bool isEnterDecKey() {
            return STATE_COMMON->GetDeckey() == ENTER_DECKEY;
        }

        // 後続状態で出力スタックが変更された可能性あり
        bool maybeEditedBySubState = false;

        // Shift+Space等による候補選択が可能か
        bool bCandSelectable = false;

    public:
        void syncOutputStackTail(const MString& outStr, int numBS) {
            if (!OUTPUT_STACK) return;

            if (numBS > 0) {
                OUTPUT_STACK->pop((size_t)numBS);
            }
            if (!outStr.empty()) {
                auto outW = to_wstr(outStr);
                OUTPUT_STACK->push(utils::convert_star_and_question_to_hankaku(outW.c_str()));
            }
        }

        // 状態の再アクティブ化
        void Reactivate() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            if (NextState()) NextState()->Reactivate();
            // ちょっと以下の意図が不明
            //maybeEditedBySubState = true;
            //DoLastHistoryProc();
            // 初期化という意味で、下記のように変更しておく(2021/5/31)
            maybeEditedBySubState = false;
            bCandSelectable = false;
            LOG_DEBUGH(_T("bCandSelectable=False"));
            resultStr.clear();
            STROKE_MERGER_NODE->ClearPrevHistState();     // まだ履歴検索が行われていないということを表す
            HIST_CAND->ClearKeyInfo();      // まだ履歴検索が行われていないということを表す
        }

        // 履歴検索を初期化する状態か
        bool IsHistoryReset() override {
            bool result = (NextState() && NextState()->IsHistoryReset());
            LOG_DEBUGH(_T("CALLED: {}: result={}"), Name, result);
            return result;
        }

    public:
        // Enter時の新しい履歴の追加
        void AddNewHistEntryOnEnter() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            if (HISTORY_DIC) {
                HIST_CAND->DelayedPushFrontSelectedWord();
                STATE_COMMON->SetBothHistoryBlockFlag();
                LOG_DEBUGH(_T("SetBothHistoryBlocker"));
                if (OUTPUT_STACK->isLastOutputStackCharKanjiOrKatakana()) {
                    // これまでの出力末尾が漢字またはカタカナであるなら
                    // 出力履歴の末尾の漢字列またはカタカナ列を取得して、それを履歴辞書に登録する
                    HISTORY_DIC->AddNewEntry(OUTPUT_STACK->GetLastKanjiOrKatakanaStr<MString>());
                } else if (OUTPUT_STACK->isLastOutputStackCharHirakana()) {
                    //// 漢字・カタカナ以外なら5〜10文字の範囲でNグラム登録する
                    //HISTORY_DIC->AddNgramEntries(OUTPUT_STACK->GetLastJapaneseStr<MString>(10));
                }
            }
        }

        // 何か文字が入力されたときの新しい履歴の追加
        void AddNewHistEntryOnSomeChar() override {
            //auto ch1 = STATE_COMMON->GetFirstOutChar();
            auto ch1 = utils::safe_front(resultStr.resultStr());
            auto ch2 = OUTPUT_STACK->GetLastOutputStackChar();
            if (ch1 != 0 && HISTORY_DIC) {
                // 今回の出力の先頭が漢字以外であり、これまでの出力末尾が漢字であるか、
                if ((!utils::is_kanji(ch1) && (utils::is_kanji(ch2))) ||
                    // または、今回の出力の先頭がカタカナ以外であり、これまでの出力末尾がカタカナであるなら、
                    (!utils::is_katakana(ch1) && (utils::is_katakana(ch2)))) {
                    LOG_DEBUG(_T("Call AddNewEntry"));
                    // 出力履歴の末尾の漢字列またはカタカナ列を取得して、それを履歴辞書に登録する
                    HISTORY_DIC->AddNewEntry(OUTPUT_STACK->GetLastKanjiOrKatakanaStr<MString>());
                } else if (utils::is_japanese_char_except_nakaguro((wchar_t)ch1)) {
                    //LOG_DEBUG(_T("Call AddNgramEntries"));
                    //// 出力末尾が日本語文字なら5〜10文字の範囲でNグラム登録する
                    //HISTORY_DIC->AddNgramEntries(OUTPUT_STACK->GetLastJapaneseStr<MString>(9) + ch1);
                }
            }
        }

        // 文字列を変換して出力、その後、履歴の追加
        void SetTranslatedOutString(const MStringResult& result) {
            SetTranslatedOutString(result.resultStr(), result.rewritableLen(), result.isBushuComp(), result.numBS());
        }

        // 文字列を変換して出力、その後、履歴の追加
        void SetTranslatedOutString(const MString& outStr, size_t rewritableLen, bool bBushuComp = true, int numBS = -1) override {
            LOG_DEBUGH(_T("ENTER: {}: outStr={}, rewritableLen={}, bushuComp={}, numBS={}"), Name, to_wstr(outStr), rewritableLen, bBushuComp, numBS);
            MString xlatStr;
            if (NextState()) {
                // Katakana など
                LOG_DEBUGH(_T("Call TranslateString of NextStates={}"), JoinedName());
                xlatStr = NextState()->TranslateString(outStr);
            }
            if (!xlatStr.empty() && xlatStr != outStr) {
                resultStr.setResultWithRewriteLen(xlatStr, xlatStr == outStr ? rewritableLen : 0, numBS);
            } else {
                resultStr.clear();
                //if (bBushuComp && SETTINGS->autoBushuCompMinCount > 0) {
                //    // 自動部首合成⇒Latticeの中でやっている
                //    LOG_DEBUGH(_T("Call AutoBushu"));
                //    BUSHU_COMP_NODE->ReduceByAutoBushu(outStr, resultStr);
                //}
                if (!resultStr.isModified()) {
                    LOG_DEBUGH(_T("Set outStr"));
                    resultStr.setResultWithRewriteLen(outStr, rewritableLen, numBS);
                }
            }
            AddNewHistEntryOnSomeChar();
            LOG_DEBUGH(_T("LEAVE: {}: resultStr={}, numBS={}"), Name, to_wstr(resultStr.resultStr()), resultStr.numBS());
        }

        void handleFullEscapeResidentState() override {
            handleFullEscape();
        }

        // 先頭文字の小文字化
        void handleEisuDecapitalize() override {
            LOG_DEBUGH(_T("ENTER: {}"), Name);
            auto romanStr = OUTPUT_STACK->GetLastAsciiKey<MString>(SETTINGS->histMapKeyMaxLength + 1);
            if (!romanStr.empty() && romanStr.size() <= SETTINGS->histMapKeyMaxLength) {
                if (is_upper_alphabet(romanStr[0])) {
                    romanStr[0] = to_lower(romanStr[0]);
                    resultStr.setResult(romanStr, (int)(romanStr.size()));
                }
            }
            LOG_DEBUGH(_T("LEAVE: {}"), Name);
        }

        // CommitState の処理 -- 処理のコミット
        void handleCommitState() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            commitHistory();
        }

        void commitHistory() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            // 候補が選択されていれば、それを使用履歴の先頭にpushする
            HIST_CAND->DelayedPushFrontSelectedWord();
            // どれかの候補が選択されている状態なら、それを確定し、履歴キーをクリアしておく
            STROKE_MERGER_NODE->ClearPrevHistState();
            HIST_CAND->ClearKeyInfo();
        }

        bool IsHistorySelectableByArrowKey() const override {
            return SETTINGS->useArrowToSelCand && bCandSelectable;
        }

    protected:
        //// 履歴常駐状態の事前チェック
        //int HandleDeckeyPreProc(int deckey) override {
        //    LOG_DEBUGH(_T("ENTER: XXXX: {}"), Name);
        //    resultStr.clear();
        //    //deckey = ModalStateUtil::ModalStatePreProc(this, deckey,
        //    //    State::isStrokableKey(deckey) && (!bCandSelectable || deckey >= 10 || !SETTINGS->selectHistCandByNumberKey));
        //    maybeEditedBySubState = false;
        //    // 常駐モード
        //    //if (pNext && pNext->GetName().find(_T("History")) == String::npos)
        //    if (IsHistoryReset()) {
        //        // 履歴機能ではない次状態(StrokeStateなど)があれば、それが何かをしているはずなので、戻ってきたら新たに候補の再取得を行うために、ここで maybeEditedBySubState を true にセットしておく
        //        //prevKey.clear();
        //        LOG_DEBUGH(_T("Set Reinitialized=true"));
        //        maybeEditedBySubState = true;
        //        STROKE_MERGER_NODE->ClearPrevHistState();    // まだ履歴検索が行われていない状態にしておく
        //        HIST_CAND->ClearKeyInfo();      // まだ履歴検索が行われていないということを表す
        //    }
        //    LOG_DEBUGH(_T("LEAVE: {}"), Name);

        //    return deckey;
        //}

        //// ノードから生成した状態を後接させ、その状態を常駐させる(ここでは 0 が渡ってくるはず)
        //void ChainAndStayResident(Node* ) {
        //    // 前状態にチェインする
        //    LOG_DEBUG(_T("Chain: {}"), Name);
        //    STATE_COMMON->ChainMe();
        //}

    private:
        bool matchWildcardKey(const MString& cand, const MString& wildKey) {
            LOG_DEBUGH(_T("cand={}, wildKey={}"), to_wstr(cand), to_wstr(wildKey));
            auto keys = utils::split(wildKey, '*');
            if (keys.size() == 2) {
                const MString& key0 = keys[0];
                const MString& key1 = keys[1];
                if (!key0.empty() && !key1.empty()) {
                    if (cand.size() >= key0.size() + key1.size()) {
                        // wildcard key なので、'|' の語尾は気にしなくてよい('|' を含むやつにはマッチさせないので)
                        LOG_DEBUGH(_T("startsWithWildKey({}, {}, 0) && utils::endsWithWildKey({}, {})"), to_wstr(cand), to_wstr(key0), to_wstr(cand), to_wstr(key1));
                        return utils::startsWithWildKey(cand, key0, 0) && utils::endsWithWildKey(cand, key1);
                    }
                }
            }
            return false;
        }

        // 直前キーが空でなく、候補が1つ以上あり、第1候補または第2候補がキー文字列から始まっていて、かつ同じではないか
        // たとえば、直前に「竈門」を交ぜ書きで出力したような場合で、これまでの出力履歴が「竈門」だけなら履歴候補の表示はやらない。
        // 他にも「竈門炭治郎」の出力履歴があるなら、履歴候補の表示をする。
        bool isHotCandidateReady(const MString& prevKey, const std::vector<MString>& cands) {
            size_t gobiLen = SETTINGS->histMapGobiMaxLength;
            size_t candsSize = cands.size();
            MString cand0 = candsSize > 0 ? cands[0] : MString();
            MString cand1 = candsSize > 1 ? cands[1] : MString();
            LOG_DEBUGH(_T("ENTER: prevKey={}, cands.size={}, cand0={}, cand1={}, gobiLen={}"), to_wstr(prevKey), candsSize, to_wstr(cand0), to_wstr(cand1), gobiLen);

            bool result = (!prevKey.empty() &&
                           ((!cand0.empty() && (utils::startsWithWildKey(cand0, prevKey, gobiLen) || matchWildcardKey(cand0, prevKey)) && cand0 != prevKey) ||
                            (!cand1.empty() && (utils::startsWithWildKey(cand1, prevKey, gobiLen) || matchWildcardKey(cand1, prevKey)) && cand1 != prevKey)));

            LOG_DEBUGH(_T("LEAVE: result={}"), result);
            return result;
        }

        // 一時的にこのフラグを立てることにより、履歴検索を行わないようになる
        bool bNoHistTemporary = false;

        // 一時的にこのフラグを立てることにより、自動モードでなくても履歴検索が実行されるようになる
        bool bManualTemporary = false;

        // 前回の履歴検索との比較、新しい履歴検索の開始 (bManual=trueなら自動モードでなくても履歴検索を実行する)
        void historySearch(bool bManual) {
            LOG_DEBUGH(_T("ENTER: auto={}, manual={}, maybeEditedBySubState={}, histInSearch={}"), \
                SETTINGS->autoHistSearchEnabled, bManual, maybeEditedBySubState, HIST_CAND->IsHistInSearch());
            if (!SETTINGS->autoHistSearchEnabled && !bManual) {
                // 履歴検索状態ではないので、前回キーをクリアしておく。
                // こうしておかないと、自動履歴検索OFFのとき、たとえば、
                // 「エッ」⇒Ctrl+Space⇒「エッセンス」⇒Esc⇒「エッ」⇒「セ」追加⇒出力「エッセ」、キー=「エッ」のまま⇒再検索⇒「エッセセンス」となる
                LOG_DEBUGH(_T("Not Hist Search mode: Clear PrevKey"));
                STROKE_MERGER_NODE->ClearPrevHistState();
                HIST_CAND->ClearKeyInfo();
            } else {
                // 履歴検索可能状態である
                LOG_DEBUGH(_T("Auto or Manual"));
                // 前回の履歴選択の出力と現在の出力文字列(改行以降)の末尾を比較する。
                // たとえば前回「中」で履歴検索し「中納言家持」が履歴出力されており、現在の出力スタックが「・・・中納言家持」なら true が返る
                bool bSameOut = !bManual && histBase->isLastHistOutSameAsCurrentOut();
                LOG_DEBUGH(_T("bSameOut={}, maybeEditedBySubState={}, histInSearch={}"), \
                    bSameOut, maybeEditedBySubState, HIST_CAND->IsHistInSearch());
                if (bSameOut && !maybeEditedBySubState && HIST_CAND->IsHistInSearch()) {
                    // 前回履歴出力が取得できた、つまり出力文字列の末尾が前回の履歴選択と同じ出力だったら、出力文字列をキーとした履歴検索はやらない
                    // これは、たとえば「中」で履歴検索し、「中納言家持」を選択した際に、キーとして返される「中納言家持」の末尾の「持」を拾って「持統天皇」を履歴検索してしまうことを防ぐため。
                    LOG_DEBUGH(_T("Do nothing: prevOut is same as current out"));
                } else {
                    // ただし、交ぜ書き変換など何か後続状態により出力がなされた場合(maybeEditedBySubState)は、履歴検索を行う。
                    LOG_DEBUGH(_T("DO HistSearch: prevOut is diff with current out or maybeEditedBySubState or not yet HistInSearch"));
                    // 現在の出力文字列は履歴選択したものではなかった
                    // キー取得用 lambda
                    auto keyGetter = []() {
                        // まず、ワイルドカードパターンを試す
                        auto key9 = OUTPUT_STACK->GetLastOutputStackStrUptoBlocker(9);
                        LOG_DEBUGH(_T("HistSearch: key9={}"), to_wstr(key9));
                        if (key9.empty() || key9.back() == ' ') {
                            return EMPTY_MSTR;
                        }
                        auto items = utils::split(key9, '*');
                        size_t nItems = items.size();
                        if (nItems >= 2) {
                            size_t len0 = items[nItems - 2].size();
                            size_t len1 = items[nItems - 1].size();
                            if (len0 > 0 && len1 > 0 && len1 <= 4) {
                                LOG_DEBUGH(_T("WILDCARD: key={}"), to_wstr(utils::last_substr(key9, len1 + 5)));
                                return utils::last_substr(key9, len1 + 5);
                            }
                        }
                        // ワイルドカードパターンでなかった
                        LOG_DEBUGH(_T("NOT WILDCARD, GetLastKanjiOrKatakanaOrHirakanaOrAsciiKey"));
                        // 出力文字から、ひらがな交じりやASCIIもキーとして取得する
                        auto jaKey = OUTPUT_STACK->GetLastKanjiOrKatakanaOrHirakanaOrAlphabetKey<MString>(SETTINGS->histMapKeyMaxLength);
                        LOG_DEBUGH(_T("HistSearch: jaKey={}"), to_wstr(jaKey));
                        if (jaKey.size() >= 9 || (!jaKey.empty() && is_alphabet(jaKey.back()))) {
                            // 同種の文字列で9文以上取れたか、またはアルファベットだったので、これをキーとする
                            return jaKey;
                        }
                        // 最終的には末尾8文字をキーとする('*' は含まない。'?' は含んでいる可能性あり)
                        LOG_DEBUGH(_T("HistSearch: tail_substr(key9, 8)={}"), to_wstr(utils::tail_substr(key9, 8)));
                        return utils::tail_substr(key9, 8);
                    };
                    // キーの取得
                    MString key = keyGetter();
                    LOG_DEBUGH(_T("HistSearch: LastJapaneseKey={}"), to_wstr(key));
                    if (!key.empty() && key.find(MSTR_CMD_HEADER) > key.size()) {
                        // キーが取得できた
                        //bool isAscii = is_ascii_char((wchar_t)utils::safe_back(key));
                        LOG_DEBUGH(_T("HistSearch: PATH 8: key={}, prevKey={}, maybeEditedBySubState={}"),
                            to_wstr(key), to_wstr(STROKE_MERGER_NODE->GetPrevKey()), maybeEditedBySubState);
                        auto histCandsChecker = [this](const std::vector<MString>& words, const MString& ky) {
                            LOG_DEBUGH(_T("HistSearch: CANDS CHECKER: words({})={}, key={}"), words.size(), to_wstr(utils::join(words, '/', 10)), to_wstr(ky));
                            if (words.empty() || (words.size() == 1 && (words[0].empty() || words[0] == ky))) {
                                LOG_DEBUGH(_T("HistSearch: CANDS CHECKER-A: cands size <= 1"));
                                // 候補が1つだけで、keyに一致するときは履歴選択状態にはしない
                            } else {
                                LOG_DEBUGH(_T("HistSearch: CANDS CHECKER-B"));
                                if (SETTINGS->autoHistSearchEnabled || SETTINGS->showHistCandsFromFirst) {
                                    // 初回の履歴選択でも横列候補表示を行う
                                    histBase->setCandidatesVKB(VkbLayout::Horizontal, words, ky);
                                }
                            }
                        };
                        if (key != STROKE_MERGER_NODE->GetPrevKey() || maybeEditedBySubState || bManual) {
                            LOG_DEBUGH(_T("HistSearch: PATH 9: different key"));
                            //bool bCheckMinKeyLen = !bManual && utils::is_hiragana(key[0]);       // 自動検索かつ、キー先頭がひらがなならキー長チェックをやる
                            bool bCheckMinKeyLen = !bManual;                                     // 自動検索ならキー長チェックをやる
                            histCandsChecker(HIST_CAND->GetCandWords(key, bCheckMinKeyLen, 0), key);
                            // キーが短くなる可能性があるので再取得
                            key = HIST_CAND->GetCurrentKey();
                            LOG_DEBUGH(_T("HistSearch: PATH 10: currentKey={}"), to_wstr(key));
                        } else {
                            // 前回の履歴検索と同じキーだった
                            LOG_DEBUGH(_T("HistSearch: PATH 11: Same as prev hist key"));
                            histCandsChecker(HIST_CAND->GetCandWords(), key);
                        }
                    }
                    LOG_DEBUGH(_T("HistSearch: SetPrevHistKeyState(key={})"), to_wstr(key));
                    STROKE_MERGER_NODE->SetPrevHistKeyState(key);
                    LOG_DEBUGH(_T("DONE HistSearch"));
                }
            }

            // この処理は、GUI側で候補の背景色を変更するために必要
            if (isHotCandidateReady(STROKE_MERGER_NODE->GetPrevKey(), HIST_CAND->GetCandWords())) {
                LOG_DEBUGH(_T("PATH 14"));
                // 何がしかの文字出力があり、それをキーとする履歴候補があった場合 -- 未選択状態にセットする
                LOG_DEBUGH(_T("Set Unselected"));
                STATE_COMMON->SetWaitingCandSelect(-1);
                bCandSelectable = true;
                LOG_DEBUGH(_T("bCandSelectable=True"));
            }
            maybeEditedBySubState = false;

            LOG_DEBUGH(_T("LEAVE"));
        }

    public:
        // 最終的な出力履歴が整ったところで呼び出される処理
        void DoLastHistoryProc() override {
            LOG_DEBUGH(_T("\nENTER: {}: {}"), Name, OUTPUT_STACK->OutputStackBackStrForDebug(10));
            LOG_DEBUGH(_T("PATH 2: bCandSelectable={}"), bCandSelectable);

            if (bCandSelectable && wasCandSelectCalled()) {
                LOG_DEBUGH(_T("PATH 3: by SelectionKey"));
                // 履歴選択キーによる処理だった場合
                if (isEnterDecKey()) {
                    LOG_DEBUGH(_T("PATH 4-A: handleEnter: setHistBlocker()"));
                    bCandSelectable = false;
                    OUTPUT_STACK->setHistBlocker();
                    //LOG_DEBUGH(_T("PATH 4-A: handleEnter: pushNewLine()"));
                    //OUTPUT_STACK->pushNewLine();setHistBlocker
                } else {
                    LOG_DEBUGH(_T("PATH 4-B: handleArrow"));
                    // この処理は、GUI側で候補の背景色を変更するのと矢印キーをホットキーにするために必要
                    LOG_DEBUGH(_T("Set selectedPos={}"), HIST_CAND->GetSelectPos());
                    STATE_COMMON->SetWaitingCandSelect(HIST_CAND->GetSelectPos());
                }
            } else {
                LOG_DEBUGH(_T("PATH 5: by Other Input"));
                // その他の文字出力だった場合
                HIST_CAND->DelayedPushFrontSelectedWord();
                bCandSelectable = false;

                LOG_DEBUGH(_T("PATH 6: bCandSelectable={}, bNoHistTemporary={}"), bCandSelectable, bNoHistTemporary);
                if (OUTPUT_STACK->isLastOutputStackCharBlocker()) {
                    LOG_DEBUGH(_T("PATH 7: LastOutputStackChar is Blocker"));
                    HISTORY_DIC->ClearNgramSet();
                }

                // 前回の履歴検索との比較、新しい履歴検索の開始
                if (bNoHistTemporary) {
                    // 一時的に履歴検索が不可になっている場合は、キーと出力文字列を比較して、異った状態になっていたら可に戻す
                    MString prevKey = STROKE_MERGER_NODE->GetPrevKey();
                    MString outStr = OUTPUT_STACK->GetLastOutputStackStrUptoBlocker(prevKey.size());
                    bNoHistTemporary = OUTPUT_STACK->GetLastOutputStackStrUptoBlocker(prevKey.size()) == prevKey;
                    LOG_DEBUGH(_T("PATH 8: bNoHistTemporary={}: prevKey={}, outStr={}"), bNoHistTemporary, to_wstr(prevKey), to_wstr(outStr));
                }

                LOG_DEBUGH(_T("PATH 9: bNoHistTemporary={}"), bNoHistTemporary);
                if (!bNoHistTemporary) {
                    if (isEditingFuncDecKey()) {
                        //// 編集用キーが呼び出されたので、全ブロッカーを置く
                        //LOG_DEBUGH(_T("PATH 9: EditingFuncDecKey: pushNewLine()"));
                        //OUTPUT_STACK->pushNewLine();
                        // 編集用キーが呼び出されたので、ブロッカーを置く
                        LOG_DEBUGH(_T("PATH 9: EditingFuncDecKey: setHistBlocker()"));
                        OUTPUT_STACK->setHistBlocker();
                    } else {
                        historySearch(bManualTemporary);
                    }
                }
                //bNoHistTemporary = false;
                //bManualTemporary = false;
            }

            bNoHistTemporary = false;
            bManualTemporary = false;

            LOG_DEBUGH(_T("LEAVE: {}\n"), Name);
        }

    private:
        void copyEditBufferToOutputStack() {
            LOG_DEBUGH(_T("ENTER: {}"), Name);
            auto editBuf = STATE_COMMON->GetEditBufferString();
            auto blockedTail = OUTPUT_STACK->GetLastOutputStackStrUptoBlocker(20);
            if (!blockedTail.empty()) {
                auto cleanTail = OUTPUT_STACK->BackStringUptoNewLine(blockedTail.size() + 8);
                size_t cleanStemLen = cleanTail.size() > blockedTail.size() ? cleanTail.size() - blockedTail.size() : 0;
                if (cleanStemLen > 0) {
                    auto cleanStem = cleanTail.substr(0, cleanStemLen);
                    // cleanStem の末尾部分と editBuf の中間部分(末尾を除く)で一致する最大部分を探索
                    if (editBuf.size() > 1) {
                        size_t bestPos = MString::npos;
                        size_t bestLen = 0;
                        size_t maxCheckLen = std::min(cleanStemLen, editBuf.size() - 1); // 中間に収まる最大長

                        for (size_t len = maxCheckLen; len > 0; --len) {
                            // cleanStem の末尾 len 文字
                            auto suffix = cleanStem.substr(cleanStemLen - len, len);

                            // editBuf 内を探索（中間: 位置 >=0 かつ 末尾にかからない）
                            size_t searchStart = 0;
                            while (searchStart + len < editBuf.size()) {
                                size_t pos = editBuf.find(suffix, searchStart);
                                if (pos == MString::npos) break;
                                if (pos + len < editBuf.size()) {
                                    bestPos = pos;
                                    bestLen = len;
                                    goto FOUND_OVERLAP; // 最大長から降順なので最初に見つかったものが最大
                                }
                                searchStart = pos + 1;
                            }
                        }
                    FOUND_OVERLAP:
                        // 見つかった場合は editBuf.insert(.substr(bestPos+bestLen) をOutput 
                        if (bestLen > 0 && bestPos != MString::npos) {
                            LOG_DEBUGH(_T("Overlap FOUND: {}"), to_wstr(editBuf.substr(bestPos, bestLen)));
                            LOG_DEBUGH(_T("Insert '|' at {} in EditBufString={}"), bestPos + bestLen, to_wstr(editBuf));
                            editBuf.insert(bestPos + bestLen, 1, '|');
                        }
                    }
                }
            }
            // OUTPUT_STACK に editBuf を追加する
            LOG_DEBUG(_T("LEAVE: Push EditBuffer to OutputStack: {}"), to_wstr(editBuf));
            OUTPUT_STACK->pushNewString(STATE_COMMON->GetEditBufferString());
        }

        bool isCandSelectable() const {
            //size_t tail_size = OUTPUT_STACK->TailSizeUptoMazeOrHistBlockerOrPunct();
            //if (tail_size > 0) return false;
            //auto blockedTail = OUTPUT_STACK->OutputStackBackStrWithFlagUpto(20);
            //if (blockedTail.empty() || blockedTail.back() != '|') return false;
            //size_t tail_pos = blockedTail.size() - 1;
            //size_t pos = blockedTail.rfind('|', tail_pos);
            return OUTPUT_STACK->tail_size() > 0 && OUTPUT_STACK->TailSizeUptoMazeOrHistBlockerOrPunct() == 0;
        }

    public:
        // (Ctrl or Shift)+Space の処理 -- 履歴検索の開始、次の候補を返す
        void handleNextOrPrevCandTrigger(bool bNext) {
            bCandSelectable = isCandSelectable();
            LOG_DEBUGH(_T("\nCALLED: {}: bCandSelectable={}, selectPos={}, bNext={}"), Name, bCandSelectable, HIST_CAND->GetSelectPos(), bNext);
            // これにより、前回のEnterによる改行点挿入やFullEscapeによるブロッカーフラグが削除される⇒(2021/12/18)workしなくなっていたので、いったん削除
            //OUTPUT_STACK->clearFlagAndPopNewLine();
            // 今回、履歴選択用ホットキーだったことを保存
            setCandSelectIsCalled();

            // 自動履歴検索が有効になっているか、初回から履歴候補の横列表示をするか、または2回目以降の履歴検索の場合は、履歴候補の横列表示あり
            bool bShowHistCands = SETTINGS->autoHistSearchEnabled || SETTINGS->showHistCandsFromFirst || bCandSelectable;

            if (!bCandSelectable) {
                // 履歴候補選択可能状態でなければ、前回の履歴検索との比較、新しい履歴検索の開始
                copyEditBufferToOutputStack();
                historySearch(true);
            }
            if (bCandSelectable) {
                LOG_DEBUGH(_T("CandSelectable: bNext={}"), bNext);
                if (bNext)
                    getNextCandidate(bShowHistCands);
                else
                    getPrevCandidate(bShowHistCands);
            } else {
                LOG_DEBUGH(_T("NOP"));
            }
            LOG_DEBUGH(_T("LEAVE\n"));
        }

        // 0～9 を処理する
        void handleStrokeKeys(int deckey) {
            LOG_DEBUGH(_T("ENTER: {}: deckey={}, bCandSelectable={}"), Name, deckey, bCandSelectable);
            if (bCandSelectable) {
                setCandSelectIsCalled();
                getPosCandidate((size_t)deckey);
            }
            LOG_DEBUGH(_T("LEAVE"));
        }

        //// Shift+Space の処理 -- 履歴検索の開始、次の候補を返す
        //void handleShiftSpace() {
        //    LOG_DEBUGH(_T("CALLED: {}"), Name);
        //    handleNextOrPrevCandTrigger(true);
        //}

        //// Ctrl+Space の処理 -- 履歴検索の開始、次の候補を返す
        //void handleCtrlSpace() {
        //    LOG_DEBUGH(_T("CALLED: {}"), Name);
        //    handleNextOrPrevCandTrigger(true);
        //}

        //// Ctrl+Shift+Space の処理 -- 履歴検索の開始、前の候補を返す
        //void handleCtrlShiftSpace() {
        //    LOG_DEBUGH(_T("CALLED: {}"), Name);
        //    handleNextOrPrevCandTrigger(false);
        //}

        // NextCandTrigger の処理 -- 履歴検索の開始、次の候補を返す
        void handleNextCandTrigger() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            handleNextOrPrevCandTrigger(true);
        }

        // PrevCandTrigger の処理 -- 履歴検索の開始、前の候補を返す
        void handlePrevCandTrigger() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            handleNextOrPrevCandTrigger(false);
        }

        // ↓の処理 -- 次候補を返す
        void handleDownArrow() override {
            LOG_DEBUGH(_T("ENTER: {}: bCandSelectable={}"), Name, bCandSelectable);
            if (SETTINGS->useArrowToSelCand && bCandSelectable) {
                setCandSelectIsCalled();
                getNextCandidate();
            } else {
                LOG_DEBUGH(_T("candSelectDeckey={:x}"), candSelectDeckey);
                State::handleDownArrow();
            }
            LOG_DEBUGH(_T("LEAVE"));
        }

        // ↑の処理 -- 前候補を返す
        void handleUpArrow() override {
            LOG_DEBUGH(_T("ENTER: {}: bCandSelectable={}"), Name, bCandSelectable);
            if (SETTINGS->useArrowToSelCand && bCandSelectable) {
                setCandSelectIsCalled();
                getPrevCandidate();
            } else {
                LOG_DEBUGH(_T("candSelectDeckey={:x}"), candSelectDeckey);
                State::handleUpArrow();
            }
            LOG_DEBUGH(_T("LEAVE"));
        }

        // FlushOutputString の処理
        void handleFlushOutputString() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            // 英数モードはキャンセルする
            if (NextState()) NextState()->handleEisuCancel();
            handleFullEscape();
            STATE_COMMON->SetFlushOutputString();
            LOG_DEBUGH(_T("LEAVE"));
        }

        // FullEscapeの処理 -- 履歴選択状態から抜けて、履歴検索文字列の遡及ブロッカーをセット
        void handleFullEscape() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            HIST_CAND->DelayedPushFrontSelectedWord();
            histBase->setBlocker();
            LOG_DEBUGH(_T("LEAVE"));
        }

        // Unblock の処理 -- 改行やブロッカーの除去
        void handleUnblock() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            // ブロッカー設定済みならそれを解除する
            OUTPUT_STACK->clearFlagAndPopNewLine();
            LOG_DEBUGH(_T("LEAVE"));
        }

        // Tab の処理 -- 次の候補を返す
        void handleTab() override {
            LOG_DEBUGH(_T("CALLED: {}: bCandSelectable={}"), Name, bCandSelectable);
            if (SETTINGS->selectHistCandByTab && bCandSelectable) {
                setCandSelectIsCalled();
                getNextCandidate();
            } else {
                State::handleTab();
            }
        }

        // ShiftTab の処理 -- 前の候補を返す
        void handleShiftTab() override {
            LOG_DEBUGH(_T("CALLED: {}: bCandSelectable={}"), Name, bCandSelectable);
            if (SETTINGS->selectHistCandByTab && bCandSelectable) {
                setCandSelectIsCalled();
                getPrevCandidate();
            } else {
                State::handleShiftTab();
            }
        }

        // Ctrl-H/BS の処理 -- 履歴検索の初期化
        void handleBS() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            STROKE_MERGER_NODE->ClearPrevHistState();
            HIST_CAND->ClearKeyInfo();
            if (WORD_LATTICE->isEmpty()) {
                State::handleBS();
            }
        }

        // DecoderOff の処理
        void handleDecoderOff() override {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            // Enter と同じ扱いにする
            AddNewHistEntryOnEnter();
            State::handleDecoderOff();
        }
        
        // RET/Enter の処理
        void handleEnter() override {
            LOG_DEBUGH(_T("ENTER: {}: bCandSelectable={}, selectPos={}"), Name, bCandSelectable, HIST_CAND->GetSelectPos());
            if (SETTINGS->selectFirstCandByEnter && bCandSelectable && HIST_CAND->GetSelectPos() < 0) {
                // 選択可能状態かつ候補未選択なら第1候補を返す。
                LOG_DEBUGH(_T("CALL: getNextCandidate(false)"));
                setCandSelectIsCalled();
                getNextCandidate(false);
            } else if (bCandSelectable && HIST_CAND->GetSelectPos() >= 0) {
                LOG_DEBUGH(_T("CALL: STROKE_MERGER_NODE->ClearPrevHistState(); HIST_CAND->ClearKeyInfo(); bManualTemporary = true"));
                // どれかの候補が選択されている状態なら、それを確定し、履歴キーをクリアしておく
                STROKE_MERGER_NODE->ClearPrevHistState();
                HIST_CAND->ClearKeyInfo();
                // 一時的にマニュアル操作フラグを立てることで、DoLastHistoryProc() から historySearch() を呼ぶときに履歴再検索が実行されるようにする
                bManualTemporary = true;
                if (SETTINGS->newLineWhenHistEnter) {
                    // 履歴候補選択時のEnterではつねに改行するなら、確定後、Enter処理を行う
                    State::handleEnter();
                }
            } else {
                // それ以外は通常のEnter処理
                LOG_DEBUGH(_T("CALL: AddNewHistEntryOnEnter()"));
                AddNewHistEntryOnEnter();
                State::handleEnter();
            }
            //// 前回の句読点から末尾までの出力文字列に対して Ngram解析を行う
            //LOG_DEBUGH(L"CALL WORD_LATTICE->updateRealtimeNgram()");
            //WORD_LATTICE->updateRealtimeNgram();
            // フロントエンドから updateRealtimeNgram() を呼び出すので、ここではやる必要がない

            LOG_DEBUGH(_T("LEAVE"));
        }

        //// Ctrl-J の処理 -- 選択可能状態かつ候補未選択なら第1候補を返す。候補選択済みなら確定扱い
        //void handleCtrlJ() {
        //    LOG_DEBUGH(_T("\nCALLED: {}: selectPos={}"), Name, HIST_CAND->GetSelectPos());
        //    //setCandSelectIsCalled();
        //    if (bCandSelectable) {
        //        if (HIST_CAND->GetSelectPos() < 0) {
        //            // 選択可能状態かつ候補未選択なら第1候補を返す。
        //            getNextCandidate();
        //        } else {
        //            // 確定させる
        //            HIST_CAND->DelayedPushFrontSelectedWord();
        //            histBase->setBlocker();
        //        }
        //    } else {
        //        // Enterと同じ扱い
        //        AddNewHistEntryOnEnter();
        //        State::handleCtrlJ();
        //    }
        //}

        // Esc の処理 -- 処理のキャンセル
        void handleEsc() override {
            LOG_DEBUGH(_T("CALLED: {}, bCandSelectable={}, SelectPos={}, EisuPrevCount={}, TotalCount={}"),
                Name, bCandSelectable, HIST_CAND->GetSelectPos(), EISU_NODE->prevHistSearchDeckeyCount, STATE_COMMON->GetTotalDecKeyCount());
            if (bCandSelectable && HIST_CAND->GetSelectPos() >= 0) {
                LOG_DEBUGH(_T("Some Cand Selected"));
                // どれかの候補が選択されている状態なら、選択のリセット
                if (STATE_COMMON->GetTotalDecKeyCount() == EISU_NODE->prevHistSearchDeckeyCount + 1) {
                    // 直前に英数モードから履歴検索された場合
                    LOG_DEBUGH(_T("SetNextNode: EISU_NODE"));
                    resetCandSelect(false);     // false: 仮想鍵盤表示を履歴選択モードにしない
                    // 一時的にこのフラグを立てることにより、履歴検索を行わないようにする
                    bNoHistTemporary = true;
                    // 再度、英数モード状態に入る
                    SetNextNodeMaybe(EISU_NODE);
                    //STATE_COMMON->SetNormalVkbLayout();
                } else {
                    resetCandSelect(true);
                    // 一時的にマニュアル操作フラグを立てることで、DoLastHistoryProc() から historySearch() を呼ぶときに履歴再検索が実行されるようにする
                    bManualTemporary = true;
                }
            } else {
                LOG_DEBUGH(_T("No Cand Selected"));
                // 一時的にこのフラグを立てることにより、履歴検索を行わないようにする
                bNoHistTemporary = true;
                // Esc処理が必要なものがあればそれをやる。なければアクティブウィンドウにEscを送る
                ResidentState::handleEsc();
                //// 何も候補が選択されていない状態なら履歴選択状態から抜ける
                //STATE_COMMON->SetHistoryBlockFlag();
                //State::handleEsc();
                //// 完全に抜ける
                //handleFullEscape();
            }
            LOG_DEBUGH(_T("LEAVE"));
        }

        //// Ctrl-U
        //void handleCtrlU() {
        //    LOG_DEBUGH(_T("CALLED: {}"), Name);
        //    STATE_COMMON->SetBothHistoryBlockFlag();
        //    State::handleCtrlU();
        //}

    private:
        // 次の候補を返す処理
        void getNextCandidate(bool bSetVkb = true) {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            outputHistResult(HIST_CAND->GetNext(), bSetVkb);
        }

        // 前の候補を返す処理
        void getPrevCandidate(bool bSetVkb = true) {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            outputHistResult(HIST_CAND->GetPrev(), bSetVkb);
        }

        // 次の候補を返す処理
        void getPosCandidate(size_t pos, bool bSetVkb = true) {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            outputHistResult(HIST_CAND->GetPositionedHist(pos), bSetVkb);
        }

        // 選択のリセット
        void resetCandSelect(bool bSetVkb) {
            LOG_DEBUGH(_T("CALLED: {}"), Name);
            outputHistResult(HIST_CAND->ClearSelectPos(), bSetVkb);
            STATE_COMMON->SetWaitingCandSelect(-1);
        }

        // 履歴結果出力 (bSetVKb = false なら、仮想鍵盤表示を履歴選択モードにしない; 英数モードから履歴検索をした直後のESCのケース)
        void outputHistResult(const HistResult& result, bool bSetVkb) {
            LOG_DEBUGH(_T("ENTER: {}: bSetVkb={}"), Name, bSetVkb);
            histBase->getLastHistKeyAndRewindOutput(resultStr);    // 前回の履歴検索キー取得と出力スタックの巻き戻し予約(numBackSpacesに値をセット)

            histBase->setOutString(result, resultStr);
            if (!result.Word.empty() && (result.Word.find(VERT_BAR) == MString::npos || utils::contains_ascii(result.Word))) {
                // 何か履歴候補(英数字を含まない変換形履歴以外)が選択されたら、ブロッカーを設定する (emptyの場合は元に戻ったので、ブロッカーを設定しない)
                LOG_DEBUGH(_T("SetHistoryBlocker"));
                STATE_COMMON->SetHistoryBlockFlag();
            }
            if (bSetVkb) histBase->setCandidatesVKB(VkbLayout::Horizontal, HIST_CAND->GetCandWords(), HIST_CAND->GetCurrentKey());

            // 英数モードはキャンセルする
            if (NextState()) NextState()->handleEisuCancel();

            LOG_DEBUGH(_T("LEAVE: prevOut={}, numBS={}"), to_wstr(STROKE_MERGER_NODE->GetPrevOutString()), resultStr.numBS());
        }

    };
    DEFINE_CLASS_LOGGER(StrokeMergerHistoryResidentStateImpl);

    DEFINE_LOCAL_LOGGER(StrokeMerger);

    // テーブルファイルを読み込んでストローク木を作成する
    void createStrokeTree(StringRef tableFile, void(*treeCreator)(StringRef, std::vector<String>&)) {
        _LOG_INFOH(_T("ENTER: tableFile={}"), tableFile);

        if (tableFile.empty()) {
            std::vector<String> emptyLines;
            LOG_DEBUGH(_T("No table file specified"));
            treeCreator(tableFile, emptyLines);
            LOG_DEBUGH(_T("empty table file created"));
        } else {
            utils::IfstreamReader reader(tableFile);
            if (reader.success()) {
                //auto lines = utils::IfstreamReader(tableFile).getAllLines();
                auto lines = reader.getAllLines();
                // ストロークノード木の構築
                treeCreator(tableFile, lines);
                _LOG_INFOH(_T("close table file: {}"), tableFile);
            } else {
                // エラー
                LOG_ERROR(_T("Can't read table file: {}"), tableFile);
                ERROR_HANDLER->Error(std::format(_T("テーブルファイル({})が開けません"), tableFile));
            }
        }

        _LOG_INFOH(_T("LEAVE"));
    }

} // namespace

// 履歴入力(常駐)機能状態インスタンスの Singleton
std::unique_ptr<StrokeMergerHistoryResidentState> StrokeMergerHistoryResidentState::_singleton;

void StrokeMergerHistoryResidentState::SetSingleton(StrokeMergerHistoryResidentState* pState) {
    _singleton.reset(pState);
}

StrokeMergerHistoryResidentState* StrokeMergerHistoryResidentState::Singleton() {
    return _singleton.get();
}

// -------------------------------------------------------------------
// StrokeMergerHistoryNode - マージ履歴機能 常駐ノード
DEFINE_CLASS_LOGGER(StrokeMergerHistoryNode);

// コンストラクタ
StrokeMergerHistoryNode::StrokeMergerHistoryNode() {
    _LOG_INFOH(_T("CALLED: constructor"));
}

// デストラクタ
StrokeMergerHistoryNode::~StrokeMergerHistoryNode() {
    _LOG_INFOH(_T("CALLED: destructor"));
}

// StrokeMergerNode::Singleton
std::unique_ptr<StrokeMergerHistoryNode> StrokeMergerHistoryNode::Singleton;

void StrokeMergerHistoryNode::CreateSingleton() {
    Singleton.reset(new StrokeMergerHistoryNode());
}

// マージ履歴機能常駐ノードの初期化
void StrokeMergerHistoryNode::Initialize() {
    LOG_INFOH(L"ENTER");
    // 履歴入力辞書ファイル名
    auto histFile = SETTINGS->historyFile;
    auto sysRomanFile = SETTINGS->historySystemRomanFile;
    LOG_DEBUGH(_T("histFile={}"), histFile);
    // 履歴入力辞書の読み込み(ファイル名の指定がなくても辞書自体は構築する)
    LOG_DEBUGH(_T("CALLED: histFile={}, sysRomanFile={}"), histFile, sysRomanFile);
    HistoryDic::CreateHistoryDic(histFile, sysRomanFile);

    HistCandidates::CreateSingleton();

    FunctionNodeManager::CreateFunctionNodeByName(_T("history"));

    StrokeMergerHistoryNode::CreateSingleton();
    LOG_INFOH(L"LEAVE");
}

// -------------------------------------------------------------------
// 当ノードを処理する State インスタンスを作成する
State* StrokeMergerHistoryNode::CreateState() {
    StrokeMergerHistoryResidentState::SetSingleton(new StrokeMergerHistoryResidentStateImpl(this));
    return MERGER_HISTORY_RESIDENT_STATE;
}

// テーブルファイルを読み込んでストローク木を作成する
// targetTable: 0=全テーブル, 1=主テーブルのみ, 2=副テーブルのみ (0以外の場合は multiStreamMode/multiCandidateMode を無効にして単一テーブルモードにする; テスト用)
void StrokeMergerHistoryNode::createStrokeTrees(int targetTable) {
#define STROKE_TREE_CREATOR(F) [](StringRef file, std::vector<String>& lines) {F(file, lines);}

    if (targetTable != 0) {
        SETTINGS->multiStreamMode = false;
        SETTINGS->multiCandidateMode = false;
        SETTINGS->tableFile = targetTable == 1 ? L"dummy1" : L"";
        SETTINGS->tableFile2 = targetTable == 2 ? L"dummy2" : L"";
        SETTINGS->tableFile3 = L"";
    }

    // 主テーブルファイルの構築
    auto tableFile1 = !SETTINGS->tableFile.empty() ? utils::joinPath(SETTINGS->rootDir, _T("tmp\\tableFile1.tbl")) : L"";
    createStrokeTree(tableFile1, STROKE_TREE_CREATOR(StrokeTableNode::CreateStrokeTree));

    // 副テーブルファイルの構築
    auto tableFile2 = !SETTINGS->tableFile2.empty() ? utils::joinPath(SETTINGS->rootDir, _T("tmp\\tableFile2.tbl")) : L"";
    createStrokeTree(tableFile2, STROKE_TREE_CREATOR(StrokeTableNode::CreateStrokeTree2));

    // 第3テーブルファイルの構築
    auto tableFile3 = !SETTINGS->tableFile3.empty() ? utils::joinPath(SETTINGS->rootDir, _T("tmp\\tableFile3.tbl")) : L"";
    createStrokeTree(tableFile3, STROKE_TREE_CREATOR(StrokeTableNode::CreateStrokeTree3));
}
