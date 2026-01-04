#include "std_utils.h"
#include "string_utils.h"
#include "file_utils.h"
#include "path_utils.h"
#include "transform_utils.h"
#include "constants/Constants.h"
#include "CharProperty.h"
#include "Logger.h"
#include "util/exception.h"

using namespace util;

namespace analyzer
{
    DEFINE_NAMESPACE_LOGGER(analyzer);
    DEFINE_CLASS_LOGGER(CharProperty);

    /**
     * 未知語定義ファイルに記述されているカテゴリー名がすべて定義されていることを確認する
     */
    void checkUnknownDef(
        const String& ufile,
        std::map<String, PVT> categoryMap,
        StringRef cfile)
    {
        std::vector<String> unkSettings;

        auto trim_and_filter = [&unkSettings](const std::vector<String>& lines) mutable {
            utils::copy_if(
                utils::transform(lines, [](StringRef s) {return utils::strip(s);}),
                unkSettings,
                [](StringRef s) {return !s.empty() && s[0] != '#';});
        };

        utils::IfstreamReader reader(ufile);
        if (reader.success()) {
            // ファイル読み込み
            trim_and_filter(reader.getAllLines());
        } else {
            LOG_WARN(_T("{} is not found. minimum setting is used"), ufile);
            trim_and_filter(utils::split(UNK_DEF_DEFAULT, '\n'));
        }

        std::set<String> unk;
        for (StringRef line : unkSettings) {
            //val items = CSVParser.parse(line);
            auto items = utils::split(line, ',');
            CHECK_OR_THROW(items.size() >= 1, L"format error: {}", line);
            StringRef key = items[0];
            CHECK_OR_THROW(utils::safe_get(categoryMap, key) != 0, L"category [{}] is undefined in {}", key, cfile);
            unk.insert(key);
        }

        for (const auto& pair : categoryMap) {
            CHECK_OR_THROW(unk.find(pair.first) != unk.end(), L"category [{}] is undefined in ", pair.first, ufile);
        }
    }

    /**
     * Compile char property
     * @param cfile 文字種定義ファイル (e.g. char.def)
     * @param ufile 未知語定義ファイル (e.g. unk.def)
     * @param ofile 出力ファイル (e.g. char.bin)
     */
    UniqPtr<CategoryTable> CharProperty::compile(StringRef cfile, StringRef ufile, StringRef ofile) {
        std::vector<CharRange> charRanges;
        std::map<String, PVT> cat_map;
        std::vector<String> cat_ary;

        // 文字種定義の読み込み
        std::vector<String> charSettings;
        utils::IfstreamReader reader(cfile);
        if (reader.success()) {
            // ファイル読み込み
            reader.getAllLines(charSettings);
        } else {
            LOG_WARN(_T("{} is not found. minimum setting is used"), cfile);
            charSettings = utils::split(CHAR_PROPERTY_DEF_DEFAULT, '\n');
        }

        // 16進数による範囲表現解析用の正規表現
        std::wregex reHexRange(_T("^0x([0-9A-Fa-f]{1,4})(\\.\\.0x([0-9A-Fa-f]{1,4}))?$"));
        std::wregex reComment(_T("\\s*#.*"));
        std::wregex reDelim(_T("\\s+"));

        // 文字種定義のコンパイル
        for (StringRef line : charSettings) {
            String ln = std::regex_replace(line, reComment, _T(""));
            if (!ln.empty()) {
                auto items = utils::reSplit(ln, reDelim);
                CHECK_OR_THROW(items.size() >= 2, L"format error: {}", line);

                std::wsmatch m;

                if (std::regex_match(items[0], m, reHexRange)) {
                    //case reHexRange(lx,_,hx) = > 
                    // 16進数 ⇒ 文字(範囲) に対するカテゴリー（文字種）付与
                    int low = utils::strToHex(m[1]);
                    int high = utils::strToHex(m[3], low);
                    CHECK_OR_THROW((low >= 0 && low < MAX_CHAR&& high >= 0 && high < MAX_CHAR&& low <= high),
                        L"range error: low={} high={}", low, high);

                    // カテゴリーは複数指定できる
                    Vector<String> cats;
                    cats.push_back(items[1]);
                    for (size_t i = 2; i < items.size(); ++i) {
                        StringRef c = items[i];
                        if (c.empty() || c[0] == '#') break;
                        CHECK_OR_THROW(cat_map.find(c) != cat_map.end(), L"category [{}] is undefined", c);
                        cats.push_back(c);
                    }
                    charRanges.push_back(CharRange{ low, high, cats });
                } else {
                    // 非16進数 ⇒ カテゴリーの定義
                    CHECK_OR_THROW(items.size() >= 4, L"format error: {}", line);

                    auto catName = items[0];  // 主カテゴリー名
                    CHECK_OR_THROW(cat_map.find(catName) == cat_map.end(), L"category {} is already defined", catName);

                    // ct:charType, items(1):invoke, items(2):group, items(3):length, id:primaryType
                    int id = (int)cat_ary.size();  // 主カテゴリーID
                    auto ct = CharInfo::typeToBitflag(id);
                    auto cv = CharInfo::apply(ct, utils::strToInt(items[1]), utils::strToInt(items[2]), utils::strToInt(items[3]), id);
                    cat_map[catName] = cv;
                    cat_ary.push_back(catName);
                }
            }
        }

        //val categoryMap = cat_map.toMap
        CHECK_OR_THROW(cat_map.size() < 18, L"too many categories(>= 18)");
        CHECK_OR_THROW(cat_map.find(L"DEFAULT") != cat_map.end(), L"category [DEFAULT] is undefined");
        CHECK_OR_THROW(cat_map.find(L"SPACE") != cat_map.end(), L"category [SPACE] is undefined");
        // 未知語定義ファイルに記述されているカテゴリー名がすべて char.def で定義されていることを確認する
        checkUnknownDef(ufile, cat_map, cfile);

        auto ciValue = [cat_map](StringRef cat) {
            auto iter = cat_map.find(cat);
            if (iter != cat_map.end()) {
                return iter->second;
            } else {
                THROW_RTE(L"category [{}] is undefined", cat);
                return (PVT)0;   // dummy
            }
        };

        // カテゴリー名列から CharInfo を作成
        auto convertToCharInfo = [ciValue](const std::vector<String>& cats) {
            if (cats.empty()) {
                LOG_ERROR(L"category size is empty");
                CHECK_OR_THROW(!cats.empty(), L"category size is empty");
            }

            PVT base = ciValue(cats[0]);
            for (size_t i = 1; i < cats.size(); ++i) {
                base = CharInfo::addCharType(base, CharInfo::primaryBitflag(ciValue(cats[i])));
            }
            return base;
        };

        std::vector<PVT> table(MAX_CHAR + 1, ciValue(L"DEFAULT"));

        for (const CharRange& r : charRanges) {
            auto ci = convertToCharInfo(r.cats);
            for (int i = r.low; i <= r.high; ++i) {
                table[i] = ci;
            }
        }

        // output binary table
        UniqPtr<CategoryTable> catMapTbl = MakeUniq<CategoryTable>();
        catMapTbl->setTables(cat_map, cat_ary, table);
        catMapTbl->serialize(ofile);
        return catMapTbl;
    }

    /**
     * Read binary category table
     */
    bool CharProperty::loadBinary(OptHandlerPtr opts) {
        return loadBinary(utils::joinPath(opts->getString(L"dicdir"), CHAR_PROPERTY_FILE));
    }

    /**
     * Read binary category table
     */
    bool CharProperty::loadBinary(StringRef path) {
        LOG_INFOH(L"ENTER: path={}", path);
        if (path.empty()) loadBinary(CHAR_PROPERTY_FILE); // (join_path(opts.get("dicdir"), CHAR_PROPERTY_FILE));

        _catMapTbl.deserialize(path);

        LOG_INFOH(L"LEAVE: true");
        return true;
    }

    // read compiled binary
    void CategoryTable::deserialize(StringRef path) {
        LOG_INFOH(L"ENTER: CategoryTable::deserialize: path={}", path);
        utils::IfstreamReader reader(path, true);
        CHECK_OR_THROW(reader.success(), L"can't open category table file for read: {}", path);

        reader.read(catMap);
        reader.read(catAry);
        reader.read(table);
        LOG_INFOH(L"LEAVE: CategoryTable::deserialize");
    }

    // write compiled binary
    void CategoryTable::serialize(StringRef path) {
        LOG_INFOH(L"ENTER: CategoryTable::serialize: path={}", path);
        utils::OfstreamWriter writer(path, true, false);
        CHECK_OR_THROW(writer.success(), L"can't open category table file for write: {}", path);

        writer.write(catMap);
        writer.write(catAry);
        writer.write(table);
        LOG_INFOH(L"LEAVE: CategoryTable::serialize");
    }

} // namespace analyzer