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

    std::map<String, String> ktypeDefs = {
        { L"形容詞",    L"43,43,X,形容詞,自立,*,*,形容詞・イ段,基本形" },
        { L"五段く",   L"679,679,X,動詞,自立,*,*,五段・カ行イ音便,基本形" },
        { L"五段ぐ",   L"723,723,X,動詞,自立,*,*,五段・ガ行,基本形" },
        { L"五段す",   L"731,731,X,動詞,自立,*,*,五段・サ行,基本形" },
        { L"五段つ",   L"738,738,X,動詞,自立,*,*,五段・タ行,基本形" },
        { L"五段ぬ",   L"746,746,X,動詞,自立,*,*,五段・ナ行,基本形" },
        { L"五段ぶ",   L"754,754,X,動詞,自立,*,*,五段・バ行,基本形" },
        { L"五段む",   L"762,762,X,動詞,自立,*,*,五段・マ行,基本形" },
        { L"五段る",   L"772,772,X,動詞,自立,*,*,五段・ラ行,基本形" },
        { L"五段う",   L"817,817,X,動詞,自立,*,*,五段・ワ行促音便,基本形" },
        { L"一段",      L"619,619,X,動詞,自立,*,*,一段,基本形" },
        { L"クル",      L"563,563,X,動詞,自立,*,*,カ変・クル,基本形" },
        { L"来ル",      L"573,573,X,動詞,自立,*,*,カ変・来ル,基本形" },
        { L"スル",      L"583,583,X,動詞,自立,*,*,サ変・−スル,基本形" },
        { L"ズル",      L"592,592,X,動詞,自立,*,*,サ変・−ズル,基本形" },
        { L"副詞",      L"1281,1281,X,副詞,一般,*,*,*,*" },
        { L"サ変",      L"1283,1283,X,名詞,サ変接続,*,*,*,*" },
        { L"名詞",      L"1285,1285,X,名詞,一般,*,*,*,*" },
        { L"形容動詞",  L"1287,1287,X,名詞,形容動詞語幹,*,*,*,*" },
        { L"固有名詞",  L"1288,1288,X,名詞,固有名詞,一般,*,*,*" },
        { L"人名",      L"1289,1289,X,名詞,固有名詞,人名,一般,*,*" },
        { L"姓名",      L"1289,1289,X,名詞,固有名詞,人名,一般,*,*" },
        { L"姓",        L"1290,1290,X,名詞,固有名詞,人名,姓,*,*" },
        { L"名",        L"1291,1291,X,名詞,固有名詞,人名,名,*,*" },
        { L"組織",      L"1292,1292,X,名詞,固有名詞,組織,*,*,*" },
        { L"地域",      L"1293,1293,X,名詞,固有名詞,地域,一般,*,*" },
        { L"地名",      L"1293,1293,X,名詞,固有名詞,地域,一般,*,*" },
        { L"国",        L"1294,1294,X,名詞,固有名詞,地域,国,*,*" },
        { L"数",        L"1295,1295,X,名詞,数,*,*,*,*" },
    };

    inline String replaceCost(String line, StringRef cost) {
        size_t pos = line.find(L"X");
        if (pos != String::npos) {
            line.replace(pos, 1, cost);
        }
        return line;
    }

    // ユーザー辞書のエントリを品詞によって標準形に変換
    String convertToNormalForm(StringRef line) {
        const auto items = utils::split(line, L",");
        LOG_DEBUGH(L"ENTER: line={}, items={}", line, utils::join(items, L":"));
        if (items.size() < 2 || items.size() > 4) {
            return line;
        }
        auto yomi = items[0];
        auto base = items[1];
        if (yomi.empty() || base.empty()) {
            return line;
        }
        auto hinshi = items.size() <= 2 || items[2].empty() ? L"名詞" : items[2];
        if (hinshi == L"地名") {
            hinshi = L"地域";
        } else if (hinshi == L"五段") {
            hinshi += base.back();
        }
        auto iter = ktypeDefs.find(hinshi);
        if (iter == ktypeDefs.end()) {
            return line;
        }
        auto cost = items.size() >= 4 ? items[3] : L"5000";
        LOG_DEBUGH(L"RESULT: line={}", base + L"," + replaceCost(iter->second, cost) + L"," + base + L"," + yomi);
        return base + L"," + replaceCost(iter->second, cost) + L"," + base + L"," + yomi;
    }

    std::map<String, VectorString> kformDefs = {
        // 五段・カ行イ音便
        { L"五段・カ行イ音便",
            { L"基本形,く,679", L"未然形,か,683", L"未然ウ接続,こ,681", L"連用形,き,689", L"連用タ接続,い,687", L"仮定形,け,675", L"命令ｅ,せ,685", L"仮定縮約１,きゃ,677"}
        },

        // 五段・ガ行
        { L"五段・ガ行",
            { L"基本形,ぐ,723", L"未然形,が,725", L"未然ウ接続,ご,724", L"連用形,ぎ,728", L"連用タ接続,い,727", L"仮定形,げ,721", L"命令ｅ,げ,726", L"仮定縮約１,ぎゃ,722"}
        },

        // 五段・サ行
        { L"五段・サ行",
            { L"基本形,す,731", L"未然形,さ,733", L"未然ウ接続,そ,732", L"連用形,し,735", L"仮定形,せ,729", L"命令ｅ,せ,734", L"仮定縮約１,しゃ,730" }
        },

        // 五段・タ行
        { L"五段・タ行",
            { L"基本形,つ,738", L"未然形,た,740", L"未然ウ接続,と,739", L"連用形,ち,743", L"連用タ接続,っ,742", L"仮定形,て,736", L"命令ｅ,て,741", L"仮定縮約１,ちゃ,737",}
        },

        // 五段・ナ行
        { L"五段・ナ行",
            { L"基本形,ぬ,746", L"未然形,な,748", L"未然ウ接続,の,747", L"連用形,に,751", L"連用タ接続,ん,750", L"仮定形,ね,744", L"命令ｅ,ね,749", L"仮定縮約１,にゃ,745"}
        },

        // 五段・バ行
        { L"五段・バ行",
            { L"基本形,ぶ,754", L"未然形,ば,756", L"未然ウ接続,ぼ,755", L"連用形,び,759", L"連用タ接続,ん,758", L"仮定形,べ,752", L"命令ｅ,べ,757", L"仮定縮約１,喚びゃ,753"}
        },

        // 五段・マ行
        { L"五段・マ行",
            { L"基本形,む,762", L"未然形,ま,764", L"未然ウ接続,も,763", L"連用形,み,767", L"連用タ接続,ん,766", L"仮定形,め,760", L"命令ｅ,め,765", L"仮定縮約１,みゃ,761" }
        },

        // 五段・ラ行
        { L"五段・ラ行",
            { L"基本形,る,772", L"未然形,ら,780", L"未然形,ん,782", L"未然ウ接続,ろ,778", L"連用形,り,788", L"連用タ接続,っ,786", L"仮定形,れ,768",
              L"命令ｅ,れ,784", L"仮定縮約１,りゃ,770", L"体言接続特殊,ん,774" }
        },

        // 五段・ワ行促音便
        { L"五段・ワ行促音便",
            { L"基本形,う,817", L"未然形,わ,823", L"未然ウ接続,お,820", L"連用形,い,832", L"連用タ接続,っ,829", L"仮定形,え,814", L"命令ｅ,え,826" }
        },

        // 一段
        { L"一段",
            { L"基本形,る,619", L"未然形,,622", L"未然ウ接続,よ,621", L"連用形,,625", L"仮定形,れ,617", L"命令ｙｏ,よ,624", L"命令ｒｏ,ろ,623", L"仮定縮約１,りゃ,618", L"体言接続特殊,ん,620" }
        },

        // カ変・来ル
        { L"カ変・来ル",
            { L"基本形,る,573", L"未然形,,577", L"未然ウ接続,よ,576", L"連用形,,580", L"仮定形,れ,571", L"命令ｙｏ,よ,579", L"命令ｉ,い,578",
              L"仮定縮約１,りゃ,572", L"体言接続特殊,ん,574", L"体言接続特殊２,,575" }
        },

        // カ変・クル
        { L"カ変・クル",
            { L"基本形,くる,563", L"未然形,こ,567", L"未然ウ接続,こよ,566", L"連用形,き,570", L"仮定形,くれ,561", L"命令ｙｏ,こよ,569", L"命令ｉ,こい,568",
              L"仮定縮約１,くりゃ,562", L"体言接続特殊,くん,564", L"体言接続特殊２,く,565" }
        },

        // サ変・−スル
        { L"サ変・−スル",
            { L"基本形,する,583", L"文語基本形,す,584", L"未然形,し,587", L"未然ウ接続,しよ,585", L"未然ウ接続,しょ,585", L"未然レル接続,せ,586", L"仮定形,すれ,581",
              L"命令ｙｏ,せよ,589", L"命令ｒｏ,しろ,588", L"仮定縮約１,すりゃ,582" }
        },

        // サ変・−ズル
        { L"サ変・−ズル",
            { L"基本形,ずる,592", L"文語基本形,ず,593", L"未然形,ぜ,595", L"未然ウ接続,ぜよ,594", L"仮定形,ずれ,590", L"命令ｙｏ,ぜよ,596", L"仮定縮約１,ずりゃ,591"}
        },

        // 形容詞・イ段
        { L"形容詞・イ段",
            { L"基本形,い,43", L"文語基本形,,45", L"未然ヌ接,から,47", L"未然ウ接続,かろ,46", L"連用タ接続,かっ,50", L"連用テ接続,く,51", L"連用テ接続,くっ,51", L"連用ゴザイ接続,ゅう,49",
              L"体言接続,き,44", L"仮定形,けれ,40", L"命令ｅ,かれ,48", L"仮定縮約１,けりゃ,41", L"仮定縮約２,きゃ,42", L"ガル接続,,39" }
        },

        // 形容詞・アウオ段
        { L"形容詞・アウオ段",
            { L"基本形,い,19", L"文語基本形,し,23", L"未然ヌ接,から,27", L"未然ウ接続,かろ,25", L"連用タ接続,かっ,33", L"連用テ接続,く,35", L"連用テ接続,くっ,35", L"連用ゴザイ接続,う,31",
              L"体言接続,き,21", L"仮定形,けれ,13", L"命令ｅ,かれ,29", L"仮定縮約１,けりゃ,15", L"仮定縮約２,きゃ,17", L"ガル接続,,11" }
        },

    };

    int getGobiLength(StringRef ktype) {
        if (ktype.starts_with(L"サ変") || ktype.starts_with(L"カ変・クル")) {
            return 2;
        } else {
            return 1;
        }
    }

    // 活用形の展開
    // line は余分な空白や先頭の * が削除されている
    VectorString expandKForm(StringRef line) {
        LOG_DEBUGH(L"ENTER: line={}", line);

        VectorString expandedList;

        VectorString items = utils::split(line, L",");
        LOG_DEBUGH(L"items={}", utils::join(items, L", "));
        if (items.size() > 11) {
            StringRef surf = items[0];
            //LOG_DEBUGH(L"push_back({})", utils::join(items, L", "));
            //expandedList.push_back(utils::join(items, ','));

            StringRef ktype = items[8];
            StringRef yomi = items[11];
            if (surf.size() > 1 && yomi.size() > 1) {
                int gobiLen = getGobiLength(ktype);
                String stem = utils::safe_substr(surf, 0, -gobiLen);
                String yomiStem = utils::safe_substr(yomi, 0, -gobiLen);
                LOG_DEBUGH(L"gobiLen={}, stem={}, yomiStem={}", gobiLen, stem, yomiStem);

                String ktype = items[8];
                auto defs = kformDefs.find(ktype);
                if (defs != kformDefs.end()) {
                    for (const auto& val : defs->second) {
                        auto kfs = utils::split(val, ',');
                        StringRef kform = kfs[0];
                        //LOG_DEBUGH(L"kfs={}", utils::join(kfs, L", "));
                        String gobi = kfs.size() > 1 ? kfs[1] : L"";
                        String conn = kfs.size() > 2 ? kfs[2] : items[1];
                        items[0] = stem + gobi;     // 表層形
                        items[1] = conn;            // 左接続
                        items[2] = conn;            // 右接続
                        items[9] = kform;           // 活用形
                        items[11] = yomiStem + gobi;  // 読み
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
            String mazeStem = utils::safe_substr(mazeYomi, 0, -getGobiLength(ktype));
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
    bool main(MapString& results, MapInt& costs, StringRef line, bool doMaze) {
        LOG_DEBUGH(L"\nENTER: doMaze={}, line={}", doMaze, line);

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

        if (doMaze) {
            //const auto surfParts = _reMatchScan(surf, L"^([^一-龠々]*)([一-龠々])([^一-龠々]*)([一-龠々])(?:([^一-龠々]*)([一-龠々])(?:([^一-龠々]*)([一-龠々]))?)?([ぁ-ゖ]*)$");
            const auto surfParts = _reMatchScan(surf, L"^([ぁ-ゖ]*)([一-龠々])([ぁ-ゖ]*)([一-龠々ァ-ヶ])(?:([ぁ-ゖ]*)([一-龠々ァ-ヶ])(?:([ぁ-ゖ]*)([一-龠々]))?)?([ぁ-ゖ]*)$");
            if (surfParts.size() == 9 &&
                (surfParts[3].empty() || !utils::is_katakana(surfParts[3][0]) || (!surfParts[5].empty() && !utils::is_katakana(surfParts[5][0])))) {
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

        LOG_DEBUGH(L"LEAVE");
        return true;
    }

    MapString linesMap;
    MapInt costsMap;

    bool mazeExpandFlag = true;

    // 活用型と交ぜ書きの前処理を行う
    // 辞書として有効な行の場合は true を返す。
    // (標準辞書の形式に従っていないものはそのまま出力)
    bool expandMazegaki(StringRef origLine, Vector<String>& errorLines, bool bUserDic) {
        LOG_DEBUGH(L"ENTER: origLine={}", origLine);

        //items = origLine.strip.split(',')
        String line = utils::reReplace(utils::strip(origLine), L"\\s*,\\s*", L",");
        if (line.empty()) {
            LOG_DEBUGH(L"LEAVE: false: empty");
            return false;
        }

        if (line[0] == '#') {
            String lowerLine = utils::toLower(line);
            if (lowerLine.starts_with(L"#unexpand")) {
                mazeExpandFlag = false;
                LOG_DEBUGH(L"LEAVE: false: unexpand mazegaki");
                return false;
            }
            if (lowerLine.starts_with(L"#expand")) {
                mazeExpandFlag = true;
                LOG_DEBUGH(L"LEAVE: false: expand mazegaki");
                return false;
            }
            LOG_DEBUGH(L"LEAVE: false: comment or ");
            return false;
        }

        bool doMaze = mazeExpandFlag;
        bool kFormExpandFlag = bUserDic;
        wchar_t firstChar = line[0];
        if (firstChar == '*' || firstChar == '+' || firstChar == '-') {
            doMaze = firstChar == '*';          // 交ぜ書き処理フラグ ('+' か '-' なら交ぜ書き処理を行わない)
            kFormExpandFlag = firstChar != '-'; // 活用形の展開フラグ ('*' か '+' なら展開、'-' なら展開しない)
            line = line.substr(1);
        }
        if (line.empty()) {
            errorLines.push_back(origLine);
            LOG_DEBUGH(L"LEAVE: false: empty");
            return false;
        }

        if (kFormExpandFlag || bUserDic) {
            // 活用形の展開(or ユーザー辞書形式)の場合、標準形に変換
            line = convertToNormalForm(line);
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
            // 標準辞書の形式に従っていないものはそのまま出力
            linesMap[origLine] = origLine;
            return true;
        }

        if (items[9] == L"体言接続特殊２") {
            // 活用形が体言接続特殊２のものは削除
            LOG_DEBUGH(L"LEAVE: false: tokushu2");
            return false;
        }

        bool mainResult = false;
        if (kFormExpandFlag) {
            // 活用形の展開
            for (const auto& expLine : expandKForm(line)) {
                mainResult = main(linesMap, costsMap, expLine, doMaze);
            }
        } else {
            mainResult = main(linesMap, costsMap, line, doMaze);
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
    Vector<String> preprocessMazegakiDic(StringRef mazeResrcDir, StringRef dicSrcPath, bool bUserDic) {
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
            if (MazegakiPreprocessor::expandMazegaki(line, errorLines, bUserDic)) {
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

