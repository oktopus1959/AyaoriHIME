#include "MazePrepro_Common.h"
#include "reporting/ErrorHandler.h"

#if 0
#undef LOG_DEBUGH
#undef LOG_DEBUG
#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFO
#endif

namespace MazegakiPreprocessor {
    DECLARE_LOGGER;

    MapString _kanjiYomi;
    MapString _kanjiBigram;

    inline String _findKanjiYomi(StringRef kanji) {
        //auto it = _kanjiYomi.find(kanji);
        //if (it != _kanjiYomi.end()) {
        //    return it->second;
        //}
        //return L"#";
        return _safeGet(_kanjiYomi, kanji);
    }

    inline String _findKatakanaYomi(wchar_t kch) {
        String yomi(utils::katakana_to_hiragana(kch), 1);
        if (kch == L'カ' || kch == L'ケ' || kch == L'ヵ' || kch == L'ヶ') {
            yomi += L"|が";
        }
        return yomi;
    }

    inline String _findKanjiBigram(StringRef kanji) {
        //auto it = _kanjiBigram.find(kanji);
        //if (it != _kanjiBigram.end()) {
        //    return it->second;
        //}
        //return L"#";
        return _safeGet(_kanjiBigram, kanji);
    }

    inline bool _isNil(StringRef str) {
        //return str.size() == 1 && str[0] == L'#';
        return str.empty();
    }

    /**yomi を 1文字と2文字に分解。
    * 
    * 例えば、"かみ" の場合、["かみ", "か", "み"] のように分解する。
    * "みなみ" なら、["みなみ", "み", "みな", "なみ", "み"] のように分解する。
    * @param yomi 読み
    * @return 分解された読みのリスト (元の yomi を含む)
    */
    VectorString _splitYomi(StringRef yomi) {
        VectorString result = { yomi };
        if (yomi.size() == 2) {
            result.push_back(yomi.substr(0, 1));
            result.push_back(yomi.substr(1, 1));
        } else if (yomi.size() == 3) {
            result.push_back(yomi.substr(0, 1));
            result.push_back(yomi.substr(0, 2));
            result.push_back(yomi.substr(1, 2));
            result.push_back(yomi.substr(2, 1));
        }
        return result;
    }

    void dakutenize(std::set<String>& vec, StringRef& y) {
        if (_reSearch(y, L"^[かきくけこさしすせそたちつてと]")) {
            vec.insert(_tr(y, L"かきくけこさしすせそたちつてと", L"がぎぐげござじずぜぞだぢづでど", 1));
            vec.insert(_tr(y, L"つ", L"ず", 1));     // ex. つく -> ずく
        } else if (_reSearch(y, L"^[はひふへほ]")) {
            vec.insert(_tr(y, L"はひふへほ", L"ばびぶべぼ", 1));
            vec.insert(_tr(y, L"はひふへほ", L"ぱぴぷぺぽ", 1));
        }
    }

    /**
    * 漢字に対する読みのバリエーションを追加する
    * @param kanji 漢字
    * @param yomiDef 読みのリスト
    * @note 読みのリストは、同じ漢字に対する読みのバリエーションを含む。
    * 例えば、漢字「神」の場合、yomiDef は "かみ|こう|じん" のようになる。
    * この関数は、読みのリストから様々なバリエーションを生成し、_kanjiYomi マップに追加する。
    */
    void addYomiVariation(StringRef kanji, StringRef yomiDef) {
        if (yomiDef.empty()) return;
        
        size_t pos = yomiDef.find(L"||", 0);
        String yomi1 = (pos != String::npos) ? yomiDef.substr(0, pos) : yomiDef;
        String yomi2 = (pos != String::npos) ? utils::safe_substr(yomiDef, pos+2) : yomiDef;

        const auto list1 = utils::split(yomi1, '|');
        const auto list2 = utils::split(yomi2, '|');
        //LOG_DEBUGH(_T("kanji={}, yomiDef={}, yomi1={}, yomi2={}, list1={}, list2={}"), kanji, yomiDef, yomi1, yomi2, utils::join(list1, '|'), utils::join(list2, '|'));

        std::set<String> uniqueYomis;
        std::set<String> tmpSet;
        for (const auto& yomi : list1) {
            if (yomi.empty()) continue;
            uniqueYomis.insert(yomi);
            tmpSet.insert(yomi);
            dakutenize(tmpSet, yomi);
        }

        for (const auto& yomi : tmpSet) {
            if (yomi.empty()) continue;
            uniqueYomis.insert(yomi);
            uniqueYomis.insert(yomi + L"っ");
            if (yomi.size() > 3) {
                uniqueYomis.insert(yomi.substr(0, yomi.size() - 1));
                uniqueYomis.insert(yomi.substr(0, yomi.size() - 2));
                //uniqueYomis.insert(yomi.substr(0, yomi.size() - 3));
            } else if (yomi.size() == 3) {
                uniqueYomis.insert(yomi.substr(0, yomi.size() - 1));
                uniqueYomis.insert(yomi.substr(0, yomi.size() - 2));
            } else if (yomi.size() == 2) {
                uniqueYomis.insert(yomi.substr(0, yomi.size() - 1));
            } else {
                uniqueYomis.insert(yomi + L"う");
                uniqueYomis.insert(yomi + L"し");
            }
            if (yomi.size() > 1) {
                uniqueYomis.insert(replace_suffix(yomi, L"う", L"い"));
                uniqueYomis.insert(replace_suffix(yomi, L"き", L"っ"));
                uniqueYomis.insert(replace_suffix(yomi, L"く", L"き"));
                uniqueYomis.insert(replace_suffix(yomi, L"く", L"っ"));
                uniqueYomis.insert(replace_suffix(yomi, L"ぐ", L"ぎ"));
                uniqueYomis.insert(replace_suffix(yomi, L"す", L"し"));
                uniqueYomis.insert(replace_suffix(yomi, L"ち", L"っ"));
                uniqueYomis.insert(replace_suffix(yomi, L"つ", L"ち"));
                uniqueYomis.insert(replace_suffix(yomi, L"つ", L"っ"));
                uniqueYomis.insert(replace_suffix(yomi, L"に", L"な"));
                uniqueYomis.insert(replace_suffix(yomi, L"ぬ", L"に"));
                uniqueYomis.insert(replace_suffix(yomi, L"ね", L"な"));
                uniqueYomis.insert(replace_suffix(yomi, L"ぶ", L"び"));
                uniqueYomis.insert(replace_suffix(yomi, L"み", L"な"));
                uniqueYomis.insert(replace_suffix(yomi, L"み", L"ん"));
                uniqueYomis.insert(replace_suffix(yomi, L"む", L"み"));
                uniqueYomis.insert(replace_suffix(yomi, L"る", L"っ"));
                uniqueYomis.insert(replace_suffix(yomi, L"る", L"り"));
                uniqueYomis.insert(replace_suffix(yomi, L"きゃ", L"か"));
                uniqueYomis.insert(replace_suffix(yomi, L"ぎゃ", L"が"));
                uniqueYomis.insert(replace_suffix(yomi, L"しゃ", L"さ"));
                uniqueYomis.insert(replace_suffix(yomi, L"じゃ", L"ざ"));
                uniqueYomis.insert(replace_suffix(yomi, L"ちゃ", L"た"));
                uniqueYomis.insert(replace_suffix(yomi, L"ぢゃ", L"だ"));
                uniqueYomis.insert(replace_suffix(yomi, L"にゃ", L"な"));
                uniqueYomis.insert(replace_suffix(yomi, L"ひゃ", L"は"));
                uniqueYomis.insert(replace_suffix(yomi, L"びゃ", L"ば"));
                uniqueYomis.insert(replace_suffix(yomi, L"みゃ", L"ま"));
                uniqueYomis.insert(replace_suffix(yomi, L"りゃ", L"ら"));
                if (yomi.size() > 2) {
                    uniqueYomis.insert(replace_suffix(yomi, L"える", L"ゆ"));
                }
            }

            //uniqueYomis.insert(yomi + L"っ");
            //if (yomi.size() > 3) {
            //    uniqueYomis.insert(yomi.substr(0, yomi.size() - 1));
            //    uniqueYomis.insert(yomi.substr(0, yomi.size() - 2));
            //    //uniqueYomis.insert(yomi.substr(0, yomi.size() - 3));
            //} else if (yomi.size() == 3) {
            //    uniqueYomis.insert(yomi.substr(0, yomi.size() - 1));
            //    uniqueYomis.insert(yomi.substr(0, yomi.size() - 2));
            //} else if (yomi.size() == 2) {
            //    uniqueYomis.insert(yomi.substr(0, yomi.size() - 1));
            //} else {
            //    uniqueYomis.insert(yomi + L"う");
            //    uniqueYomis.insert(yomi + L"し");
            //}
        }

        // list2 は濁点化と促音化のみ
        for (const auto& yomi : list2) {
            if (!yomi.empty()) {
                uniqueYomis.insert(yomi);
                dakutenize(uniqueYomis, yomi);
                if (yomi.size() > 1) {
                    uniqueYomis.insert(replace_suffix(yomi, L"く", L"っ"));
                    uniqueYomis.insert(replace_suffix(yomi, L"ち", L"っ"));
                    uniqueYomis.insert(replace_suffix(yomi, L"つ", L"っ"));
                }
            }
        }

        // set の要素を vector にコピー
        VectorString vec(uniqueYomis.begin(), uniqueYomis.end());

        // 長さ順でソート（長さが同じなら辞書順）
        std::sort(vec.begin(), vec.end(), [](StringRef a, StringRef b) {
            return a.size() == b.size() ? a < b : a.size() > b.size();
        });
        _kanjiYomi[kanji] = utils::join(vec, L"|");
        //LOG_DEBUG(L"_kanjiYomi[{}] = {}", kanji, _kanjiYomi[kanji]);
    }

    void _loadJoyoKanjiFile(StringRef filePath) {
        LOG_DEBUGH(_T("ENTER: {}"), filePath);
        utils::IfstreamReader reader(filePath);
        if (reader.success()) {
            _kanjiYomi.clear();
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(_kataka2hiragana(utils::replace_all(utils::strip(line), L" +", L"\t")), '\t');
                if (items.size() == 2 && !items[0].empty() && items[0][0] != '#') {
                    addYomiVariation(items[0], items[1]);
                }
            }
            LOG_DEBUGH(_T("LEAVE"));
        } else {
            LOG_ERROR(_T("Failed to open file: {}"), filePath);
        }
    }

    void _loadKanjiBigramFile(StringRef filePath) {
        LOG_DEBUGH(L"ENTER: filePath={}", filePath);

        utils::IfstreamReader reader(filePath);
        if (reader.success()) {
            _kanjiBigram.clear();
            for (const auto& line : reader.getAllLines()) {
                auto items = utils::split(utils::replace_all(utils::strip(line), L" +", L"\t"), '\t');
                if (items.size() == 2) {
                    const auto& key = items[0];
                    auto yomi = items[1];
                    if (_reSearch(yomi, L"^[かきくけこさしすせそたちつてと]")) {
                        auto yomi2 = _tr(yomi, L"かきくけこさしすせそたちつてと", L"がぎぐげござじずぜぞだぢづでど", 1);
                        if (yomi2 != yomi) {
                            yomi = yomi + L"|" + yomi2;
                        }
                        yomi2 = _tr(yomi, L"つ", L"ず", 1);
                        if (yomi2 != yomi) {
                            yomi = yomi + L"|" + yomi2;
                        }
                    } else if (_reSearch(yomi, L"^[はひふへほ]")) {
                        auto yomi2 = _tr(yomi, L"はひふへほ", L"ばびぶべぼ", 1);
                        if (yomi2 != yomi) {
                            yomi = yomi + L"|" + yomi2;
                        }
                        yomi2 = _tr(yomi, L"はひふへほ", L"ぱぴぷぺぽ", 1);
                        if (yomi2 != yomi) {
                            yomi = yomi + L"|" + yomi2;
                        }
                    }
                    if (!key.empty() && key[0] != '#' && !yomi.empty()) {
                        auto bg = _findKanjiBigram(key);
                        if (!_isNil(bg)) {
                            _kanjiBigram[key] = bg + L"|" + yomi;
                        } else {
                            _kanjiBigram[key] = yomi;
                        }
                    }
                }
            }
            LOG_DEBUGH(_T("LEAVE"));
        } else {
            LOG_ERROR(_T("Failed to open file: {}"), filePath);
        }
    }

    String getPos(StringRef p1, StringRef p2, StringRef p3, StringRef p4, StringRef kt) {
        if (p1 == L"名詞") {
            if (p3 == L"地域") {
                return L"地名";
            } else if (p3 == L"人名") {
                if (p4 == L"姓" || p4 == L"名") {
                    return p4;
                } else {
                    return p3;
                }
            } else if (p3 == L"組織") {
                return p3;
            } else if (p2 == L"固有名詞") {
                return p2;
            } else if (p2 == L"サ変接続") {
                return L"名詞サ変";
            } else if (p2 == L"形容動詞語幹") {
                return L"名詞形動";
            } else if (p2 == L"接尾") {
                if (p3 == L"助数詞") {
                    return p3;
                } else if (p3 == L"人名") {
                    return p2 + p3;
                } else if (p3 == L"地域") {
                    return p2 + L"地名";
                } else {
                    return p2 + L"一般";
                }
            } else {
                return p1;
            }
        } else if (p1 == L"動詞") {
            const auto items = utils::reScan(kt, L"五段・(.行)");
            if (!items.empty()) {
                return L"動詞" + items[0] + L"五段";
            } else if (kt == L"一段") {
                return L"動詞一段";
            } else {
                const auto items2 = utils::reScan(kt, L"^([カサザラ]変)");
                if (!items2.empty()) {
                    return L"動詞" + items2[0];
                } else {
                    return L"";     // 不正な動詞品詞;
                }
            }
        } else if (p1 == L"形容詞" || p1 == L"副詞" || p1 == L"連体詞" || p1 == L"接続詞" || p1 == L"感動詞") {
            return p1;
        } else if (p1 == L"接頭詞") {
            return L"接頭語";
        } else {
            return L"";
        }
    }

    void push_items(VectorString& items, StringRef item1, StringRef item2) {
        items.push_back(item1);
        items.push_back(item2);
    }

    void push_items(VectorString& items, StringRef item1, StringRef item2, StringRef item3) {
        items.push_back(item1);
        items.push_back(item2);
        items.push_back(item3);
    }

    void push_items(VectorString& items, StringRef item1, StringRef item2, StringRef item3, StringRef item4) {
        items.push_back(item1);
        items.push_back(item2);
        items.push_back(item3);
        items.push_back(item4);
    }

    void push_items(VectorString& items, StringRef item1, StringRef item2, StringRef item3, StringRef item4, StringRef item5) {
        items.push_back(item1);
        items.push_back(item2);
        items.push_back(item3);
        items.push_back(item4);
        items.push_back(item5);
    }

    String HIRAGANA_PATT = L"[^ぁぃぅぇぉゃゅょんをー].{0,3}?";

    //inline String makePatt1(StringRef h) {
    //    return h + L"(" + HIRAGANA_PATT + L")";
    //}

    //inline String makePatt2(StringRef h, StringRef p) {
    //    return h + L"((?:" + p + L"))";
    //}

    //inline String makePatt3(StringRef h, StringRef p) {
    //    return h + L"((?:" + p + L").{0,1})";
    //}

#define SCAN_AND_RETURN_IF_MATCH(_n, _yomi, _patt) \
    { \
        LOG_DEBUG(L"{}: SCAN_AND_RETURN_IF_MATCH: yomi={}, patt={}", _n, _yomi, _patt); \
        const auto _items = _reMatchScan(_yomi, _patt); \
        if (!_items.empty()) { \
            LOG_DEBUG(L"Matched: {}-{}: {}", _n, _items.size(), utils::join(_items, ',')); \
            if (_items.size() >= 4) { \
                return { _items[0], _items[1], _items[2], _items[3] }; \
            } else if (_items.size() == 3) { \
                return { _items[0], _items[1], _items[2] }; \
            } else if (_items.size() == 2) { \
                return { _items[0], _items[1] }; \
            } else { \
                return { _items[0] }; \
            } \
        } \
    }

#define SCAN_AND_RETURN_IF_MATCH_3A(_n, _yomi, _patt) \
    { \
        LOG_DEBUG(L"{}: SCAN_AND_RETURN_IF_MATCH_3A: yomi={}, patt={}", _n, _yomi, _patt); \
        const auto _items = _reMatchScan(_yomi, _patt); \
        if (_items.size() >= 2) { \
            LOG_DEBUG(L"Matched: {}-{}: {}", _n, _items.size(), utils::join(_items, ',')); \
            if (_items.size() >= 3) { \
                return { _items[0], L"", _items[1], _items[2] }; \
            } else if (_items.size() == 2) { \
                return { _items[0], L"", _items[1] }; \
            } \
        } \
    }

#define SCAN_AND_RETURN_IF_MATCH_3B(_n, _yomi, _patt) \
    { \
        LOG_DEBUG(L"{}: SCAN_AND_RETURN_IF_MATCH_3B: yomi={}, patt={}", _n, _yomi, _patt); \
        const auto _items = _reMatchScan(_yomi, _patt); \
        if (_items.size() >= 2) { \
            LOG_DEBUG(L"Matched: {}-{}: {}", _n, _items.size(), utils::join(_items, ',')); \
            if (_items.size() >= 3) { \
                return { _items[0], _items[1], L"", _items[2] }; \
            } else if (_items.size() == 2) { \
                return { _items[0], _items[1], L"" }; \
            } \
        } \
    }

#define SCAN_AND_RETURN_IF_MATCH_3C(_n, _yomi, _patt) \
    { \
        LOG_DEBUG(L"{}: SCAN_AND_RETURN_IF_MATCH_3C: yomi={}, patt={}", _n, _yomi, _patt); \
        const auto _items = _reMatchScan(_yomi, _patt); \
        if (_items.size() >= 2) { \
            LOG_DEBUG(L"Matched: {}-{}: {}", _n, _items.size(), utils::join(_items, ',')); \
            if (_items.size() >= 3) { \
                return { _items[0], _items[1], _items[2], L"" }; \
            } else if (_items.size() == 2) { \
                return { _items[0], _items[1], L"", L"" }; \
            } \
        } \
    }

#define SCAN_AND_RETURN_IF_MATCH_2A(_n, _yomi, _patt) \
    { \
        LOG_DEBUG(L"{}: SCAN_AND_RETURN_IF_MATCH_2A: yomi={}, patt={}", _n, _yomi, _patt); \
        const auto _items = _reMatchScan(_yomi, _patt); \
        if (_items.size() == 2) { \
            LOG_DEBUG(L"Matched: {}-{}: {}", _n, _items.size(), utils::join(_items, ',')); \
            return { _items[0], L"", L"", _items[1] }; \
        } \
    }

#define SCAN_AND_RETURN_IF_MATCH_2B(_n, _yomi, _patt) \
    { \
        LOG_DEBUG(L"{}: SCAN_AND_RETURN_IF_MATCH_2B: yomi={}, patt={}", _n, _yomi, _patt); \
        const auto _items = _reMatchScan(_yomi, _patt); \
        if (_items.size() == 2) { \
            LOG_DEBUG(L"Matched: {}-{}: {}", _n, _items.size(), utils::join(_items, ',')); \
            return { _items[0], L"", _items[1], L"" }; \
        } \
    }

#define SCAN_AND_RETURN_IF_MATCH_2C(_n, _yomi, _patt) \
    { \
        LOG_DEBUG(L"{}: SCAN_AND_RETURN_IF_MATCH_2C: yomi={}, patt={}", _n, _yomi, _patt); \
        const auto _items = _reMatchScan(_yomi, _patt); \
        if (_items.size() >= 2) { \
            LOG_DEBUG(L"Matched: {}: {}", _n, utils::join(_items, ',')); \
            return { _items[0], _items[1], L"", L"" }; \
        } \
    }

    /** 漢字に対する読みの検索 (k1, k2 は empty でない)
    * @param yomi 読み
    * @param h1, h2, h3 出し語におけるひらがなの並び
    * @param k1, k2, k3, k4 見出し語における漢字の並び
    */
    VectorString findYomi(StringRef yomi, StringRef k1, StringRef h1, StringRef k2, StringRef h2, StringRef k3, StringRef h3, StringRef k4) {
        LOG_DEBUGH(L"ENTER: yomi={}, k1={}, h1={}, k2={}, h2={}, k3={}, h3={}, k4={}", yomi, k1, h1, k2, h2, k3, h3, k4);

        VectorString result;
        if (yomi.empty() || yomi == L"#") {
            return result; // 空の読みは無視
        }

        const auto ky1 = _findKanjiYomi(k1);
        const auto ky2 = !k2.empty() && utils::is_katakana(k2.front()) ? _findKatakanaYomi(k2.front()) : _findKanjiYomi(k2);
        const auto ky3 = !k3.empty() && utils::is_katakana(k3.front()) ? _findKatakanaYomi(k3.front()) : _findKanjiYomi(k3);
        const auto ky4 = _findKanjiYomi(k4);
        LOG_DEBUGH(L"kanji1={}, ky1={}", k1, ky1);
        LOG_DEBUGH(L"kanji2={}, ky2={}", k2, ky2);
        LOG_DEBUGH(L"kanji3={}, ky3={}", k3, ky3);
        LOG_DEBUGH(L"kanji4={}, ky4={}", k4, ky4);

        const auto _ky1 = ky1.empty() ? L"" : L"(?:" + ky1 + L")[のん]?";
        const auto _ky2 = ky2.empty() ? L"" : L"(?:" + ky2 + L")[のん]?";
        const auto _ky3 = ky3.empty() ? L"" : L"(?:" + ky3 + L")[のん]?";
        const auto _ky4 = ky4.empty() ? L"" : L"(?:" + ky4 + L")[のん]?";

        const auto kb12 = _findKanjiBigram(k1 + h1 + k2);
        const auto kb23 = _findKanjiBigram(k2 + h2 + k3);
        const auto kb34 = _findKanjiBigram(k3 + h3 + k4);
        LOG_DEBUGH(L"kanji12={}, kb12={}", k1 + h1 + k2, kb12);
        LOG_DEBUGH(L"kanji23={}, kb23={}", k2 + h2 + k3, kb23);
        LOG_DEBUGH(L"kanji34={}, kb34={}", k3 + h3 + k4, kb34);

        // 漢字を1文字ずつ扱う(ここでは漢字4文字は対象外)
        String patt;
        String patt1;

        if (!k4.empty()) {
            // 4
            if (!_isNil(ky1) && !_isNil(ky2) && !_isNil(ky3) && !_isNil(ky4)) {
                SCAN_AND_RETURN_IF_MATCH(1, yomi, std::format(L"({}){}({}){}({}){}({})", _ky1, h1, _ky2, h2, _ky3, h3, ky4));
            }

            // 3
            if (!_isNil(ky1) && !_isNil(ky2) && !_isNil(ky3)) {
                SCAN_AND_RETURN_IF_MATCH(2, yomi, std::format(L"({}){}({}){}({}){}({})", _ky1, h1, _ky2, h2, ky3, h3, HIRAGANA_PATT));
            }
            if (!_isNil(ky1) && !_isNil(ky2) && !_isNil(ky4)) {
                SCAN_AND_RETURN_IF_MATCH(3, yomi, std::format(L"({}){}({}){}({}){}({})", _ky1, h1, ky2, h2, HIRAGANA_PATT, h3, ky4));
            }
            if (!_isNil(ky1) && !_isNil(ky3) && !_isNil(ky4)) {
                SCAN_AND_RETURN_IF_MATCH(4, yomi, std::format(L"({}){}({}){}({}){}({})", ky1, h1, HIRAGANA_PATT, h2, _ky3, h3, ky4));
            }
            if (!_isNil(ky2) && !_isNil(ky3) && !_isNil(ky4)) {
                SCAN_AND_RETURN_IF_MATCH(5, yomi, std::format(L"({}){}({}){}({}){}({})", HIRAGANA_PATT, h1, _ky2, h2, ky3, h3, ky4));
            }

            // 2
            if (!_isNil(ky1) && !_isNil(ky2)) {
                if (!h3.empty()) {
                    SCAN_AND_RETURN_IF_MATCH(6, yomi, std::format(L"({}){}({}){}({}){}({})", _ky1, h1, ky2, h2, HIRAGANA_PATT, h3, HIRAGANA_PATT));
                } else if (!_isNil(kb34)) {
                    SCAN_AND_RETURN_IF_MATCH_3C(7, yomi, std::format(L"({}){}({}){}({})", _ky1, h1, _ky2, h2, kb34));
                } else {
                    SCAN_AND_RETURN_IF_MATCH_3C(8, yomi, std::format(L"({}){}({}){}({})", _ky1, h1, ky2, h2, HIRAGANA_PATT));
                }
            }
            if (!_isNil(ky1) && !_isNil(ky3)) {
                SCAN_AND_RETURN_IF_MATCH(9, yomi, std::format(L"({}){}({}){}({}){}({})", ky1, h1, HIRAGANA_PATT, h2, ky3, h3, HIRAGANA_PATT));
            }
            if (!_isNil(ky1) && !_isNil(ky4)) {
                if (!h2.empty()) {
                    SCAN_AND_RETURN_IF_MATCH(10, yomi, std::format(L"({}){}({}){}({}){}({})", ky1, h1, HIRAGANA_PATT, h2, HIRAGANA_PATT, h3, ky4));
                } else if (!_isNil(kb23)) {
                    SCAN_AND_RETURN_IF_MATCH_3B(11, yomi, std::format(L"({}){}({}){}({})", _ky1, h1, kb23, h3, ky4));
                } else {
                    SCAN_AND_RETURN_IF_MATCH_3B(12, yomi, std::format(L"({}){}({}){}({})", ky1, h1, HIRAGANA_PATT, h3, ky4))
                };
            }
            if (!_isNil(ky2) && !_isNil(ky3)) {
                SCAN_AND_RETURN_IF_MATCH(13, yomi, std::format(L"({}){}({}){}({}){}({})", HIRAGANA_PATT, h1, _ky2, h2, ky3, h3, HIRAGANA_PATT));
            }
            if (!_isNil(ky2) && !_isNil(ky4)) {
                SCAN_AND_RETURN_IF_MATCH(14, yomi, std::format(L"({}){}({}){}({}){}({})", HIRAGANA_PATT, h1, ky2, h2, HIRAGANA_PATT, h3, ky4));
            }
            if (!_isNil(ky3) && !_isNil(ky4)) {
                if (!h1.empty()) {
                    SCAN_AND_RETURN_IF_MATCH(15, yomi, std::format(L"({}){}({}){}({}){}({})", HIRAGANA_PATT, h1, HIRAGANA_PATT, h2, ky3, h3, ky4));
                } else if (!_isNil(kb12)) {
                    SCAN_AND_RETURN_IF_MATCH_3A(16, yomi, std::format(L"({}){}({}){}({})", kb12, h2, ky3, h3, ky4));
                } else {
                    SCAN_AND_RETURN_IF_MATCH_3A(17, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h1, _ky3, h3, ky4))
                };
            }
            // 1
            if (!_isNil(ky1)) {
                if (!h2.empty() && !h3.empty()) {
                    SCAN_AND_RETURN_IF_MATCH(18, yomi, std::format(L"({}){}({}){}({}){}({})", ky1, h1, HIRAGANA_PATT, h2, HIRAGANA_PATT, h3, HIRAGANA_PATT));
                } else {
                    if (!_isNil(kb34)) {
                        SCAN_AND_RETURN_IF_MATCH_3C(19, yomi, std::format(L"({}){}({}){}({})", ky1, h1, HIRAGANA_PATT, h2, kb34));
                    }
                    if (!_isNil(kb23)) {
                        SCAN_AND_RETURN_IF_MATCH_3B(20, yomi, std::format(L"({}){}({}){}({})", _ky1, h1, kb23, h3, HIRAGANA_PATT));
                    }
                    if (!h2.empty() && h3.empty()) {
                        SCAN_AND_RETURN_IF_MATCH_3C(21, yomi, std::format(L"({}){}({}){}({})", ky1, h1, HIRAGANA_PATT, h2, HIRAGANA_PATT));
                    } else if (h2.empty() && !h3.empty()) {
                        SCAN_AND_RETURN_IF_MATCH_3B(22, yomi, std::format(L"({}){}({}){}({})", ky1, h1, HIRAGANA_PATT, h3, HIRAGANA_PATT));
                    } else {
                        SCAN_AND_RETURN_IF_MATCH_2C(23, yomi, std::format(L"({}){}({})", ky1, h1, HIRAGANA_PATT));
                    }
                }
            } else if (!_isNil(ky2)) {
                if (!h3.empty()) {
                    SCAN_AND_RETURN_IF_MATCH(24, yomi, std::format(L"({}){}({}){}({}){}({})", HIRAGANA_PATT, h1, ky2, h2, HIRAGANA_PATT, h3, HIRAGANA_PATT));
                } else {
                    if (!_isNil(kb34)) {
                        SCAN_AND_RETURN_IF_MATCH_3C(25, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h1, _ky2, h2, kb34));
                    } else {
                        SCAN_AND_RETURN_IF_MATCH_3C(26, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h1, ky2, h2, HIRAGANA_PATT));
                    }
                }
            } else if (!_isNil(ky3)) {
                if (!h1.empty()) {
                    SCAN_AND_RETURN_IF_MATCH(27, yomi, std::format(L"({}){}({}){}({}){}({})", HIRAGANA_PATT, h1, HIRAGANA_PATT, h2, ky3, h3, HIRAGANA_PATT));
                } else {
                    if (!_isNil(kb12)) {
                        SCAN_AND_RETURN_IF_MATCH_3A(28, yomi, std::format(L"({}){}({}){}({})", kb12, h2, ky3, h3, HIRAGANA_PATT));
                    } else {
                        SCAN_AND_RETURN_IF_MATCH_3A(29, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h2, ky3, h3, HIRAGANA_PATT));
                    }
                }
            } else if (!_isNil(ky4)) {
                if (!h2.empty() && !h3.empty()) {
                    SCAN_AND_RETURN_IF_MATCH(30, yomi, std::format(L"({}){}({}){}({}){}({})", HIRAGANA_PATT, h1, HIRAGANA_PATT, h2, HIRAGANA_PATT, h3, ky4));
                } else {
                    if (!_isNil(kb12)) {
                        SCAN_AND_RETURN_IF_MATCH_3A(31, yomi, std::format(L"({}){}({}){}({})", kb12, h2, HIRAGANA_PATT, h3, ky4));
                    }
                    if (!_isNil(kb23)) {
                        SCAN_AND_RETURN_IF_MATCH_3B(32, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h2, kb23, h3, ky4));
                    }
                    if (!h1.empty() && h2.empty()) {
                        SCAN_AND_RETURN_IF_MATCH_3B(33, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h1, HIRAGANA_PATT, h3, ky4));
                    } else if (h1.empty() && !h2.empty()) {
                        SCAN_AND_RETURN_IF_MATCH_3A(34, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h2, HIRAGANA_PATT, h3, ky4));
                    } else {
                        SCAN_AND_RETURN_IF_MATCH_2A(35, yomi, std::format(L"({}){}({})", HIRAGANA_PATT, h3, ky4));
                    }
                }
            }
            // 0
            if (!_isNil(kb12) && !_isNil(kb34)) {
                SCAN_AND_RETURN_IF_MATCH_2B(36, yomi, std::format(L"({}){}({})", kb12, h2, kb34));
            }
            if (!_isNil(kb12)) {
                if (!h3.empty()) {
                    SCAN_AND_RETURN_IF_MATCH_3A(37, yomi, std::format(L"({}){}({}){}({})", kb12, h2, HIRAGANA_PATT, h3, HIRAGANA_PATT));
                } else {
                    SCAN_AND_RETURN_IF_MATCH_2B(38, yomi, std::format(L"({}){}({})", kb12, h2, HIRAGANA_PATT));
                }
            }
            if (!_isNil(kb23)) {
                SCAN_AND_RETURN_IF_MATCH_3B(39, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h1, kb23, h3, HIRAGANA_PATT));
            }
            if (!_isNil(kb34)) {
                if (!h1.empty()) {
                    SCAN_AND_RETURN_IF_MATCH_3C(40, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h1, HIRAGANA_PATT, h2, kb34));
                } else {
                    SCAN_AND_RETURN_IF_MATCH_2B(41, yomi, std::format(L"({}){}({})", HIRAGANA_PATT, h2, kb34));
                }
            }
        } else if (!k3.empty()) {
            // 漢字3文字の場合
            // 3
            if (!_isNil(ky1) && !_isNil(ky2) && !_isNil(ky3)) {
                SCAN_AND_RETURN_IF_MATCH(42, yomi, std::format(L"({}){}({}){}({})", _ky1, h1, _ky2, h2, ky3));
            }
            if (!_isNil(ky1) && !_isNil(kb23)) {
                SCAN_AND_RETURN_IF_MATCH_2C(47, yomi, std::format(L"({}){}({})", ky1, h1, kb23));
            }
            if (!_isNil(kb12) && !_isNil(ky3)) {
                SCAN_AND_RETURN_IF_MATCH_2B(51, yomi, std::format(L"({}){}({})", kb12, h2, HIRAGANA_PATT));
            }
            // 2
            if (!_isNil(ky2) && !_isNil(ky3)) {
                SCAN_AND_RETURN_IF_MATCH(45, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h1, _ky2, h2, ky3));
            }
            if (!_isNil(ky1) && !_isNil(ky2)) {
                SCAN_AND_RETURN_IF_MATCH(43, yomi, std::format(L"({}){}({}){}({})", _ky1, h1, ky2, h2, HIRAGANA_PATT));
            }
            if (!_isNil(ky1) && !_isNil(ky3)) {
                SCAN_AND_RETURN_IF_MATCH(44, yomi, std::format(L"({}){}({}){}({})", ky1, h1, HIRAGANA_PATT, h2, ky3));
            }

            if (!_isNil(kb23)) {
                SCAN_AND_RETURN_IF_MATCH_2C(47, yomi, std::format(L"({}){}({})", HIRAGANA_PATT, h1, kb23));
            }
            if (!_isNil(kb12)) {
                SCAN_AND_RETURN_IF_MATCH_2B(51, yomi, std::format(L"({}){}({})", kb12, h2, HIRAGANA_PATT));
            }

            // 1
            if (!_isNil(ky3)) {
                if (!h1.empty()) {
                    SCAN_AND_RETURN_IF_MATCH(50, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h1, HIRAGANA_PATT, h2, ky3));
                } else {
                    SCAN_AND_RETURN_IF_MATCH_2B(52, yomi, std::format(L"({}){}({})", HIRAGANA_PATT, h2, ky3))
                };
            }
            if (!_isNil(ky2)) {
                SCAN_AND_RETURN_IF_MATCH(49, yomi, std::format(L"({}){}({}){}({})", HIRAGANA_PATT, h1, ky2, h2, HIRAGANA_PATT));
            }
            if (!_isNil(ky1)) {
                if (!h2.empty()) {
                    SCAN_AND_RETURN_IF_MATCH(46, yomi, std::format(L"({}){}({}){}({})", ky1, h1, HIRAGANA_PATT, h2, HIRAGANA_PATT));
                } else {
                    SCAN_AND_RETURN_IF_MATCH_2C(48, yomi, std::format(L"({}){}({})", ky1, h1, HIRAGANA_PATT));
                }
            }
            // 0
            //if (!_isNil(kb12)) {
            //    SCAN_AND_RETURN_IF_MATCH_2B(53, yomi, std::format(L"({}){}({})", kb12, h2, HIRAGANA_PATT));
            //}
            //if (!_isNil(kb23)) {
            //    SCAN_AND_RETURN_IF_MATCH_2C(54, yomi, std::format(L"({}){}({})", HIRAGANA_PATT, h1, kb23));
            //}
        } else {
            // 漢字2文字の場合
            // 2
            if (!_isNil(ky1) && !_isNil(ky2)) {
                SCAN_AND_RETURN_IF_MATCH(55, yomi, std::format(L"({}){}({})", _ky1, h1, ky2));
            }
            // 1
            if (!_isNil(ky2)) {
                SCAN_AND_RETURN_IF_MATCH(56, yomi, std::format(L"({}){}({})", HIRAGANA_PATT, h1, ky2));
            }
            if (!_isNil(ky1)) {
                SCAN_AND_RETURN_IF_MATCH(57, yomi, std::format(L"({}){}({})", ky1, h1, HIRAGANA_PATT));
            }
            // 0
        }

        LOG_DEBUG(L"LEAVE: []");
        return {};
    }

    void loadFiles(StringRef joyoKanjiFile, StringRef kanjiBigramFile) {
        // 常用漢字表の読み込み
        if (!utils::isFileExistent(joyoKanjiFile)) {
            ERROR_HANDLER->Error(L"常用漢字表ファイルが存在しません: " + joyoKanjiFile);
            return;
        }
        _loadJoyoKanjiFile(joyoKanjiFile);

        // 漢字bigramの読み込み
        if (!utils::isFileExistent(kanjiBigramFile)) {
            ERROR_HANDLER->Error(L"漢字bigramファイルが存在しません: " + joyoKanjiFile);
            return;
        }
        _loadKanjiBigramFile(kanjiBigramFile);
    }

} // MazegakiPreprocessor

