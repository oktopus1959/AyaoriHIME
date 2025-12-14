#include "Logger.h"
#include "file_utils.h"
#include "settings.h"

#include "MorphBridge.h"
#include "Llama/LlamaBridge.h"
#include "Lattice2_Common.h"
#include "Lattice2_Morpher.h"

namespace lattice2 {
    DECLARE_LOGGER;     // defined in Lattice2.cpp

#define MAZEGAKI_PREFERENCE_FILE    L"files/mazegaki.pref.txt"

    // 交ぜ書き優先度辞書
    std::map<MString, int> mazegakiPrefDict;

    // 交ぜ書き優先度がオンラインで更新されたか
    bool mazegakiPref_updated = false;

    // 交ぜ書き優先辞書の読み込み
    void loadMazegakiPrefFile() {
        auto path = utils::joinPath(SETTINGS->rootDir, MAZEGAKI_PREFERENCE_FILE);
        LOG_INFO(_T("LOAD: {}"), path.c_str());
        utils::IfstreamReader reader(path);
        if (reader.success()) {
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(utils::replace_all(utils::strip(utils::reReplace(line, L"#.*$", L"")), L"[ \t]+", L"\t"), '\t');
                if (!items.empty() && !items[0].empty() && items[0][0] != L'#') {
                    MString mazeFeat = to_mstr(items[0]);
                    if (items.size() >= 2 && isDecimalString(items[1])) {
                        mazegakiPrefDict[mazeFeat] = std::stoi(items[1]);
                    } else {
                        mazegakiPrefDict[mazeFeat] = -DEFAULT_WORD_BONUS;       // mazegaki優先度のデフォルトは -DEFAULT_WORD_BONUS
                    }
                }
            }
            LOG_INFO(_T("DONE: entries count={}"), mazegakiPrefDict.size());
        }
    }

    // 交ぜ書き優先度の取得
    int getMazegakiPreferenceCost(const MString& mazeFeat) {
        auto iter = mazegakiPrefDict.find(mazeFeat);
        if (iter == mazegakiPrefDict.end()) {
            return 0;
        } else {
            return iter->second;
        }
    }

    // 交ぜ書き優先辞書の書き込み
    void saveMazegakiPrefFile() {
        LOG_SAVE_DICT(L"ENTER: file={}, mazegakiPref_updated={}", MAZEGAKI_PREFERENCE_FILE, mazegakiPref_updated);
        auto path = utils::joinPath(SETTINGS->rootDir, MAZEGAKI_PREFERENCE_FILE);
        if (mazegakiPref_updated) {
            // 一旦、一時ファイルに書き込み
            auto pathTmp = path + L".tmp";
            {
                LOG_SAVE_DICT(_T("SAVE: mazegaki preferencele pathTmp={}"), pathTmp.c_str());
                utils::OfstreamWriter writer(pathTmp);
                if (writer.success()) {
                    for (const auto& pair : mazegakiPrefDict) {
                        String line;
                        int count = pair.second;
                        if (count < 0 || count > 1 || (count == 1 && Reporting::Logger::IsWarnEnabled())) {
                            // count が 0 または 1 の N-gramは無視する
                            line.append(to_wstr(pair.first));           // mazeFeat
                            line.append(_T("\t"));
                            line.append(std::to_wstring(pair.second));  // ボーナス
                            writer.writeLine(utils::utf8_encode(line));
                        }
                    }
                    mazegakiPref_updated = false;
                }
                LOG_SAVE_DICT(_T("DONE: entries count={}"), mazegakiPrefDict.size());
            }
            // pathTmp ファイルのサイズが path ファイルのサイズよりも小さい場合は、書き込みに失敗した可能性があるので、既存ファイルを残す
            utils::compareAndMoveFileToBackDirWithRotation(pathTmp, path, SETTINGS->backFileRotationGeneration);
        }
        LOG_SAVE_DICT(L"LEAVE: file={}", MAZEGAKI_PREFERENCE_FILE);
    }

    // 交ぜ書き優先度の更新
    void updateMazegakiPreference(const CandidateString& raised, const CandidateString& depressed) {
        _LOG_DETAIL(L"ENTER: raised={}, depressed={}", raised.debugString(), depressed.debugString());
        if (!raised.mazeFeat().empty()) {
            mazegakiPrefDict[raised.mazeFeat()] += -DEFAULT_WORD_BONUS;
            _LOG_DETAIL(L"raised: mazegakiPrefDict[{}]={}", to_wstr(raised.mazeFeat()), mazegakiPrefDict[raised.mazeFeat()]);
            mazegakiPref_updated = true;
        }
        if (!depressed.mazeFeat().empty()) {
            mazegakiPrefDict[depressed.mazeFeat()] += DEFAULT_WORD_BONUS;
            _LOG_DETAIL(L"depressed: mazegakiPrefDict[{}]={}", to_wstr(depressed.mazeFeat()), mazegakiPrefDict[depressed.mazeFeat()]);
            mazegakiPref_updated = true;
        }
        _LOG_DETAIL(L"LEAVE");
    }

    //--------------------------------------------------------------------------------------
    // 形態素解析関連
    const MString MS_MAZE = to_mstr(L"MAZE");

    const MString NonTerminalMarker = to_mstr(L"\t非終端,*");

    bool isNonTerminalMorph(const MString& morph) {
        return morph.find(NonTerminalMarker) != MString::npos;
    }

    // 1文字のひらがな形態素で、前後もひらがなの場合のコスト
    int MORPH_ISOLATED_HIRAGANA_COST = 3000;

    // 連続する単漢字の場合のコスト
    int MORPH_CONTINUOUS_ISOLATED_KANJI_COST = 3000;

    // 2文字以上の形態素で漢字を含む場合のボーナス
    //int MORPH_ANY_KANJI_BONUS = 5000;
    //int MORPH_ANY_KANJI_BONUS = 3000;
    int MORPH_ANY_KANJI_BONUS = 1000;

    // 3文字以上の形態素ですべてひらがなの場合のボーナス
    int MORPH_ALL_HIRAGANA_BONUS = 1000;

    // 2文字以上の形態素ですべてカタカナの場合のボーナス
    int MORPH_ALL_KATAKANA_BONUS = 3000;

    // 形態素解析コストの計算
    int calcMorphCost(const MString& s, std::vector<MString>& morphs) {
        int cost = 0;
        if (!s.empty()) {
            // 形態素解析器の呼び出し
            // mazePenalty = 0 なので、形態素解析器の中で、デフォルトの mazePenalty が加算される。
            cost = MorphBridge::morphCalcCost(s, morphs, 0, 0, true);

            _LOG_DETAIL(L"ENTER: {}: orig morph cost={}, morph={}", to_wstr(s), cost, to_wstr(utils::join(morphs, '/')));
            std::vector<std::vector<MString>> wordItemsList;
            for (auto iter = morphs.begin(); iter != morphs.end(); ++iter) {
                wordItemsList.push_back(utils::split(*iter, '\t'));
                //// MAZEコストの追加
                //if (utils::endsWith(wordItemsList.back().back(), MS_MAZE)) {
                //    size_t len = wordItemsList.back().front().size();
                //    int penalty = len <= 2 ? MAZE_PENALTY_2 : len == 3 ? MAZE_PENALTY_3 : MAZE_PENALTY_4;
                //    cost += penalty;
                //    _LOG_DETAIL(L"add MAZE penalty={}, new cost={}", penalty, cost);
                //}
            }
            for (auto iter = wordItemsList.begin(); iter != wordItemsList.end(); ++iter) {
                const auto& items = *iter;
                const MString& w = items[0];
                const MString& feat = items[2];

                //if (w.size() == 1 && utils::is_hiragana(w[0])) {
                //    // 1文字ひらがなが2つ続いて、その前後もひらがなのケース
                //    // 「開発させる」⇒「さ きれ て さ せる」
                //    if (iter != wordItemsList.begin() && iter + 1 != wordItemsList.end() && iter + 2 != wordItemsList.end()) {
                //        auto iter0 = iter - 1;
                //        auto iter1 = iter + 1;
                //        auto iter2 = iter + 2;
                //        const MString& w1 = iter1->front();
                //        if (w1.size() == 1 && utils::is_hiragana(w1[0])) {
                //            // 1文字ひらがなが2つ続いた
                //            if (utils::is_hiragana(iter0->front().back()) && utils::is_hiragana(iter2->front().front())) {
                //                // その前後もひらがながだった
                //                cost += MORPH_ISOLATED_HIRAGANA_COST;
                //                _LOG_DETAIL(L"{} {}: ADD ISOLATED_HIRAGANA_COST({}): morphCost={}", to_wstr(w), to_wstr(w1), MORPH_ISOLATED_HIRAGANA_COST, cost);
                //            }
                //        }
                //    }
                //}

                if (SETTINGS->depressedContinuousKanjiNum > 0) {
                    if (w.size() == 1 && utils::is_pure_kanji(w[0])) {
                        if (SETTINGS->depressedContinuousKanjiNum == 2) {
                            // 単漢字が2つ続くケース (「耳調量序高い」（これってけっこう高い）)
                            if (iter + 1 != wordItemsList.end()) {
                                auto iter1 = iter + 1;
                                const MString& w1 = iter1->front();
                                if (w1.size() == 1 && utils::is_pure_kanji(w1[0])) {
                                    cost += MORPH_CONTINUOUS_ISOLATED_KANJI_COST;
                                    _LOG_DETAIL(L"{} {}: ADD MORPH_CONTINUOUS_ISOLATED_KANJI_COST({}): morphCost={}",
                                        to_wstr(w), to_wstr(w1), MORPH_CONTINUOUS_ISOLATED_KANJI_COST, cost);
                                }
                            }
                        } else if (SETTINGS->depressedContinuousKanjiNum > 2) {
                            // 単漢字が3つ続くケース (「耳調量序高い」（これってけっこう高い）)
                            if (iter + 1 != wordItemsList.end() && iter + 2 != wordItemsList.end()) {
                                auto iter1 = iter + 1;
                                auto iter2 = iter + 2;
                                const MString& w1 = iter1->front();
                                const MString& w2 = iter2->front();
                                if (w1.size() == 1 && utils::is_pure_kanji(w1[0]) && w2.size() == 1 && utils::is_pure_kanji(w2[0])) {
                                    cost += MORPH_CONTINUOUS_ISOLATED_KANJI_COST;
                                    _LOG_DETAIL(L"{} {} {}: ADD MORPH_CONTINUOUS_ISOLATED_KANJI_COST({}): morphCost={}",
                                        to_wstr(w), to_wstr(w1), to_wstr(w2), MORPH_CONTINUOUS_ISOLATED_KANJI_COST, cost);
                                }
                            }
                        }
                    }
                }
                //if (w.size() >= 2 && std::any_of(w.begin(), w.end(), [](mchar_t c) { return utils::is_kanji(c); })) {
                //    cost -= MORPH_ANY_KANJI_BONUS * (int)(w.size() - 1);
                //}
                //if (w.size() >= 2) {
                //    int kCnt = (int)std::count_if(w.begin(), w.end(), [](mchar_t c) { return utils::is_kanji(c); });
                //    if (kCnt > 0) {
                //        cost -= MORPH_ANY_KANJI_BONUS * kCnt;
                //        _LOG_DETAIL(L"{}: SUB ANY_KANJI_BONUS({}): morphCost={}", to_wstr(w), MORPH_ANY_KANJI_BONUS * kCnt, cost);
                //    }
                //}
                if (w.size() >= 3 && !utils::endsWith(feat, MS_MAZE) && std::all_of(w.begin(), w.end(), [](mchar_t c) { return utils::is_hiragana(c); })) {
                    cost -= MORPH_ALL_HIRAGANA_BONUS;
                    _LOG_DETAIL(L"{}: SUB ALL_HIRAGANA_BONUS({}): morphCost={}", to_wstr(w), MORPH_ALL_HIRAGANA_BONUS, cost);
                }
                if (w.size() >= 2 && std::all_of(w.begin(), w.end(), [](mchar_t c) { return utils::is_katakana(c); })) {
                    auto iter1 = iter + 1;
                    bool flag = iter1 == wordItemsList.end();
                    if (!flag) {
                        const MString& w2 = iter1->front();
                        // 次がカタカナ連でないか、合計で6文以上
                        flag = !std::all_of(w2.begin(), w2.end(), [](mchar_t c) { return utils::is_katakana(c); }) || w.size() + w2.size() >= 6;
                    }
                    if (flag) {
                        // 次がカタカナ連でないか、合計で6文以上なら
                        cost -= MORPH_ALL_KATAKANA_BONUS;
                        _LOG_DETAIL(L"{}: SUB ALL_KATAKANA_BONUS({}): morphCost={}", to_wstr(w), MORPH_ALL_KATAKANA_BONUS, cost);
                    }
                }
            }
            _LOG_DETAIL(L"LEAVE: {}: total morphCost={}", to_wstr(s), cost);
        }
        return cost;
    }

    //// 表層形と交ぜ書き原形などの形態素情報
    //class MorphInfo {
    //    MString surface;
    //    MString mazeBase;

    //};

#if 0
    int totalCostWithMorph(const MString& candStr) {
        std::vector<MString> morphs;
        int morphCost = calcMorphCost(candStr, morphs);
        return morphCost;
    }
#endif


} // namespace lattice2

