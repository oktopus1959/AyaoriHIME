#include "Logger.h"
//
#include "settings.h"
#include "StateCommonInfo.h"
#include "BushuComp/BushuComp.h"
#include "BushuComp/BushuDic.h"
#include "KeysAndChars/EasyChars.h"
//
#include "Lattice.h"
#include "Lattice2_Common.h"
#include "Lattice2_CandidateString.h"

#include "MorphBridge.h"

#define _LOG_INFOH LOG_INFO
#define _LOG_DETAIL LOG_DEBUG
#if 1
#undef IS_LOG_DEBUGH_ENABLED
#define IS_LOG_DEBUGH_ENABLED true
#undef _LOG_INFOH
#undef _LOG_DETAIL
#undef LOG_INFO
#undef LOG_DEBUGH
#undef LOG_DEBUG
#define _LOG_INFOH LOG_WARN
#define _LOG_DETAIL LOG_WARN
#define LOG_INFO LOG_INFOH
#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFOH
#endif

namespace lattice2 {
    DECLARE_LOGGER;     // defined in Lattice2.cpp

    DEFINE_CLASS_LOGGER(CandidateString);

    // 候補文字列
    // 末尾文字列にマッチする RewriteInfo を取得する
    std::tuple<const RewriteInfo*, int> CandidateString::matchWithTailString(const PostRewriteOneShotNode* rewriteNode) const {
        size_t maxlen = SETTINGS->kanaTrainingMode && ROOT_STROKE_NODE->hasOnlyUsualRewriteNdoe() ? 0 : 8;     // かな入力練習モードで濁点のみなら書き換えをやらない
        bool bRollOverStroke = STATE_COMMON->IsRollOverStroke();
        _LOG_DETAIL(_T("bRollOverStroke={}"), bRollOverStroke);
        while (maxlen > 0) {
            LOG_DEBUGH(_T("maxlen={}"), maxlen);
            const MString targetStr = utils::safe_tailstr(_str, maxlen);
            LOG_DEBUGH(_T("targetStr={}"), to_wstr(targetStr));
            if (targetStr.empty()) break;

            const RewriteInfo* rewInfo = 0;
            if (bRollOverStroke) rewInfo = rewriteNode->getRewriteInfo(targetStr + MSTR_PLUS);        // ロールオーバー打ちのときは"+"を付加したエントリを検索
            if (!rewInfo) rewInfo = rewriteNode->getRewriteInfo(targetStr);
            if (rewInfo) {
                LOG_DEBUGH(_T("REWRITE_INFO found: outStr={}, rewritableLen={}, subTable={:p}"), to_wstr(rewInfo->rewriteStr), rewInfo->rewritableLen, (void*)rewInfo->subTable);
                return { rewInfo, (int)targetStr.size() };
            }

            maxlen = targetStr.size() - 1;
        }
        return { 0, 0 };
    }

    // 複数文字が設定されているストロークを1文字ずつに分解
    std::vector<MString> CandidateString::split_piece_str(const MString& s) const {
        if (s.size() == 1 && s.front() == '|') {
            return std::vector<MString>(1, s);
        } else {
            return utils::split(s, '|');
        }
    }

    MString CandidateString::convertoToKatakanaIfAny(const MString& str, bool bKatakana) const {
        MString result = str;
        if (bKatakana) {
            for (size_t i = 0; i < result.size(); ++i) {
                mchar_t ch = (wchar_t)result[i];
                if (utils::is_hiragana(ch)) { result[i] = utils::hiragana_to_katakana(ch); }
            }
        }
        return result;
    }

    // グローバルな後置書き換えを適用する
    MString CandidateString::applyGlobalPostRewrite(const MString& adder) const {
        size_t maxlen = SETTINGS->kanaTrainingMode && ROOT_STROKE_NODE->hasOnlyUsualRewriteNdoe() ? 0 : 8;     // かな入力練習モードで濁点のみなら書き換えをやらない
        if (maxlen > _str.size()) maxlen = _str.size();
        for (size_t len = maxlen; len > 0; --len) {
            MString key = utils::safe_tailstr(_str, len) + adder;
            LOG_DEBUGH(_T("key={}"), to_wstr(key));
            auto iter = globalPostRewriteMap.find(key);
            if (iter != globalPostRewriteMap.end()) {
                // 書き換え文字列が見つかった
                LOG_DEBUGH(_T("FOUND: rewrite={}"), to_wstr(iter->second));
                return len < _str.size() ? utils::safe_substr(_str, 0, _str.size() - len) + iter->second : iter->second;
            }
        }
        return _str + adder;
    }

    // 自動部首合成の実行
    std::tuple<MString, int> CandidateString::applyAutoBushu(const WordPiece& piece, int strokeCount) const {
        LOG_DEBUGH(_T("CALLED: _str={}, _strokeLen={}, piece={}, strokeCount={}"), to_wstr(_str), piece.debugString(), _strokeLen, strokeCount);
        MStringResult resultOut;
        if (_strokeLen + piece.strokeLen() == strokeCount) {
            if (SETTINGS->autoBushuCompMinCount > 0 && BUSHU_DIC) {
                // 自動部首合成が有効である
                const MString& pieceStr = piece.getString();
                if (_str.size() > 0 && (pieceStr.size() == 1 || (pieceStr.size() > 1 && pieceStr[1] == '|'))) {
                    // 自動部首合成の実行 (複数文字がある場合は先頭の文字だけを対象)
                    LOG_DEBUGH(L"CALL ReduceByAutoBushu({}, {})", (wchar_t)_str.back(), (wchar_t)pieceStr.front());
                    BUSHU_COMP_NODE->ReduceByAutoBushu(_str.back(), pieceStr.front(), resultOut);
                    if (!resultOut.resultStr().empty()) {
                        MString s(_str);
                        s.back() = resultOut.resultStr().front();
                        return { s, 1 };
                    }
                    //mchar_t m = BUSHU_DIC->FindAutoComposite(_str.back(), piece.getString().front());
                    ////if (m == 0) m = BUSHU_DIC->FindComposite(_str.back(), piece.getString().front(), 0);
                    //LOG_DEBUGH(_T("BUSHU_DIC->FindAutoComposite({}, {}) -> {}"),
                    //    to_wstr(utils::safe_tailstr(_str, 1)), to_wstr(utils::safe_substr(piece.getString(), 0, 1)), to_wstr(m));
                    //if (m != 0) {
                    //    MString s(_str);
                    //    s.back() = m;
                    //    return { s, 1 };
                    //}
                }
            }
        }
        // 空を返す
        return { resultOut.resultStr(), resultOut.numBS() };
    }

    // 部首合成の実行
    MString CandidateString::applyBushuComp() const {
        if (BUSHU_DIC) {
            if (_str.size() >= 2) {
                // 部首合成の実行
                MString ms = BUSHU_COMP_NODE->ReduceByBushu(_str[_str.size() - 2], _str[_str.size() - 1]);
                LOG_DEBUGH(_T("BUSHU_COMP_NODE->ReduceByBushu({}, {}) -> {}"),
                    to_wstr(_str[_str.size() - 2]), to_wstr(_str[_str.size() - 1]), to_wstr(ms));
                if (!ms.empty()) {
                    MString s(_str.substr(0, _str.size() - 2));
                    s.append(1, ms[0]);
                    return s;
                }
                //mchar_t m = BUSHU_DIC->FindComposite(_str[_str.size() - 2], _str[_str.size() - 1], '\0');
                //LOG_DEBUGH(_T("BUSHU_DIC->FindComposite({}, {}) -> {}"),
                //    to_wstr(_str[_str.size() - 2]), to_wstr(_str[_str.size() - 1]), to_wstr(m));
                //if (m != 0) {
                //    MString s(_str.substr(0, _str.size() - 2));
                //    s.append(1, m);
                //    return s;
                //}
            }
        }
        return EMPTY_MSTR;
    }

    // リアルタイムNgramの更新
    void updateRealtimeNgram(const MString& str);

    // 交ぜ書き変換の実行
    std::vector<CandidateString> CandidateString::applyMazegaki() const {
        _LOG_DETAIL(L"ENTER: {}", to_wstr(_str));
        EASY_CHARS->DumpEasyCharsMemory();

        // リアルタイムNgramの更新
        updateRealtimeNgram(_str);

        MString tail;
        MString head;
        std::vector<MString> cands;

        std::vector<MString> words;
        // まず、交ぜ書き優先で形態素解析する
        int mazePenalty = -1;           // -SETTINGS->morphMazeEntryPenalty
        int mazeConnPenalty = 3000;     // -SETTINGS->morphMazeConnectionPenalty
        _LOG_DETAIL(L"MorphBridge::morphCalcCost(_str={}, words, mazePenalty={}, mazeConnPenalty={}, allowNonTerminal=False)", to_wstr(_str), mazePenalty, mazeConnPenalty);
        int cost = MorphBridge::morphCalcCost(_str, words, mazePenalty, mazeConnPenalty, false);
        _LOG_DETAIL(L"{}: orig morph cost={}, morph={}", to_wstr(_str), cost, to_wstr(utils::join(words, '/')));
        bool tailMaze = false;          // 末尾の交ぜ書きのみが置換される
        for (auto iter = words.rbegin(); iter != words.rend(); ++iter) {
            _LOG_DETAIL(L"morph: {}", to_wstr(*iter));
            auto items = utils::split(*iter, '\t');
            if (!items.empty()) {
                const MString& surf = items[0];
                if (tailMaze) {
                    head.insert(0, surf);
                } else if (items.size() == 1 || items[1] == MSTR_MINUS) {
                    // 変換後文字列が定義されていない
                    tail.insert(0, surf);
                } else {
                    const MString& mazeCands = items[1];
                    getDifficultMazeCands(surf, mazeCands, cands);
                    // 末尾の交ぜ書きを実行したら、残りは元の表層形を出力
                    tailMaze = true;
                }
            }
        }

        std::vector<CandidateString> result;
        if (cands.empty()) {
            // 交ぜ書き候補が無かった
            result.push_back(CandidateString(head + tail, strokeLen()));
            _LOG_DETAIL(L"maze result (no cands): {}", result.back().debugString());
        } else {
            // 交せ書き候補によるバリエーション
            for (const auto& c : cands) {
                //result.push_back(CandidateString(head + c + tail, strokeLen()));
                auto items = utils::split(c, '@');      // 変換形@属性
                auto word = head + items[0] + tail;
                if (items.size() > 1) {
                    result.push_back(CandidateString(word, strokeLen(), items[1]));
                } else {
                    result.push_back(CandidateString(word, strokeLen()));
                }
                _LOG_DETAIL(L"maze result: {}", result.back().debugString());
            }
        }
        _LOG_DETAIL(L"LEAVE");
        return result;
    }

    // 交ぜ書き優先度の取得
    int getMazegakiPreferenceCost(const MString& mazeFeat);

    void CandidateString::getDifficultMazeCands(const MString& surf, const MString& mazeCands, std::vector<MString>& result) const {
        auto cands = utils::split(mazeCands, '|');
        if (cands.size() <= 1) {
            result.push_back(mazeCands);
        } else {
            std::set<mchar_t> surfKanjis;
            for (mchar_t mc : surf) {
                if (utils::is_kanji(mc)) surfKanjis.insert(mc);
            }
            std::vector<MString> unused;
            for (const auto& cand : cands) {
                auto items = utils::split(cand, '@');
                bool found = false;
                for (mchar_t mc : items[0]) {
                    if (surfKanjis.find(mc) == surfKanjis.end() && !EASY_CHARS->IsEasyChar(mc)) {
                        // 難字を含む候補が見つかった
                        result.push_back(cand);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    unused.push_back(cand);
                }
            }
            //if (result.empty()) {
            //    std::copy(cands.begin(), cands.end(), std::back_inserter(result));
            //}
            for (const auto& cand : unused) {
                result.push_back(cand);
            }

            std::stable_sort(result.begin(), result.end(), [](const MString& a, const MString& b) {
                auto aItems = utils::split(a, '@');
                auto bItems = utils::split(b, '@');
                int aPriCost = aItems.size() > 1 ? getMazegakiPreferenceCost(aItems[1]) : 0;
                int bPriCost = bItems.size() > 1 ? getMazegakiPreferenceCost(bItems[1]) : 0;
                _LOG_DETAIL(L"aPriCost={}, bPriCost={}", aPriCost, bPriCost);
                return aPriCost < bPriCost;
                });
        }
    }

    CandidateString CandidateString::makeCandWithFeat(const std::vector<MString>& items, int strkLen) const {
        if (items.size() > 1) {
            return CandidateString(items[0], strkLen, items[1]);
        } else {
            return CandidateString(items[0], strkLen);
        }
    }

    // 単語素片を末尾に適用してみる
    std::vector<MString> CandidateString::applyPiece(const WordPiece& piece, int strokeCount, int paddingLen, bool isStrokeBS, bool bKatakanaConversion) const {
        LOG_DEBUGH(_T("ENTER: pieces={}, strokeCount={}, paddingLen={}, isStrokeBS={}, _strokeLen={}"), piece.debugString(), strokeCount, paddingLen, isStrokeBS, _strokeLen);
        std::vector<MString> ss;
        if (isStrokeBS || (piece.isPadding() && _strokeLen + paddingLen == strokeCount) || (_strokeLen + piece.strokeLen() == strokeCount)) {
            LOG_DEBUGH(_T("isStrokeBS({}) || _strokeLen({}) + piece.strokeLen({})|paddingLen({}) == strokeCount({})"),
                isStrokeBS, _strokeLen, piece.strokeLen(), paddingLen, strokeCount);
            // 素片のストローク数が適合した
            if (piece.rewriteNode()) {
                // 書き換えノード
                const RewriteInfo* rewInfo;
                int numBS = 0;
                std::tie(rewInfo, numBS) = matchWithTailString(piece.rewriteNode());

                if (rewInfo) {
                    // 末尾にマッチする書き換え情報があった
                    for (MString s : split_piece_str(rewInfo->rewriteStr)) {
                        ss.push_back(utils::safe_substr(_str, 0, -numBS) + s);
                    }
                }
                // 書き換えない候補も追加
                // 複数文字が設定されたストロークの扱い
                LOG_DEBUGH(_T("rewriteNode: {}"), to_wstr(piece.rewriteNode()->getString()));
                for (MString s : split_piece_str(piece.rewriteNode()->getString())) {
                    ss.push_back(_str + convertoToKatakanaIfAny(s, bKatakanaConversion));
                }
            } else {
                int numBS = piece.numBS();
                if (numBS > 0) {
                    if ((size_t)numBS < _str.size()) {
                        ss.push_back(utils::safe_substr(_str, 0, (int)(_str.size() - numBS)));
                    } else {
                        ss.push_back(EMPTY_MSTR);
                    }
                } else {
                    // 複数文字が設定されたストロークの扱い
                    LOG_DEBUGH(_T("normalNode: {}"), to_wstr(piece.getString()));
                    for (MString s : split_piece_str(piece.getString())) {
                        if (bKatakanaConversion) {
                            ss.push_back(_str + convertoToKatakanaIfAny(s, bKatakanaConversion));
                        } else {
                            ss.push_back(applyGlobalPostRewrite(s));
                        }
                    }
                }
            }
        } else {
            // ここで空文字列を push_back してはいけない。_candidates の stroke が進むことによって余計な候補が追加されてしまう。
            LOG_DEBUGH(_T("return NO result: _strokeLen({}) + piece.strokeLen({}) != strokeCount({})"), _strokeLen, piece.strokeLen(), strokeCount);
        }

        LOG_DEBUG(L"LEAVE: ss=\"{}\"", to_wstr(utils::join(ss, '|')));
        return ss;
    }

    String CandidateString::debugString() const {
        return to_wstr(_str)
            + _T(" (totalCost=") + std::to_wstring(totalCost())
            + _T("(_cost=") + std::to_wstring(_cost)
            + _T(",_penalty=") + std::to_wstring(_penalty)
            //+ _T(",_llama_loss=") + std::to_wstring(_llama_loss)
            + _T("), strokeLen=") + std::to_wstring(_strokeLen)
            + _T(", mazeFeat='") + to_wstr(_mazeFeat)
            + _T("')");
    }

} // namespace lattice2

