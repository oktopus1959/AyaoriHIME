#include "MazePrepro_Common.h"
#include "reporting/ErrorHandler.h"

#if 0
#undef LOG_DEBUGH
#undef LOG_DEBUG
#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFO
#endif

namespace MazegakiPreprocessor {
    DEFINE_NAMESPACE_LOGGER(MazegakiPreprocessor);


    int _mazeCost = 0;

    inline String _getNthYomi(VectorString yomis, size_t n) {
        if (n < yomis.size()) {
            return yomis[n];
        }
        return L"nil";
    }

    /** 交ぜ書きバリエーションの作成 (!k1.empty() && !k2.empty())
    */
    VectorString makeVariation(StringRef yomi, StringRef h1, String k1, StringRef h2, String k2, StringRef h3, String k3, StringRef h4, String k4, StringRef r) {
        LOG_DEBUGH(L"make_variation: ENTER: yomi={}, h1={}, k1={}, h2={}, k2={}, h3={}, k3={}, h4={}, k4={}, r={}", yomi, h1, k1, h2, k2, h3, k3, h4, k4, r);

        if (yomi.empty() || yomi == L"#") {
            return {}; // 空の読みは無視
        }

        //auto yomis = findYomi(yomi, h1, k1, h2, k2 == L"々" ? k1 : k2, h3, k3.empty() ? k2 : k3, h4, k4.empty() ? k3 : k4, r);
        auto subYomi = utils::safe_substr(utils::safe_substr(yomi, h1.size()), 0, -(int)r.size());
        auto yomis = findYomi(subYomi, k1, h2, k2 == L"々" ? k1 : k2, h3, k3, h4, k4);
        LOG_DEBUGH(L"yomis={}", utils::join(yomis, L","));

        if (yomis.empty()) {
            return {};
        }

        if (IS_LOG_DEBUG_ENABLED) {
            const auto y1 = _getNthYomi(yomis, 0);
            const auto y2 = _getNthYomi(yomis, 1);
            const auto y3 = _getNthYomi(yomis, 2);
            const auto y4 = _getNthYomi(yomis, 3);

            // STDERR.puts "make_variation: after find_yomi: #{k1}=#{y1}, #{k2}=#{y2 ? y2 : 'nil'}, #{k3}=#{y3 ? y3 : 'nil'}, #{k4}=#{y4 ? y4 : 'nil'}" if $verbose
            LOG_DEBUGH(L"make_variation: after find_yomi: yomi={}, yomis=[{}], <{}>=<{}>, <{}>=<{}>, <{}>=<{}>, <{}>=<{}>",
                yomi, utils::join(yomis, L","), k1, y1, k2, y2, k3, y3, k4, y4);
        }
        SetString mazeSet;

        if (yomis.size() >= 4) {
            const auto y1 = _safeGet(yomis, 0);
            const auto y2 = _safeGet(yomis, 1);
            const auto y3 = _safeGet(yomis, 2);
            const auto y4 = _safeGet(yomis, 3);
            if (y2.empty()) {
                k1 = k1 + k2;
                k2 = L"";
            }
            if (y3.empty()) {
                k2 = k2 + k3;
                k3 = L"";
            }
            if (y4.empty()) {
                k3 = k3 + k4;
                k4 = L"";
            }
            //STDERR.puts "make_variation 4: #{k1}=#{y1}, #{k2}=#{y2}, #{k3}=#{y3}, #{k4}=#{y4}" if $verbose
            LOG_DEBUGH(L"make_variation 4: {}={}, {}={}, {}={}, {}={}", k1, y1, k2, y2, k3, y3, k4, y4);
            mazeSet.insert(h1 + k1 + h2 + k2 + h3 + k3 + h4 + y4 + r);
            mazeSet.insert(h1 + y1 + h2 + k2 + h3 + k3 + h4 + y4 + r);
            mazeSet.insert(h1 + k1 + h2 + y2 + h3 + k3 + h4 + y4 + r);
            mazeSet.insert(h1 + y1 + h2 + y2 + h3 + k3 + h4 + y4 + r);
            mazeSet.insert(h1 + k1 + h2 + k2 + h3 + y3 + h4 + y4 + r);
            mazeSet.insert(h1 + y1 + h2 + k2 + h3 + y3 + h4 + y4 + r);
            mazeSet.insert(h1 + k1 + h2 + y2 + h3 + y3 + h4 + y4 + r);
            mazeSet.insert(h1 + y1 + h2 + k2 + h3 + k3 + h4 + k4 + r);
            mazeSet.insert(h1 + k1 + h2 + y2 + h3 + k3 + h4 + k4 + r);
            mazeSet.insert(h1 + y1 + h2 + y2 + h3 + k3 + h4 + k4 + r);
            mazeSet.insert(h1 + k1 + h2 + k2 + h3 + y3 + h4 + k4 + r);
            mazeSet.insert(h1 + y1 + h2 + k2 + h3 + y3 + h4 + k4 + r);
            mazeSet.insert(h1 + k1 + h2 + y2 + h3 + y3 + h4 + k4 + r);
            mazeSet.insert(h1 + y1 + h2 + y2 + h3 + y3 + h4 + k4 + r);
        } else if (yomis.size() == 3) {
            const auto y1 = _safeGet(yomis, 0);
            const auto y2 = _safeGet(yomis, 1);
            const auto y3 = _safeGet(yomis, 2);
            if (y2.empty() && !k2.empty()) {
                k1 = k1 + k2;
                k2 = L"";
            }
            if (y3.empty() && !k3.empty()) {
                k2 = k2 + k3;
                k3 = L"";
            }
            if (!k4.empty()) {
                k3 = k3 + k4;
            }
            //STDERR.puts "make_variation 3: #{k1}=#{y1}, #{k2}=#{y2}, #{k3}=#{y3}" if $verbose
            LOG_DEBUGH(L"make_variation 3: {}={}, {}={}, {}={}", k1, y1, k2, y2, k3, y3);
            mazeSet.insert(h1 + k1 + h2 + k2 + h3 + y3 + r);
            mazeSet.insert(h1 + y1 + h2 + k2 + h3 + y3 + r);
            mazeSet.insert(h1 + k1 + h2 + y2 + h3 + y3 + r);
            mazeSet.insert(h1 + y1 + h2 + k2 + h3 + k3 + r);
            mazeSet.insert(h1 + k1 + h2 + y2 + h3 + k3 + r);
            mazeSet.insert(h1 + y1 + h2 + y2 + h3 + k3 + r);
        } else if (yomis.size() == 2) {
            const auto y1 = _safeGet(yomis, 0);
            const auto y2 = _safeGet(yomis, 1);
            //STDERR.puts "make_variation 2: #{k1}=#{y1}, #{k2}=#{y2}" if $verbose
            LOG_DEBUGH(L"make_variation 2: {}={}, {}={}", k1, y1, k2, y2);
            mazeSet.insert(h1 + k1 + h2 + y2 + r);
            mazeSet.insert(h1 + y1 + h2 + k2 + r);
        }
        RegexUtil reKanji(L"[一-龠々]+");
        VectorString result;
        std::copy_if(mazeSet.begin(), mazeSet.end(), std::back_inserter(result), [&reKanji](StringRef iter) {return !reKanji.match(iter);});
        return result;
    }

    std::map<String, VectorString> kformDefs = {
        // 五段・カ行イ音便
        { L"五段・カ行イ音便",
            { L"未然形,か,683", L"未然ウ接続,こ,681", L"連用形,き,689", L"連用タ接続,い,687", L"仮定形,け,675", L"命令ｅ,せ,685", L"仮定縮約１,きゃ,677" }},

        // 五段・サ行
        { L"五段・サ行",
            { L"未然形,さ,733", L"未然ウ接続,そ,732", L"連用形,し,735", L"仮定形,せ,729", L"命令ｅ,せ,734", L"仮定縮約１,しゃ,730" }},

        // 五段・マ行
        { L"五段・マ行",
            { L"未然形,ま,764", L"未然ウ接続,も,763", L"連用形,み,767", L"連用タ接続,ん,766", L"仮定形,め,760", L"命令ｅ,め,765", L"仮定縮約１,みゃ,761" }},

        // 五段・ラ行
        { L"五段・ラ行",
            { L"未然形,ら,780", L"未然形,ん,782", L"未然ウ接続,ろ,778", L"連用形,り,788", L"連用タ接続,っ,786", L"仮定形,れ,768",
              L"命令ｅ,れ,784", L"仮定縮約１,りゃ,770", L"体言接続特殊,ん,774" }},

        // 五段・ワ行促音便
        { L"五段・ワ行促音便",
            { L"未然形,わ,823", L"未然ウ接続,お,820", L"連用形,い,832", L"連用タ接続,っ,829", L"仮定形,え,814", L"命令ｅ,え,826" }},

        // 一段
        { L"一段",
            { L"未然形,,622", L"未然ウ接続,よ,621", L"連用形,,625", L"仮定形,れ,617", L"命令ｙｏ,よ,624", L"命令ｒｏ,ろ,623", L"仮定縮約１,りゃ,618", L"体言接続特殊,ん,620" }},

        // サ変・−スル
        { L"サ変・−スル",
            { L"基本形,する,583", L"未然形,し,587", L"未然ウ接続,しよ,585", L"未然ウ接続,しょ,585", L"未然レル接続,せ,586", L"仮定形,すれ,581",
              L"命令ｙｏ,せよ,589", L"命令ｒｏ,しろ,588", L"仮定縮約１,すりゃ,582" }},

        // 形容詞・イ段
        { L"形容詞・イ段",
            { L"文語基本形,,45", L"未然ヌ接,から,47", L"未然ウ接続,かろ,46", L"連用タ接続,かっ,50", L"連用テ接続,く,51", L"連用テ接続,くっ,51", L"連用ゴザイ接続,ゅう,49",
              L"体言接続,き,44", L"仮定形,けれ,40", L"命令ｅ,かれ,48", L"仮定縮約１,けりゃ,41", L"仮定縮約２,きゃ,42", L"ガル接続,,39" }},

        // 形容詞・アウオ段
        { L"形容詞・アウオ段",
            { L"文語基本形,し,23", L"未然ヌ接,から,27", L"未然ウ接続,かろ,25", L"連用タ接続,かっ,33", L"連用テ接続,く,35", L"連用テ接続,くっ,35", L"連用ゴザイ接続,う,31",
              L"体言接続,き,21", L"仮定形,けれ,13", L"命令ｅ,かれ,29", L"仮定縮約１,けりゃ,15", L"仮定縮約２,きゃ,17", L"ガル接続,,11" }},

    };

    // 活用形の展開
    // line は余分な空白や先頭の * が削除されている
    VectorString expandKForm(StringRef line) {
        LOG_DEBUGH(L"ENTER: line={}", line);

        VectorString expandedList;

        VectorString items = utils::split(line, L",");
        LOG_DEBUGH(L"items={}", utils::join(items, L", "));
        if (items.size() > 11) {
            String surf = items[0];
            LOG_DEBUGH(L"push_back({})", utils::join(items, L", "));
            expandedList.push_back(utils::join(items, ','));

            String yomi = items[11];
            if (surf.size() > 1 && yomi.size() > 1) {
                String stem = utils::safe_substr(surf, 0, -1);
                String yomiStem = utils::safe_substr(yomi, 0, -1);
                LOG_DEBUGH(L"stem={}, yomiStem={}", stem, yomiStem);

                String ktype = items[8];
                auto defs = kformDefs.find(ktype);
                if (defs != kformDefs.end()) {
                    for (const auto& val : defs->second) {
                        auto kfs = utils::split(val, ',');
                        //LOG_DEBUGH(L"kfs={}", utils::join(kfs, L", "));
                        String tail = kfs.size() > 1 ? kfs[1] : L"";
                        String conn = kfs.size() > 2 ? kfs[2] : items[1];
                        items[0] = stem + tail;     // 表層形
                        items[1] = conn;            // 左接続
                        items[2] = conn;            // 右接続
                        items[9] = kfs[0];          // 活用形
                        items[11] = yomiStem + tail;  // 読み
                        LOG_DEBUGH(L"push_back({})", utils::join(items, L", "));
                        expandedList.push_back(utils::join(items, ','));
                    }
                }
            }
        }
        if (expandedList.empty()) {
            expandedList.push_back(line);
        }

        LOG_DEBUGH(L"LEAVE: result size={}", expandedList.size());
        return expandedList;
    }

    String outputEntry(StringRef surf, StringRef base, VectorRefString items) {
        // puts "#{surf},#{items[1..9].join(',')},#{base},#{items[11]},#{items[12]}"
        return std::format(L"{},{},{},{},{}", surf, utils::join(items.begin() + 1, items.begin() + 9, L","), base, items[11], _safeGet(items, 12));
    }

    //# 語幹から基本形を得るための辞書
    MapString _mazeBaseMap;

    //def checkMazeBase(mazeYomi, ktype, kform)
    //    if kform == "基本形"
    //      if ktype.start_with?("サ変")
    //        mazeStem = mazeYomi[0...-2]
    //      else
    //        mazeStem = mazeYomi[0...-1]
    //      end
    //      STDERR.puts "_mazeBaseMap[#{mazeStem}] = #{mazeYomi}"  if $verbose
    //      $_mazeBaseMap[mazeStem] = mazeYomi
    //    end
    //end
    void checkMazeBase(StringRef mazeYomi, StringRef ktype, StringRef kform) {
        LOG_DEBUGH(L"ENTER: mazeYomi={}, ktype={}, kform={}", mazeYomi, ktype, kform);
        if (kform == L"基本形") {
            String mazeStem;
            if (ktype.starts_with(L"サ変")) {
                mazeStem = utils::safe_substr(mazeYomi, 0, -2);
            } else {
                mazeStem = utils::safe_substr(mazeYomi, 0, -1);
            }
            LOG_DEBUGH(L"_mazeBaseMap[{}] = {}", mazeStem, mazeYomi);
            _mazeBaseMap[mazeStem] = mazeYomi;
        }
    }

    //def getMazeBase(mazeYomi)
    //  $mazeBaseMap.each {|stem, base|
    //    if mazeYomi.start_with?(stem)
    //      STDERR.puts "getMazeBase(#{mazeYomi}) => stem: #{stem}, base:#{base}"  if $verbose
    //      return base
    //    end
    //  }
    //  return mazeYomi
    //end
    String getMazeBase(StringRef mazeYomi) {
        LOG_DEBUGH(L"ENTER: mazeYomi={}", mazeYomi);
        for (const auto& [stem, base] : _mazeBaseMap) {
            if (mazeYomi.starts_with(stem)) {
                LOG_DEBUGH(L"getMazeBase({}) => stem: {}, base: {}", mazeYomi, stem, base);
                return base;
            }
        }
        LOG_DEBUGH(L"LEAVE: getMazeYomi({}) => {}", mazeYomi, mazeYomi);
        return mazeYomi;  // デフォルトはそのまま返す
    }

    //# main loop
    //def main(results, costs, items)
    bool main(MapString& results, MapInt& costs, StringRef line) {
        LOG_DEBUGH(L"\nENTER: line={}", line);

        const auto items = utils::split(line, L",");
        LOG_DEBUGH(L"join={}", utils::join(items, L" | "));
        if (items.size() < 12) {
            LOG_DEBUGH(L"LEAVE: items size < 12");
            return false; // 不正なアイテム数
        }

        String surf = _safeGet(items, 0);
        String leftId = _safeGet(items, 1);
        String rightId = _safeGet(items, 2);
        int cost = utils::stringToInt(_safeGet(items, 3));
        String majorPos = _safeGet(items, 4);
        String minorPos = _safeGet(items, 5);
        //String pos1 = items[4];
        //String pos2 = items[5];
        //String pos3 = items[6];
        //String pos4 = items[7];
        String ktype = _safeGet(items, 8);
        String kform = _safeGet(items, 9);
        String base = _safeGet(items, 10);
        String hiraYomi = utils::strip(_kataka2hiragana(_safeGet(items, 11)));

        String hinshi = majorPos + L":" + minorPos;
        String keyRest = std::format(L"{},{},{}", leftId, rightId, hinshi);

        LOG_DEBUGH(L"surf={}, leftId={}, rightId={}, cost={}, hinshi={}, ktype={}, kform={}, base={}, hiraYomi={}, keyRest={}",
            surf, leftId, rightId, cost, hinshi, ktype, kform, base, hiraYomi, keyRest);

        // 漢字・ひらがな・カタカナ・句読点以外を含むやつは除外
        if (!_reMatch(surf, L"[ぁ-んァ-ヶー・一-龠々、。〓「」『』]+")) {
            LOG_DEBUGH(L"LEAVE: invalid surf={}", surf);
            return false;
        }

        // 元の定義を追加
        //  origKey = "#{surf},#{keyRest},#{base}"
        String origKey = std::format(L"{},{},{}", surf, keyRest, base);
        //  origLine = "#{items[0..3].join(",")},#{base},#{hinshi},#{base}"
        String origLine = std::format(L"{},{},{},{},{}", utils::join(items.begin(), items.begin() + 4, L","), hinshi, base, base, surf);
        //  STDERR.puts "results[#{origKey}] = #{origLine}, cost=#{cost}, costs[origKey]=#{costs[origKey]}"  if $verbose
        LOG_DEBUGH(L"main::check results[origKey={}]=\"{}\", cost={}, costs[origKey]={}", origKey, _safeGet(results, origKey), cost, _safeGet(costs, origKey));
        //  if !results[origKey] || cost < costs[origKey]
        //    results[origKey] = origLine
        //    costs[origKey] = cost
        //  end
        if (!results.contains(origKey) || cost < _safeGet(costs, origKey)) {
            LOG_DEBUGH(L"main::put results[origKey={}]=\"{}\"", origKey, origLine);
            results[origKey] = origLine;
            costs[origKey] = cost;
        }

        ////  STDERR.puts "surf=#{surf}" if $verbose
        //LOG_DEBUGH(L"surf={}", surf);

        //// 漢字を含まないやつは除外
        //if (!_reSearch(surf, L"[一-龠々]")) {
        //    LOG_DEBUGH(L"LEAVE: no kanji in surf={}", surf);
        //}

        //  # 交ぜ書きエントリーの追加処理
        //  # feature の末尾に "MAZE" を追加
        //  #mazeRest = "#{leftId},#{rightId},#{cost+_mazeCost},#{base},#{hinshi},#{surf},MAZE"
        //  mazeRestHead = "#{leftId},#{rightId},#{cost+_mazeCost}"
        //  mazeRestTail = "#{hinshi},#{base},#{surf},MAZE"
        String mazeRestHead = std::format(L"{},{},{}", leftId, rightId, cost + _mazeCost);
        String mazeRestTail = std::format(L"{},{},MAZE", base, surf);

        //#  next if kf != '*' && kf != '基本形'

        //#  pos = getPos(pos1,pos2,pos3,pos4,kt)
        //#  next unless pos

        //  #outputEntry("#{surf}\t#{surf}\t#{pos}", "#{hiraYomi} -- BASE")

        //  if kform == "基本形"
        //    $mazeBaseMap = {}
        //  end
        if (kform == L"基本形") {
            _mazeBaseMap.clear();
        }

        //const auto surfParts = _reMatchScan(surf, L"^([^一-龠々]*)([一-龠々])([^一-龠々]*)([一-龠々])(?:([^一-龠々]*)([一-龠々])(?:([^一-龠々]*)([一-龠々]))?)?([ぁ-ゖ]*)$");
        const auto surfParts = _reMatchScan(surf, L"^([ぁ-ゖ]*)([一-龠々])([ぁ-ゖ]*)([一-龠々])(?:([ぁ-ゖ]*)([一-龠々])(?:([ぁ-ゖ]*)([一-龠々]))?)?([ぁ-ゖ]*)$");
        if (surfParts.size() == 9) {
            // 2つ以上の漢字を含む (カタカナは含まない)
            auto variations = makeVariation(hiraYomi,
                _safeGet(surfParts, 0), _safeGet(surfParts, 1), _safeGet(surfParts, 2),
                _safeGet(surfParts, 3), _safeGet(surfParts, 4), _safeGet(surfParts, 5),
                _safeGet(surfParts, 6), _safeGet(surfParts, 7), _safeGet(surfParts, 8));
            for (const auto& mazeYomi : variations) {
                //outputEntry("#{mazeYomi}\t#{surf}\t#{pos}", "#{hiraYomi}") if (_reSearch(mazeYomi, L"[一-龠々]"))
                //outputEntry(mazeYomi, surf, lcost, rcost, wcost, surfParts)
                // 交ぜ書きの定義を追加(元の定義を上書きする可能性あり)
                LOG_DEBUGH(L"mazeYomi={}", mazeYomi);
                checkMazeBase(mazeYomi, ktype, kform);
                String mazeKey = std::format(L"{},{},{}", mazeYomi, keyRest, surf);
                LOG_DEBUGH(L"main::check results[mazeKey={}]=\"{}\", cost={}, costs[mazeKey]={}", mazeKey, _safeGet(results, mazeKey), cost, _safeGet(costs, mazeKey));
                if (!results.contains(mazeKey) || cost < _safeGet(costs, mazeKey)) {
                    String mazeLine = std::format(L"{},{},{},{},{}", mazeYomi, mazeRestHead, hinshi, getMazeBase(mazeYomi), mazeRestTail);
                    LOG_DEBUGH(L"main::put results[mazeKey={}]=\"{}\"", mazeKey, mazeLine);
                    results[mazeKey] = mazeLine;
                    costs[mazeKey] = cost;
                }
            }
        }
        //  # 表層がひらがな以外を含み、ひらがなだけの読みがある
        if (_reSearch(surf, L"[^ぁ-ゖ]") && !hiraYomi.empty()) {
            //outputEntry(hiraYomi, surf, surfParts)
            // 交ぜ書きの定義を追加(元の定義を上書きする可能性あり)
            LOG_DEBUGH(L"hiraYomi={}", hiraYomi);
            checkMazeBase(hiraYomi, ktype, kform);
            String mazeKey = std::format(L"{},{},{}", hiraYomi, keyRest, surf);
            LOG_DEBUGH(L"main::check results[mazeKey={}]=\"{}\", cost={}, costs[mazeKey]={}", mazeKey, _safeGet(results, mazeKey), cost, _safeGet(costs, mazeKey));
            if (!results.contains(mazeKey) || cost < _safeGet(costs, mazeKey)) {
                String mazeLine = std::format(L"{},{},{},{},{}", hiraYomi, mazeRestHead, hinshi, getMazeBase(hiraYomi), mazeRestTail);
                LOG_DEBUGH(L"main::put results[mazeKey={}]=(hiraYomi)\"{}\"", mazeKey, mazeLine);
                results[mazeKey] = mazeLine;
                costs[mazeKey] = cost;
            }
        }

        return true;
    }

    MapString linesMap;
    MapInt costsMap;

    // 活用型と交ぜ書きの前処理を行う
    // 辞書として有効な行の場合は true を返す。
    // (ユーザー辞書の形式に従っていないものはそのまま出力)
    bool expandMazegaki(StringRef origLine, Vector<String>& errorLines) {
        LOG_DEBUGH(L"ENTER: origLine={}", origLine);

        //items = origLine.strip.split(',')
        String line = utils::reReplace(utils::strip(origLine), L"\\s*,\\s*", L",");
        if (line.empty() || line[0] == '#') {
            LOG_DEBUGH(L"LEAVE: false: null or ignored");
            return false;
        }

        bool expandKFormFlag = false;
        if (line[0] == '*') {
            expandKFormFlag = true; // 活用形の展開フラグ
            line = line.substr(1);
        }
        if (line.empty()) {
            errorLines.push_back(origLine);
            LOG_DEBUGH(L"LEAVE: false: empty");
            return false;
        }

        const auto items = utils::split(line, L",");

        if (items[0].empty()) {
            errorLines.push_back(origLine);
            LOG_DEBUGH(L"LEAVE: false: empty head");
            return false;
        }

        if (_reSearch(items[0], L"[^ぁ-ゖァ-ヶー一-龠々、。〓「」『』]")) {
            // ひらがな・カタカナ・漢字・句読点以外を含むものは削除
            LOG_DEBUGH(L"LEAVE: false: head other than Japanese");
            return false;
        }

        //if (items[0].size() > 8 && _reSearch(items[0], L"[ァ-ヶ].*[ぁ-ゖ一-龠]|[ぁ-ゖ一-龠].*[ァ-ヶ]")) {
        //    // 8文字以上でカタカナのブロックが2つ以上あるものは削除
        //    LOG_DEBUGH(L"LEAVE: false");
        //    return false;
        //}

        if (items.size() < 11) {
            LOG_DEBUGH(L"LEAVE: true: less than normal format");
            // ユーザー辞書の形式に従っていないものはそのまま出力
            linesMap[origLine] = origLine;
            return true;
        }

        if (items[9] == L"体言接続特殊２") {
            // 活用形が体言接続特殊２のものは削除
            LOG_DEBUGH(L"LEAVE: false: tokushu2");
            return false;
        }

        bool mainResult = false;
        if (expandKFormFlag) {
            for (const auto& expLine : expandKForm(line)) {
                mainResult = main(linesMap, costsMap, expLine);
            }
        } else {
            mainResult = main(linesMap, costsMap, line);
        }

        LOG_DEBUGH(L"LEAVE: mainResult={}", mainResult);
        return mainResult;

    }

    // 出力
    void output(std::vector<String>& resultLines) {
        LOG_INFO(L"ENTER: resultLines.size={}, linesMap.size={}", resultLines.size(), linesMap.size());

        resultLines.reserve(resultLines.size() + linesMap.size()); // サイズを予約してコピー効率化
        for (const auto& [key, value] : linesMap) {
            resultLines.push_back(value);
        }

        linesMap.clear();
        costsMap.clear();

        LOG_INFO(L"ENTER: resultLines.size={}", resultLines.size());
    }

    /**
     * 交ぜ書きの前処理を初期化する
     * @param joyoKanjiFile 常用漢字表ファイルのパス
     * @param kanjiBigramFile 漢字bigramファイルのパス
     */
    void initialize(StringRef joyoKanjiFile, StringRef kanjiBigramFile) {
        LOG_DEBUGH(L"ENTER: joyoKanjiFile={}, kanjiBigramFile={}", joyoKanjiFile, kanjiBigramFile);

        linesMap.clear();
        costsMap.clear();

        loadFiles(joyoKanjiFile, kanjiBigramFile);

        LOG_DEBUGH(L"LEAVE: Initialization completed successfully.");
    }

    // 交ぜ書き辞書の準備と展開
    Vector<String> preprocessMazegakiDic(StringRef mazeResrcDir, StringRef dicSrcPath) {
        LOG_INFOH(L"ENTER: mazeResrcDir={}, dicSrcPath={}", mazeResrcDir, dicSrcPath);

        Vector<String> resultLines;

        MazegakiPreprocessor::initialize(utils::joinPath(mazeResrcDir, JOYO_KANJI_FILE), utils::joinPath(mazeResrcDir, KANJI_BIGRAM_FILE));
        if (ERROR_HANDLER->HasError()) {
            return resultLines;
        }

        utils::IfstreamReader reader(dicSrcPath);
        if (reader.fail()) {
            ERROR_HANDLER->Error(std::format(L"Could not open file: {}", dicSrcPath));
            LOG_WARN(ERROR_HANDLER->GetErrorMsg());
            return resultLines;
        }

        Vector<String> errorLines;
        int srcLineNum = 0;
        while (true) {
            auto [line, eof] = reader.getLine();
            _LOG_DEBUGH(L"line={}, eof={}", line, eof);
            if (eof || (line == L"." && dicSrcPath == L"-")) break;
            // 展開処理
            if (MazegakiPreprocessor::expandMazegaki(line, errorLines)) {
                ++srcLineNum;
            }
        }
        if (!errorLines.empty()) {
            ERROR_HANDLER->Warn(std::format(L"以下の辞書記述に問題があります:\n{}", utils::join(errorLines, L"\n")));
        }

        if (!ERROR_HANDLER->HasError()) {
            MazegakiPreprocessor::output(resultLines);
            ERROR_HANDLER->Info(std::format(L"辞書の前処理: 元の有効行数={}, 展開後行数={}", srcLineNum, resultLines.size()));
        }

        LOG_INFOH(L"LEAVE: resultLines.size={}", resultLines.size());
        return resultLines;

    }

} // MazegakiPreprocessor

