#include "Logger.h"
#include "file_utils.h"
//
#include "settings.h"
#include "StateCommonInfo.h"
#include "BushuComp/BushuComp.h"
#include "BushuComp/BushuDic.h"
#include "KeysAndChars/EasyChars.h"
//
#include "Dymazin/MorphBridge.h"

#include "Lattice.h"
#include "Lattice2_Common.h"
#include "Lattice2_CandidateString.h"
#include "Lattice2_Ngram.h"
#include "Lattice2_Morpher.h"

namespace lattice2 {
    DECLARE_LOGGER;     // defined in Lattice2.cpp

#define GLOBAL_POST_REWRITE_FILE    JOIN_USER_FILES_FOLDER(L"global-post-rewrite-map.txt")

    // グローバルな後置書き換えマップ
    std::map<MString, MString> globalPostRewriteMap;

    // グローバルな後置書き換えマップファイルの読み込み
    void readGlobalPostRewriteMapFile() {
        globalPostRewriteMap.clear();
        auto path = utils::joinPath(SETTINGS->rootDir, GLOBAL_POST_REWRITE_FILE);
        LOG_INFO(_T("LOAD: {}"), path.c_str());
        utils::IfstreamReader reader(path);
        int count = 0;
        if (reader.success()) {
            LOG_INFO(_T("FOUND: {}"), path.c_str());
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(utils::replace_all(utils::strip(line), L"[>: ]+", L"\t"), '\t');
                if (items.size() == 2 &&
                    items[0].size() >= 1 && items[1].size() >= 1 &&
                    items[0][0] != L'#' && items[0][0] != L';') {

                   globalPostRewriteMap[to_mstr(items[0])] = to_mstr(items[1]);
                   ++count;
                }
            }
        }
        LOG_INFO(_T("LEAVE: count={}"), count);
    }

    DEFINE_CLASS_LOGGER(CandidateString);

    // 候補文字列
    // 末尾文字列にマッチする RewriteInfo を取得する
    std::tuple<const RewriteInfo*, int> CandidateString::matchWithTailString(const PostRewriteOneShotNode* rewriteNode) const {
        size_t maxlen = SETTINGS->kanaTrainingMode && ROOT_STROKE_NODE->hasOnlyUsualRewriteNdoe() ? 0 : 8;     // かな入力練習モードで濁点のみなら書き換えをやらない
        bool bRollOverStroke = STATE_COMMON->IsRollOverStroke();
        _LOG_DETAIL(_T("bRollOverStroke={}"), bRollOverStroke);
        while (maxlen > 0) {
            LOG_DEBUGH(_T("maxlen={}"), maxlen);
            //const MString targetStr = utils::safe_tailstr(_str, maxlen);
            const MString targetStr = SETTINGS->googleCompatible ? OUTPUT_STACK->backStringWhileOnlyRewritable(maxlen) : OUTPUT_STACK->backStringUptoRewritableBlock(maxlen);
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

    struct MorphCand {
        MString str;
        MString feat;
        int bonusOrPenalty = 0;

        String toString() const {
            return to_wstr(str) + L"@" + to_wstr(feat) + L"(" + std::to_wstring(bonusOrPenalty) + L")";
        }
    };

    MString join_all_flatten(
        const std::vector<std::vector<MString>>& vectors,
        const MString& inner_sep,
        const MString& outer_sep) {
        std::vector<MString> tmp;
        tmp.reserve(vectors.size());

        for (const auto& words : vectors) {
            tmp.push_back(utils::join(words, inner_sep));
        }

        return utils::join(tmp, outer_sep);
    }

    MString join_words(const std::vector<MorphCand>& vectors) {
        std::vector<MString> tmp;
        tmp.reserve(vectors.size());
        for (const auto& cand : vectors) {
            tmp.push_back(cand.str);
        }
        return utils::join(tmp, EMPTY_MSTR);
    }

    MString join_feats(const std::vector<MorphCand>& vectors) {
        std::vector<MString> tmp;
        tmp.reserve(vectors.size());
        for (const auto& cand : vectors) {
            tmp.push_back(cand.feat);
        }
        return utils::join(tmp, MAZE_TRANSLATION_FEATURE_DELIM);
    }

    // 累積コスト計算
    int accum_cost(const std::vector<MorphCand>& vectors) {
        int cost = 0;
        for (const auto& cand : vectors) {
            cost += cand.bonusOrPenalty;
        }
        return cost;
    }

    String join_all_cands_for_debug(const std::vector<MorphCand>& vectors) {
        std::vector<String> tmp;
        tmp.reserve(vectors.size());
        for (const auto& cand : vectors) {
            tmp.push_back(cand.toString());
        }
        return utils::join(tmp, L"||");
    }

    String join_all_cands_vector_for_debug(const std::vector<std::vector<MorphCand>>& vectors) {
        std::vector<String> tmp;
        tmp.reserve(vectors.size());
        for (const auto& cands : vectors) {
            tmp.push_back(join_all_cands_for_debug(cands));
        }
        return utils::join(tmp, L" ||| ");
    }

    String join_all_candidates(const std::vector<CandidateString>& vectors, const String& outer_sep) {
        std::vector<String> tmp;
        tmp.reserve(vectors.size());
        for (const auto& words : vectors) {
            tmp.push_back(words.debugString());
        }
        return utils::join(tmp, outer_sep);
    }

    // 隣接する形態素候補の組み合わせを全列挙してコスト計算
    void enumerate_contiguous_morphs_combination_and_calc_cost(
        size_t index,
        const std::vector<std::vector<MorphCand>>& morphCands,
        size_t maxCandidateNum,
        std::vector<MorphCand>& vecWork,
        std::vector<MorphCand>& results) {
        if (index == morphCands.size()) {
            // 終端に到達したので、コスト計算
            MString str = join_words(vecWork);
            MString feat = join_feats(vecWork);
            int srcAccumCost = accum_cost(vecWork);
            std::vector<MString> morphs;
            // 形態素解析コストの計算
            int morphCost = MorphBridge::morphCalcCost(str, morphs, 0, 0, false);
            //morphs.clear();
            // Ngramコストの計算
            int ngramCost = getNgramCost(str, false);
            int cost = srcAccumCost + morphCost + ngramCost;
            LOG_INFO(L"TAIL morph: str={}, feat={}, cost={} (srcAccumCost={}, morph={}, ngram={})",
                to_wstr(str), to_wstr(feat), cost, srcAccumCost, morphCost, ngramCost);
            results.push_back(MorphCand{std::move(str), std::move(feat), srcAccumCost + cost});
            return;
        }

        size_t count = 0;
        for (const auto& c : morphCands[index]) {
            auto old_size = vecWork.size();
            vecWork.push_back(c);
            enumerate_contiguous_morphs_combination_and_calc_cost(index + 1, morphCands, maxCandidateNum, vecWork, results);
            vecWork.resize(old_size);
            ++count;
            if (count >= maxCandidateNum) break;  // 各ベクトルから指定の最大数まで選択
        }
    }

    std::vector<CandidateString> generate_candidateString_from_items_sorted_by_cost(std::vector<MorphCand>& items, int strokeLen, size_t k) {
        std::stable_sort(items.begin(), items.end(),
            [](const MorphCand& a, const MorphCand& b) {
                return a.bonusOrPenalty < b.bonusOrPenalty;
            });

        std::vector<CandidateString> results;
        results.reserve(k);
        size_t j = 0;
        for (const auto& it : items) {
            results.push_back(CandidateString(it.str, strokeLen, it.feat));     // ムーブで取り出し
            ++j;
            if (j >= k) break;
        }
        return results;
    }

    // 交ぜ書き候補の追加と優先度コストの取得
    void add_maze_cand(std::vector<MorphCand>& result, const MString& morph, int bonusOrPenalty) {
        auto items = utils::split(morph, '@');
        if (items.size() > 1) {
            int mazePreferenceBonus = getMazegakiPreferenceBonus(items[1]);
            _LOG_DETAIL(L"cand xlat={}, feat={}, mazePref={}, bonusOrPenalty={}", to_wstr(items[0]), to_wstr(items[1]), mazePreferenceBonus, bonusOrPenalty);
            result.push_back(MorphCand{ items[0], items[1], mazePreferenceBonus + bonusOrPenalty });
        } else {
            result.push_back(MorphCand{ items[0], EMPTY_MSTR, 0 });
        }
    }

    struct MazeCandInfo {
        MString str;
        int bonusOrPenalty;
    };

    // 形態素単位で、交ぜ書き候補を取得する
    void getMazeCands(const MString& surf, const MString& mazeCands, std::vector<MorphCand>& result) {
        _LOG_DETAIL(L"ENTER: surf={}, mazeCands={}, result={}", to_wstr(surf), to_wstr(mazeCands), join_all_cands_for_debug(result));
        auto cands = utils::split(mazeCands, '|');
        if (cands.size() <= 1) {
            _LOG_DETAIL(L"put single mazeCands={}", to_wstr(mazeCands));
            add_maze_cand(result, mazeCands, 0);
        } else {
            std::set<mchar_t> surfKanjis;
            for (mchar_t mc : surf) {
                if (utils::is_kanji(mc)) surfKanjis.insert(mc);
            }
            std::vector<MazeCandInfo> rests;
            MString firstHinshi;        // 先頭品詞
            for (const auto& cand : cands) {
                auto items = utils::split(cand, '@');
                const MString& xlat = items[0];
                const MString& feat = items.size() > 1 ? items[1] : EMPTY_MSTR;
                MString hinshi;
                auto featItems = utils::split(feat, '/');
                if (featItems.size() > 1) {
                    hinshi = utils::substr_upto(featItems[1], ':');
                } else {
                    hinshi = MSTR_HASH_MARK;    // "#": 品詞不明
                }
                if (firstHinshi.empty()) {
                    firstHinshi = hinshi;
                }
                bool found = false;
                bool sameHinshi = hinshi == firstHinshi;
                if (sameHinshi) {
                    // 同じ品詞なら難字優先
                    for (mchar_t mc : xlat) {
                        if (surfKanjis.find(mc) != surfKanjis.end() && !EASY_CHARS->IsEasyChar(mc)) {
                            // 難字を含む候補が見つかったので、こちらを優先して出力
                            _LOG_DETAIL(L"put difficult cand={}", to_wstr(cand));
                            add_maze_cand(result, cand, 0);     // TODO: 難字ボーナスを付与する？
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    int bonusOrPenalty = sameHinshi ? 0 : DIFFERENT_HINSHI_PENALTY ;
                    rests.push_back(MazeCandInfo{ cand, bonusOrPenalty });
                }
            }
            // 難字を含まない残りの候補を追加
            //_LOG_DETAIL(L"rests={}", to_wstr(utils::join(rests, to_mstr(L" ||| "))));
            for (const auto& info : rests) {
                add_maze_cand(result, info.str, info.bonusOrPenalty);
            }

            // feature部による交ぜ書き優先度でソート
            std::stable_sort(result.begin(), result.end(), [](const MorphCand& a, const MorphCand& b) {
                return a.bonusOrPenalty < b.bonusOrPenalty;
            });
        }
        _LOG_DETAIL(L"LEAVE: result={}", join_all_cands_for_debug(result));
    }

    size_t calcMazeMorphsUpperLimit(size_t maxMazeVariations, size_t index, const std::vector<std::vector<MorphCand>>& morphCands) {
        size_t upperLimit = maxMazeVariations;
        while (upperLimit > 1) {
            size_t nVariations = 1;
            size_t maxMorphs = 1;
            for (size_t i = index; i < morphCands.size(); ++i) {
                size_t nMorphs = morphCands[i].size();
                if (nMorphs > upperLimit) nMorphs = upperLimit;
                if (nMorphs > maxMorphs) maxMorphs = nMorphs;
                nVariations *= nMorphs;
            }
            if (nVariations <= maxMazeVariations) break;
            if (maxMorphs > 10) {
                upperLimit = 10;
            } else if (maxMorphs > 7) {
                upperLimit = 7;
            } else if (maxMorphs > 5) {
                upperLimit = 5;
            } else if (maxMorphs > 2) {
                --upperLimit;
            }
        }
        return upperLimit;
    }

    // 交ぜ書き変換の実行
    std::vector<CandidateString> CandidateString::applyMazegaki() const {
        _LOG_DETAIL(L"ENTER: {}", to_wstr(_str));
        EASY_CHARS->DumpEasyCharsMemory();

        // リアルタイムNgramの更新
        updateRealtimeNgram(_str);

        std::vector<MString> words;
        // まず、交ぜ書き優先で形態素解析する
        int mazePenalty = -1;           // -SETTINGS->morphMazeEntryPenalty
        int mazeConnPenalty = 3000;     // -SETTINGS->morphMazeConnectionPenalty
        _LOG_DETAIL(L"MorphBridge::morphCalcCost(_str={}, words, mazePenalty={}, mazeConnPenalty={}, allowNonTerminal=False)", to_wstr(_str), mazePenalty, mazeConnPenalty);
        int cost = MorphBridge::morphCalcCost(_str, words, mazePenalty, mazeConnPenalty, false);
        _LOG_DETAIL(L"{}: orig morph cost={}, morph={}", to_wstr(_str), cost, to_wstr(utils::join(words, to_mstr(L" ||| "))));

        std::vector<std::vector<MorphCand>> vecMorphCands;
        // 形態素単位で、交ぜ書き候補を取得する
        for (auto iter = words.begin(); iter != words.end(); ++iter) {
            _LOG_DETAIL(L"morph: {}", to_wstr(*iter));
            auto items = utils::split(*iter, '\t');
            if (items.size() > 1) {
                const MString& surf = items[0];
                vecMorphCands.emplace_back();
                const MString& mazeCands = items[1];
                getMazeCands(surf, mazeCands, vecMorphCands.back());
            }
        }
        _LOG_DETAIL(L"vecMorphCands={}", join_all_cands_vector_for_debug(vecMorphCands));

        // 交せ書き候補によるバリエーション
#define MAX_MAZEGAKI_VARIATIONS 300
#define MAX_MAZEGAKI_MORPHS 5
//#define MAX_MORPH_CAND_NUM 3
//#define MAX_ITEM_NUM 300    /* >= pow(MAX_MORPH_CAND_NUM, MAX_MAZEGAKI_MORPHS) */
#define MAX_CANDITATE_STR_NUM 20
        std::vector<MorphCand> morphs;
        size_t index = 0;
        size_t numMorph = vecMorphCands.size();
        // 形態素数が多すぎる場合は、前のほうをまとめる
        while (numMorph > MAX_MAZEGAKI_MORPHS) {
            morphs.push_back(vecMorphCands[index][0]);
            ++index;
            --numMorph;
        }
        // 先頭に空白を含む形態素があれば、その前までもまとめる
        size_t endIndex = index + numMorph;
        for (size_t i = index; i < endIndex; ++i) {
            for (const auto& cand : vecMorphCands[i]) {
                if (!cand.str.empty() && cand.str.front() == ' ') {
                    // 先頭に空白を含む形態素が見つかった
                    _LOG_DETAIL(L"found space in morph cand: {}", to_wstr(cand.str));
                    // ここまでをまとめる
                    while (index < i) {
                        morphs.push_back(vecMorphCands[index][0]);
                        ++index;
                        --numMorph;
                    }
                    break;
                }
            }
        }
        std::vector<MorphCand> sortedItems;
        sortedItems.reserve(MAX_MAZEGAKI_VARIATIONS);
        // 隣接する形態素候補の組み合わせを全列挙してコスト計算
        size_t maxCandidateNum = calcMazeMorphsUpperLimit(MAX_MAZEGAKI_VARIATIONS, index, vecMorphCands);
        enumerate_contiguous_morphs_combination_and_calc_cost(index, vecMorphCands, maxCandidateNum, morphs, sortedItems);
        _LOG_DETAIL(L"LEAVE: num of sortedItems={}", sortedItems.size());
        // コストでソートして候補文字列を取得
        return generate_candidateString_from_items_sorted_by_cost(sortedItems, strokeLen(), MAX_CANDITATE_STR_NUM);
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

                bool bRewriteFound = false;
                if (rewInfo) {
                    // 末尾にマッチする書き換え情報があった
                    for (MString s : split_piece_str(rewInfo->rewriteStr)) {
                        ss.push_back(utils::safe_substr(_str, 0, -numBS) + s);
                        if (!SETTINGS->multiCandidateMode) {
                            // 単一候補モードの場合は最初の候補だけを追加
                            bRewriteFound = true;
                            break;
                        }
                    }
                }
                if (!bRewriteFound) {
                    // 書き換えない候補も追加
                    // 複数文字が設定されたストロークの扱い
                    LOG_DEBUGH(_T("rewriteNode: {}"), to_wstr(piece.rewriteNode()->getString()));
                    for (MString s : split_piece_str(piece.rewriteNode()->getString())) {
                        ss.push_back(_str + convertoToKatakanaIfAny(s, bKatakanaConversion));
                    }
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
                        if (!SETTINGS->multiCandidateMode) {
                            // 単一候補モードの場合は最初の候補だけを追加
                            break;
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
            + _T("(_morph=") + std::to_wstring(_morphCost)
            + _T(",_ngram=") + std::to_wstring(_ngramCost)
            + _T(",_penalty=") + std::to_wstring(_penalty)
            //+ _T(",_llama_loss=") + std::to_wstring(_llama_loss)
            + _T("), strokeLen=") + std::to_wstring(_strokeLen)
            + _T(", mazeFeat='") + to_wstr(_mazeFeat)
            + _T("')");
    }

} // namespace lattice2

