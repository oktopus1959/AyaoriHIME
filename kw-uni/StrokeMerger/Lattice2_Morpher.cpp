#include "Logger.h"
#include "file_utils.h"
#include "settings.h"

#include "Dymazin/MorphBridge.h"
#include "Llama/LlamaBridge.h"
#include "Lattice2_Common.h"
#include "Lattice2_Morpher.h"

namespace {
    DEFINE_LOGGER(Lattice2_Morpher);
}

namespace lattice2 {
#define MAZEGAKI_PREFERENCE_FILE    JOIN_USER_FILES_FOLDER(L"mazegaki.pref.txt")

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
    int getMazegakiPreferenceBonus(const MString& mazeFeat) {
        auto iter = mazegakiPrefDict.find(mazeFeat);
        if (iter == mazegakiPrefDict.end()) {
            return 0;
        } else {
            return iter->second * SETTINGS->mazegakiBonusPointFactor;
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
    void updateMazegakiPreference(const CandidateString& raised, const CandidateString& lowered) {
        _LOG_DETAIL(L"ENTER: raised={}, lowered={}", raised.debugString(), lowered.debugString());
        if (!raised.mazeFeat().empty()) {
            auto raisedFeats = utils::split(raised.mazeFeat(), MAZE_TRANSLATION_FEATURE_DELIM);
            auto loweredFeats = utils::split(lowered.mazeFeat(), MAZE_TRANSLATION_FEATURE_DELIM);
            if (raisedFeats.size() != loweredFeats.size()) {
                _LOG_DETAIL(L"LEAVE: raisedFeats.size({}) != loweredFeats.size({})", raisedFeats.size(), loweredFeats.size());
                return;
            }
            for (size_t i = 0; i < raisedFeats.size(); ++i) {
                if (raisedFeats[i] != loweredFeats[i]) {
                    mazegakiPrefDict[raisedFeats[i]] -= 2;
                    _LOG_DETAIL(L"raised: mazegakiPrefDict[{}]={}", to_wstr(raisedFeats[i]), mazegakiPrefDict[raisedFeats[i]]);
                    if (mazegakiPrefDict[loweredFeats[i]] < 0) ++mazegakiPrefDict[loweredFeats[i]];
                    _LOG_DETAIL(L"lowered: mazegakiPrefDict[{}]={}", to_wstr(loweredFeats[i]), mazegakiPrefDict[loweredFeats[i]]);
                    mazegakiPref_updated = true;
                }
            }
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
            _LOG_DETAIL(L"ENTER: {}", to_wstr(s));
            // 形態素解析器の呼び出し
            // mazePenalty = 0 なので、形態素解析器の中で、デフォルトの mazePenalty が加算される。
            // 戻りの morph の構造は、"表層形 <TAB> 変換形[| 別候補]...(or -) <TAB> 品詞:細品詞,交ぜ基本形,変換基本形,変換表層形[,MAZE]" となっている。
            cost = MorphBridge::morphCalcCost(s, morphs, SETTINGS->morphMazeEntryPenalty, 0, true);

            _LOG_DETAIL(L"LEAVE: {}: morphCost={}, morph={}", to_wstr(s), cost, to_wstr(utils::join(morphs, to_mstr(L" || "))));
        }
        return cost;
    }

} // namespace lattice2

