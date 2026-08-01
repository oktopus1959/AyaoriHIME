#include "Logger.h"
#include "string_type.h"
#include "string_utils.h"
#include "file_utils.h"
#include "path_utils.h"

#include "Constants.h"
#include "Settings.h"
#include "ErrorHandler.h"
#include "OutputStack.h"
#include "SimpleDictionary.h"
#include "EasyChars.h"
#include "StrokeHelp.h"
#include "RomanToKatakana.h"

#if 0
#undef _DEBUG_SENT
#undef LOG_DEBUG
#undef LOG_DEBUGH
#undef _LOG_DEBUGH
#undef _LOG_DEBUGH_COND
#define _DEBUG_SENT(x) x
#define LOG_SAVE_DICT LOG_WARNH
#define LOG_DEBUG LOG_INFOH
#define LOG_DEBUGH LOG_INFOH
#define _LOG_DEBUGH LOG_INFOH
#define _LOG_DEBUGH_COND LOG_INFOH_COND
#else
#define LOG_SAVE_DICT LOG_INFOH
#endif

namespace {
    // -------------------------------------------------------------------
    typedef size_t HashVal;

    // -------------------------------------------------------------------
    // ハッシュ値から文字列集合へのマップ
    class HashToStrMap {
        std::map<HashVal, std::set<MString>> dic;

        std::set<MString> emptySet;

    public:
        // 全エントリーをクリア
        void Clear() {
            dic.clear();
        }

        // 文字列(単語)の追加
        void Insert(const MString& s) {
            auto hsh = utils::get_hash(s);
            auto iter = dic.find(hsh);
            if (iter == dic.end()) {
                dic[hsh] = utils::make_one_element_set(s);
            } else {
                iter->second.insert(s);
            }
        }

        // 単語の削除
        void Remove(const MString& s) {
            auto iter = dic.find(utils::get_hash(s));
            if (iter != dic.end()) {
                iter->second.erase(s);
            }
        }

        // 単語は既に登録済みか
        bool FindWord(const MString& s) const {
            auto iter = dic.find(utils::get_hash(s));
            return (iter != dic.end() && iter->second.find(s) != iter->second.end());
        }

        // 指定されたハッシュ値を持つ単語の集合を取得する
        const std::set<MString>& GetSet(HashVal hsh) {
            auto iter = dic.find(hsh);
            return iter != dic.end() ? iter->second : emptySet;
        }

        const std::set<MString> GetAllWords() {
            std::set<MString> result;
            for (const auto& pr : dic) {
                utils::apply_union(result, pr.second);
            }
            return result;
        }

        //bool IsEmpty() const {
        //    return dic.empty();
        //}
    };

    // インスタンス
    HashToStrMap hashToStrMap;

    // -------------------------------------------------------------------
    // 単語中の文字から、それを含む文字列のハッシュ値集合へのマップ
    class SimpleCharDic {
        std::map<mchar_t, std::set<HashVal>> dic;

        void insert(mchar_t mch, size_t hsh) {
            auto iter = dic.find(mch);
            if (iter == dic.end()) {
                dic[mch] = utils::make_one_element_set(hsh);
            } else {
                iter->second.insert(hsh);
            }
        }

        void getSet(std::set<MString>& result, mchar_t mch) {
            auto iter = dic.find(mch);
            if (iter != dic.end()) {
                for (auto hsh : iter->second) {
                    const std::set<MString>& set_ = hashToStrMap.GetSet(hsh);
                    //if (!set_.empty()) result.insert(set_.begin(), set_.end());
                    for (const MString& w : set_) {
                        // '||' を '|' に置換しておく
                        result.insert(utils::replace(w, MSTR_VERT_BAR_2, MSTR_VERT_BAR));
                    }
                }
            }
        }

    public:
        void Insert(mchar_t mch, const MString& s) {
            if (!hashToStrMap.FindWord(s)) {
                insert(mch, utils::get_hash(s));
            }
        }

        std::set<MString> GetSetByMchar(mchar_t mch) {
            std::set<MString> result;
            getSet(result, mch);
            return result;
        }
    };

    // 単語の先頭4文字を含むハッシュ値集合へのマップ
    class Simple4CharsDic {
        DECLARE_CLASS_LOGGER;

        // 0～3文字目に指定文字を含む文字列ハッシュ集合のリスト
        std::vector<SimpleCharDic> simpleCharDics;

    public:
        Simple4CharsDic() {
            simpleCharDics.resize(4);
        }

        void Insert(const MString& word) {
            LOG_DEBUG(_T("ENTER: word={}"), to_wstr(word));
            for (size_t i = 0; i < simpleCharDics.size() && i < word.size(); ++i) {
                LOG_DEBUG(_T("simpleCharDics[{}].Insert({}, {})"), i, (wchar_t)word[i], to_wstr(word));
                simpleCharDics[i].Insert(word[i], word);
            }
            LOG_DEBUG(_T("LEAVE"));
        }

        // key の末尾n文字にマッチする文字列集合を取得する('?' も考慮, ただし少なくとも1文字は'?'以外を含む)
        std::set<MString> GetSet(const MString& key, size_t n) {
            _LOG_DEBUGH(_T("ENTER: key={}, n={}"), to_wstr(key), n);
            std::set<MString> result;
            size_t start = n >= key.size() ? 0 : key.size() - n;
            size_t nkey = key.size() - start;
            std::vector<size_t> quesPoses;  // '?' の位置
            for (size_t i = 0; i < simpleCharDics.size() && i < nkey; ++i) {
                auto mch = key[start + i];
                if (mch == '?') {
                    _LOG_DEBUGH(_T("'?' found in key={}: start={}, i={}"), to_wstr(key), start, i);
                    quesPoses.push_back(i);
                    // '?' なら全部にマッチするとみなし、長さだけをチェック
                    if (i > 0 && !result.empty()) {
                        std::set<MString> newResult;
                        for (auto w : result) {
                            if (w.size() > i) newResult.insert(w);
                        }
                        if (newResult.empty()) {
                            result.clear();
                            break;
                        }
                        result = newResult;
                    }
                    continue; 
                }
                if (result.empty()) {
                    result = simpleCharDics[i].GetSetByMchar(mch);
                } else {
                    utils::apply_intersection(result, simpleCharDics[i].GetSetByMchar(mch));
                }
                if (result.empty()) break;
            }
            _LOG_DEBUGH(_T("result.size={}"), result.size());
            if (!quesPoses.empty()) {
                // '?' があった
                _LOG_DEBUGH(_T("'?' pos={}, {}, {}"), quesPoses.size() > 0 ? quesPoses[0] : -1, quesPoses.size() > 1 ? quesPoses[1] : -1, quesPoses.size() > 2 ? quesPoses[2] : -1);
                std::set<MString> newResult;
                for (auto w : result) {
                    _LOG_DEBUGH(_T("CHECK: w={}"), to_wstr(w));
                    size_t vbarPos = w.find_first_of(VERT_BAR);
                    bool bFound = true;
                    for (auto i : quesPoses) {
                        if (i >= vbarPos || i >= w.size() || utils::is_hiragana(w[i])) {
                            bFound = false;
                            break;
                        }
                    }
                    if (bFound) {
                        _LOG_DEBUGH(_T("'?' match: w={}"), to_wstr(w));
                        newResult.insert(w);
                    }
                }
                if (newResult.empty()) {
                    result.clear();
                } else {
                    result = newResult;
                }
                _LOG_DEBUGH(_T("'?' found: result.size={}"), result.size());
            }
            _LOG_DEBUGH(_T("LEAVE: result.size={}"), result.size());
            return result;
        }

        // '*' をはさんで、前半部の key1 と後半部の key2 にマッチする文字列集合を取得。key1のうちマッチした部分の長さを返す
        size_t FindMatchingWords(const MString& key1, const MString& key2, std::set<MString>& result) {
            std::set<MString> temp_set;
            auto key0 = utils::last_substr(key1, simpleCharDics.size());    // 前半キーの末尾4文字(以下)だけをキーとする
            result.clear();
            size_t start = 0;
            while (start < key0.size()) {
                for (size_t i = 0; i < key0.size() - start; ++i) {
                    auto mch = key0[start + i];
                    if (mch == '?') continue; // '?' なら全部にマッチするとみなす
                    if (temp_set.empty())
                        temp_set = simpleCharDics[i].GetSetByMchar(mch);
                    else
                        utils::apply_intersection(temp_set, simpleCharDics[i].GetSetByMchar(mch));
                    if (temp_set.empty()) break;
                }
                if (!temp_set.empty()) {
                    result = utils::filter(temp_set, [key2](const MString& w) {return utils::endsWithWildKey(w, key2);});
                    if (!result.empty()) {
                        size_t key_size = key0.size() - start;
                        _LOG_DEBUGH(_T("result.size={}, keyMatchLen={}"), result.size(), key_size);
                        return key_size;
                    }
                }
                ++start;
            }
            _LOG_DEBUGH(_T("result.size=0, keyMatchLen=0"));
            return 0;
        }
    };
    DEFINE_CLASS_LOGGER(Simple4CharsDic);

    // -------------------------------------------------------------------
    // 使用された順に並べたリスト
    class SimpleDicUsedList {
        DECLARE_CLASS_LOGGER;

        const size_t MAX_SIZE = 10000;
        const size_t EXTRA_SIZE = 1000;

        std::vector<MString> usedList;

        bool bDirty = false;

    public:
        // UTF8で書かれた辞書ソースを読み込む
        void ReadFile(const std::vector<String>& lines) {
            LOG_INFOH(_T("ENTER: {} lines"), lines.size());
            std::set<String> used;
            for (const auto& w : lines) {
                if (!utils::contains(used, w)) {
                    usedList.push_back(to_mstr(w));
                    used.insert(w);
                    if (usedList.size() >= MAX_SIZE) break;
                }
            }
            bDirty = false;
            LOG_INFOH(_T("LEAVE"));
        }

        void PushEntry(const MString& word, size_t minlen = 2) {
            _LOG_DEBUGH(_T("CALLED: word={}, minlen={}"), to_wstr(word), minlen);
            if (word.size() >= minlen) {
                if (!usedList.empty()) {
                    if (usedList[0] == word) return;
                    utils::erase(usedList, word);
                }
                usedList.insert(usedList.begin(), word);
                if (usedList.size() >= MAX_SIZE + EXTRA_SIZE) {
                    _LOG_DEBUGH(_T("EXTRA entries erasing...: size={}"), usedList.size());
                    usedList.erase(usedList.begin() + MAX_SIZE, usedList.end());
                    _LOG_DEBUGH(_T("EXTRA entries erased: size={}"), usedList.size());
                }
                bDirty = true;
            }
        }

        // set_ および usedList に含まれるものから下記を満たすものを outvec に返す
        // outvec_ に格納されたものは set_ から取り除く
        // keylen = キー長
        // ・単語長が wlen に一致する
        // ・wlen == 0 なら単語長 >= 2
        // ・wlen >= 9 なら単語長 >= 9
        // ・ただし、キーが1文字(keylen==1)なら、候補列から1文字単語は除く
        void ExtractUsedWords(const MString& key, SImpleDicResultList& outvec, std::set<MString>& set_, size_t wlen = 0) {
            LOG_DEBUG(_T("CALLED: key={}, wlen={}"), to_wstr(key), wlen);
            size_t keylen = key.size();
            _DEBUG_SENT(size_t n = 0);
            for (const auto& w : usedList) {
                //_DEBUG_SENT(if (w.find(VERT_BAR) != MString::npos) _LOG_DEBUGH(_T("VERT_BAR: {}"), to_wstr(w)));
                if ((w.size() == wlen || (wlen == 0 && w.size() >= 2) || (wlen >= 9 && w.size() > 9)) && w != key && utils::contains(set_, w)) {
                    if (keylen != 1 || w.size() >= 2) {
                        // キーが1文字なら、候補列から1文字単語は除く
                        _DEBUG_SENT(\
                            if (n < 10) { \
                                _LOG_DEBUGH(_T("outvec.PushEntry(key={}, w={})"), to_wstr(key), to_wstr(w)); \
                            } else if (n == 10) { \
                                _LOG_DEBUGH(_T("and {} entries..."), usedList.size() - 10); \
                            }\
                            ++n);
                        outvec.PushEntry(key, w);
                        set_.erase(w);
                    }
                }
            }
        }

        // 前半部が key に完全一致する変換候補だけを、最近使用した順で取得する
        void ExtractExactUsedMapWords(const MString& key, SImpleDicResultList& outvec, std::set<MString>& set_, size_t wlen = 0) {
            LOG_DEBUG(_T("CALLED: key={}, wlen={}"), to_wstr(key), wlen);
            size_t keylen = key.size();
            std::map<MString, MString> exactEntries;
            for (const auto& s : set_) {
                if (s.size() > keylen && s[keylen] == VERT_BAR) {
                    // 読み取り専用辞書の内部表現「||」を、使用履歴と同じ「|」にそろえる
                    exactEntries.emplace(utils::replace(s, MSTR_VERT_BAR_2, MSTR_VERT_BAR), s);
                }
            }

            for (const auto& usedWord : usedList) {
                auto iter = exactEntries.find(usedWord);
                if (iter != exactEntries.end()) {
                    const auto& entry = iter->second;
                    if ((wlen > 0 && entry.size() == wlen) || (wlen == 0 && (keylen != 1 || entry.size() >= 2))) {
                        outvec.PushEntry(key, entry);
                    }
                    set_.erase(entry);
                    exactEntries.erase(iter);
                    if (exactEntries.empty()) break;
                }
            }
        }

        // 辞書内容の書き込み
        void WriteFile(utils::OfstreamWriter& writer) {
            LOG_DEBUGH(_T("CALLED"));
            std::set<MString> used;
            for (const auto& word : usedList) {
                if (!utils::contains(used, word)) {
                    writer.writeLine(utils::utf8_encode(to_wstr(word)));
                    used.insert(word);
                }
            }
            bDirty = false;
        }

        //// 辞書が空か
        //bool IsEmpty() const {
        //    return usedList.empty();
        //}

        // 辞書が更新されているか
        bool IsDirty() const {
            return bDirty;
        }

    };
    DEFINE_CLASS_LOGGER(SimpleDicUsedList);

    // -------------------------------------------------------------------
    // 複数候補があるケースで先頭候補を並べたリスト
    class SimpleDicHeadCandList {
        DECLARE_CLASS_LOGGER;

        const size_t MAX_SIZE = 10000;
        const size_t EXTRA_SIZE = 1000;

        std::vector<MString> headCandList;

    public:
        void PushEntry(const MString& word) {
            //_LOG_DEBUGH(_T("CALLED: word={}"), to_wstr(word));
            headCandList.push_back(word);
        }

        // 指定キーの優先エントリを置換する。未登録なら追加する
        void ReplaceEntry(const MString& key, const MString& word) {
            bool found = false;
            for (auto& entry : headCandList) {
                if (entry.size() > key.size() && entry.compare(0, key.size(), key) == 0 && entry[key.size()] == VERT_BAR) {
                    entry = word;
                    found = true;
                }
            }
            if (!found) PushEntry(word);
        }

        void ExtractHeadWord(const MString& key, SImpleDicResultList& outvec, std::set<MString>& set_) {
            LOG_DEBUG(_T("CALLED: key={}"), to_wstr(key));
            _DEBUG_SENT(size_t n = 0);
            for (const auto& w : headCandList) {
                if (w.size() > key.size() && w[key.size()] == '|' && utils::contains(set_, w)) {
                    // キーが1文字なら、候補列から1文字単語は除く
                    _DEBUG_SENT(\
                        if (n < 10) { \
                            _LOG_DEBUGH(_T("outvec.PushEntry(key={}, w={})"), to_wstr(key), to_wstr(w)); \
                        } else if (n == 10) { \
                            _LOG_DEBUGH(_T("and {} entries..."), headCandList.size() - 10); \
                        }\
                        ++n);
                    outvec.PushEntry(key, w);
                    set_.erase(w);
                }
            }
        }

    };
    DEFINE_CLASS_LOGGER(SimpleDicHeadCandList);

    // -------------------------------------------------------------------
    // 履歴辞書の実装クラス
    class SimpleDictionaryImpl : public SimpleDictionary {
    private:
        DECLARE_CLASS_LOGGER;
        // 0～3文字目に指定文字を含む文字列ハッシュ集合のリスト
        Simple4CharsDic simple4CharsDic;

        SimpleDicUsedList usedList;

        // 変換形の優先順リスト
        SimpleDicHeadCandList translatePriorityList;

        // 先頭優先辞書に出現した変換キー
        std::set<String> firstPreferredKeys;

        // 結果を保持しておくリスト
        //std::vector<SimpleDicResult> resultList;
        SImpleDicResultList resultList;

    private:
        // 一行の辞書ソース文字列を解析して辞書に登録する
        bool addOneEntry(const MString& line, size_t minlen = 2, bool bForce = false) {
            LOG_DEBUG(_T("ENTER: line={}, minlen={}"), to_wstr(line), minlen);
            auto word = utils::strip(line);
            LOG_DEBUG(_T("word={}"), to_wstr(word));
            // 空白行または1文字以下、あるいは強制登録でなくて、先頭が '#' or ';' の場合は、何もしない
            if (word.size() < minlen || (!bForce && (word[0] == '#' || word[0] == ';'))) {
                LOG_DEBUG(_T("LEAVE: false"));
                return false;
            }

            if (!hashToStrMap.FindWord(word)) {
                //simpleDic1.Insert(word);
                //simpleDic2.Insert(word);
                //simpleDic3.Insert(word);
                //simpleDic4.Insert(word);
                simple4CharsDic.Insert(word);
                hashToStrMap.Insert(word);
            }
            LOG_DEBUG(_T("LEAVE: true"));
            return true;
        }

        // UTF8で書かれた辞書ソースを読み込む
        void readFile(const std::vector<String>& lines, bool bReadOnly) {
            LOG_INFOH(_T("ENTER: {} lines, bReadOnly={}"), lines.size(), bReadOnly);
            int logLevel = Reporting::Logger::LogLevel();
            if (lines.size() > 10) Reporting::Logger::SetLogLevel(0);
            for (const auto& line : lines) {
                if (!line.empty() && line[0] != '#') {
                    if (bReadOnly && line.find(_T("||")) == String::npos) {
                        addOneEntry(to_mstr(utils::replace(line, _T("|"), _T("||"))), 1);
                    } else {
                        addOneEntry(to_mstr(line), 1);
                    }
                }
            }
            Reporting::Logger::SetLogLevel(logLevel);
            LOG_INFOH(_T("LEAVE"));
        }

    public:
        SimpleDictionaryImpl() {
            LOG_INFOH(_T("ctor"));
        }

        //// UTF8で書かれた辞書ソースを読み込む
        //void ReadFile(const std::vector<String>& lines) override {
        //    readFile(lines, false);
        //}

        // 辞書ソースを読み込む (同じ変換キーを持つエントリの先頭を translatePriorityList に追加)
        void ReadDicFileAsReadOnlyFirstPreferred(const std::vector<String>& lines) override {
            LOG_INFOH(_T("ENTER: {} lines"), lines.size());
            readFile(lines, true);

            // 同じ変換キーを持つエントリの先頭を translatePriorityList に追加
            std::map<String, MString> firstEntries;
            std::set<String> priorityKeys;
            size_t count = 0;
            for (const auto& line : lines) {
                if (line.empty() || line[0] == '#') continue;
                size_t pos = line.find(_T("|"));
                if (pos < line.size()) {
                    String word = line.substr(0, pos);
                    firstPreferredKeys.insert(word);
                    auto [iter, inserted] = firstEntries.emplace(word, to_mstr(line));
                    if (!inserted && priorityKeys.insert(word).second) {
                        translatePriorityList.PushEntry(iter->second);
                        ++count;
                    }
                }
            }
            LOG_INFOH(_T("LEAVE: duplicate key count={}"), count);
        }

        // 辞書ソースを読み込む (同じ変換キーを持つエントリの末尾を translatePriorityList に追加)
        void ReadDicFileAsReadOnlyLastPreferred(const std::vector<String>& lines) override {
            LOG_INFOH(_T("ENTER: {} lines"), lines.size());
            readFile(lines, true);

            // 重複キーまたは先頭優先辞書にもあるキーの末尾エントリを translatePriorityList に登録
            std::map<String, MString> lastEntries;
            std::set<String> priorityKeys;
            for (const auto& line : lines) {
                if (line.empty() || line[0] == '#') continue;
                size_t pos = line.find(_T("|"));
                if (pos < line.size()) {
                    String word = line.substr(0, pos);
                    auto [iter, inserted] = lastEntries.emplace(word, to_mstr(line));
                    if (!inserted) {
                        iter->second = to_mstr(line);
                        priorityKeys.insert(word);
                    }
                    if (firstPreferredKeys.contains(word)) priorityKeys.insert(word);
                }
            }
            for (const auto& word : priorityKeys) {
                translatePriorityList.ReplaceEntry(to_mstr(word), lastEntries.at(word));
            }
            LOG_INFOH(_T("LEAVE: priority key count={}"), priorityKeys.size());
        }

    private:
        void pushCandidate(const MString& key, const MString& s, size_t& n) {
            _DEBUG_SENT(if (n < 10) _LOG_DEBUGH(_T("resultList.PushEntry(key={}, s={})"), to_wstr(key), to_wstr(s)));
            resultList.PushEntry(key, utils::replace_all(s, '\t', '|'));
            ++n;
        }

        std::vector<MString> splitByCapitalLetter(const MString& key) {
            std::vector<MString> list;
            if (!key.empty()) {
                size_t startPos = 0;
                size_t pos = 1;
                while (pos < key.size()) {
                    if (is_upper_alphabet(key[pos])) {
                        list.push_back(utils::safe_substr(key, startPos, (int)(pos - startPos)));
                        startPos = pos;
                    }
                    ++pos;
                }
                if (startPos < key.size()) {
                    list.push_back(utils::safe_substr(key, startPos));
                }
            }
            return list;
        }

        void pushRomanEntry(const MString& key) {
            _LOG_DEBUGH(_T("convertRomanToKatakana: key={}"), to_wstr(key));
            resultList.PushEntry(key, key + MSTR_VERT_BAR + MSTR_HASH_MARK + RomanToKatakana::convertRomanToKatakana(key));
        }

        // resultList に最近使ったものから取得した候補を格納し、pasts には set_ に含まれるものでそれ以外の候補を格納する
        // wlen > 0 なら、その長さの候補だけを返す
        void extract_and_copy(const MString& key, std::set<MString>& set_, size_t wlen, bool bWild = false) {
            if (!bWild) bWild = key.find('?') != MString::npos;
            _LOG_DEBUGH(_T("extract_and_copy(key={}, bWild={}, wlen={}, set_.size()={}, set_.begin()={}"), to_wstr(key), bWild, wlen, set_.size(), set_.empty() ? L"(none)" : to_wstr(*set_.begin()));
            resultList.SetKeyInfoIfFirst(key, bWild);
            size_t keylen = key.size();
            size_t n = 0;
            bool bRomanNeeded = utils::isRomanString(key);

            if (keylen <= 3) {
                // 短いキーでは、前半部がキーに完全一致する変換候補を最近使用候補より優先する
                usedList.ExtractExactUsedMapWords(key, resultList, set_, wlen);
                translatePriorityList.ExtractHeadWord(key, resultList, set_);
                std::vector<MString> exactUsedMapEntries;
                for (const auto& s : set_) {
                    if (s.size() > keylen && s[keylen] == VERT_BAR) exactUsedMapEntries.push_back(s);
                }
                for (const auto& s : exactUsedMapEntries) {
                    if ((wlen > 0 && s.size() == wlen) || (wlen == 0 && (keylen != 1 || s.size() >= 2))) {
                        pushCandidate(key, s, n);
                    }
                    set_.erase(s);
                }
                // 明示的な変換候補の次に、自動ローマ字変換を優先する
                if (bRomanNeeded) {
                    pushRomanEntry(key);
                    bRomanNeeded = false;
                }
                // 優先候補がなければ、ここから従来どおり最近使用順の前方一致候補になる
                usedList.ExtractUsedWords(key, resultList, set_, wlen);
            } else {
                // 4文字以上は従来の候補順を維持する
                usedList.ExtractUsedWords(key, resultList, set_, wlen);
                translatePriorityList.ExtractHeadWord(key, resultList, set_);
            }

            // set_ を vec に詰め替えてソートしてから回す。なお、'|' のままだと期待した順にならないので、'\t' に置換してからソートする(後で'|'に戻す)
            std::vector<MString> vec;
            std::transform(set_.begin(), set_.end(), std::back_inserter(vec), [](const auto& w) { return utils::replace_all(w, '|', '\t');});
            if (vec.size() < 100000) std::sort(vec.begin(), vec.end());

            for (const auto& s : vec) {
                if (bRomanNeeded) {
                    // ローマ字候補を追加
                    _LOG_DEBUGH(_T("check ROMAN key: s={}"), to_wstr(s));
                    size_t vbarPos = s.find_first_of('\t');
                    if (vbarPos < s.size() && vbarPos > key.size()) {
                        pushRomanEntry(key);
                        bRomanNeeded = false;
                    }
                }
                // keylen == 1 なら1文字単語は対象外
                if ((wlen > 0 && s.size() == wlen) || (wlen == 0 && (keylen != 1 || s.size() >= 2)) && s != key) {
                    pushCandidate(key, s, n);
                }
            }
            // 必要ならローマ字候補を追加
            if (bRomanNeeded) pushRomanEntry(key);

            _LOG_DEBUGH(_T("RESULT: resultList.size()={}"), resultList.Size());
        }

        // '*' をはさんで、前半部の key1 と後半部の key2 にマッチする文字列集合を取得
        // resultList に最近使ったものから取得した候補を格納し、その後にそれ以外の候補を格納する
        size_t extract_and_copy_for_wildecard_included(const MString& key) {
            size_t matchLen = 0;
            auto items = utils::split(key, '*');
            size_t itemsSize = items.size();
            if (itemsSize >= 2 && !items[itemsSize - 1].empty() && items[itemsSize - 1].size() <= 4) {
                // key が '*' を含み、最後の '*' の後が4文字以下
                std::set<MString> set_;
                const MString& key1 = items[itemsSize - 2];
                const MString& key2 = items[itemsSize - 1];
                _LOG_DEBUGH(_T("simple4CharsDic.FindMatchingWords({}, {})"), to_wstr(key1), to_wstr(key2));
                matchLen = simple4CharsDic.FindMatchingWords(key1, key2, set_) + key2.size() + 1;
                _LOG_DEBUGH(_T("matchLen={}, set_.size()={}"), matchLen, set_.size());
                extract_and_copy(utils::last_substr(key, matchLen), set_, 0, true);
                _LOG_DEBUGH(_T("resultList.size()={}"), resultList.Size());
            }
            return matchLen;
        }

        // key の pos 位置以降がマッチする候補の取得
        // まず key の pos 位置から4文字(i.e., key[pos, pos+4])にマッチする候補を取得し、それに対して startsWithWildKey() でさらにマッチをかける
        // resultList に最近使ったものから取得した候補を格納し、その後にそれ以外の候補を格納する
        // wlen > 0 なら、その長さの候補だけを返す
        void extract_and_copy_for_longer_than_4(const MString& key, size_t wlen, size_t pos) {
            auto subStr = key.substr(pos);
            auto subKey = subStr.substr(0, 4);
            _LOG_DEBUGH(_T("subStr={}, subKey={}"), to_wstr(subStr), to_wstr(subKey));
            std::set<MString> set_ = utils::filter(simple4CharsDic.GetSet(subKey, 4), [subStr](const auto& s) {return utils::startsWithWildKey(s, subStr, 0);});
            _LOG_DEBUGH(_T("filter(simple4CharsDic.GetSet(subKey={}, 4), startsWithWildKey(s, qKey={}, 0)): set_.size()={}"), to_wstr(subKey), to_wstr(subStr), set_.size());
            if (!set_.empty()) {
                //bool bWild = subStr.find('?') != MString::npos;
                extract_and_copy(subStr, set_, wlen);
            }
            _LOG_DEBUGH(_T("RESULT: pos={}, resultList.size()={}"), pos, resultList.Size());
        }

        // keyの末尾n文字にマッチする候補を取得して out に返す
        // wlen は候補文字列の長さに関する制約
        void extract_and_copy_for_tail_n(const MString& key, size_t n, size_t wlen = 0) {
            _LOG_DEBUGH(_T("CALLED: key={}, n={}, wlen={})"), to_wstr(key), n, wlen);
            std::set<MString> set_ = simple4CharsDic.GetSet(key, n);
            if (!set_.empty()) {
                //MString tailKey = utils::last_substr(key, n);
                //bool bWild = tailKey.find('?') != MString::npos;
                extract_and_copy(utils::last_substr(key, n), set_, wlen);
            }
        }

    public:
        // 指定の部分文字列に対する変換候補のリストを取得する
        // key.size() == 1 なら 2文字以上の候補列を返す
        // key.size() >= 2 なら key.size() 文字以上の候補を返す
        const SImpleDicResultList& GetCandidates(const MString& key, MString& resultKey,
                                            bool allowSingleAsciiMap = false) override {
            _LOG_DEBUGH(_T("ENTER: key={}, allowSingleAsciiMap={}"), to_wstr(key), allowSingleAsciiMap);
            // 結果を返すためのリストをクリアしておく
            resultList.ClearKeyInfo();
            if (!key.empty()) {
                // マッチしたキーの長さ(0ならキー全体がマッチ、>0 ならマッチした末尾の長さ)
                size_t resultKeyLen = 0;

                // key が '*' を含んでいる場合にワイルドカードのマッチングを行う
                resultKeyLen = extract_and_copy_for_wildecard_included(key);

#define IS_LIST_EMPTY() (resultList.Empty())

                // 英大文字でない文字を含む(ローマ字キーとみなす)
                bool bIsRomanKey = utils::isAsciiString(key) && !utils::isUpperAlphabetString(key);
                bool bListEmpty = IS_LIST_EMPTY();
                bool bAll = SETTINGS->simpleDicGatherAllCandidates && bListEmpty;

                _LOG_DEBUGH(_T("bAll={}"), bAll);

#define CHECK_LIST_EMPTY(n) \
            if (bListEmpty) {\
                resultKeyLen = n;\
                resultList.ClearKeyInfo();\
            }

                // Phase-A
                size_t keySize = key.size();

                // keyが5文字以上の場合には、先頭からマッチングさせる(4文字以下の場合は、この後の Phase-B で試される)
                if ((bAll || bListEmpty) && keySize >= 5) {
                    // "■■■■■" (5)
                    _LOG_DEBUGH(_T("CHECK-POINT-A"));
                    CHECK_LIST_EMPTY(0);    // 最終的にマッチすれば、先頭からのマッチになるので 0 でよい
                    extract_and_copy_for_longer_than_4(key, 0, 0);
                }
                bListEmpty = IS_LIST_EMPTY();

                // 上記がマッチせず、keyが6文字以上の非romanキーの場合には、key.substr(1) について試す
                if ((bAll || bListEmpty) && keySize >= 6 && !bIsRomanKey) {
                    // "□■■■■■" (6)
                    _LOG_DEBUGH(_T("CHECK-POINT-B"));
                    CHECK_LIST_EMPTY(5);
                    extract_and_copy_for_longer_than_4(key, 0, 1);
                }
                bListEmpty = IS_LIST_EMPTY();

                // 上記がマッチせず、keyが7文字以上の非romanキーの場合には、末尾から6文字および5文字について試す
                if ((bAll || bListEmpty) && keySize >= 7 && !bIsRomanKey) {
                    if (keySize >= 8) {
                        // "□□■■■■■■" (8)
                        _LOG_DEBUGH(_T("CHECK-POINT-C"));
                        CHECK_LIST_EMPTY(6);
                        extract_and_copy_for_longer_than_4(key, 0, keySize - resultKeyLen);
                    }
                    bListEmpty = IS_LIST_EMPTY();
                    if (keySize == 7 || (bAll || bListEmpty)) {
                        // "□□■■■■■" (7)
                        // "□□□■■■■■" (8)
                        _LOG_DEBUGH(_T("CHECK-POINT-D"));
                        CHECK_LIST_EMPTY(5);
                        extract_and_copy_for_longer_than_4(key, 0, keySize - resultKeyLen);
                    }
                }
                bListEmpty = IS_LIST_EMPTY();

                // Phase-B (4文字以下のマッチング)
                if ((bAll || bListEmpty) && (!bIsRomanKey || keySize <= 4)) {
                    auto checkFunc = [key](size_t len) { return key.size() >= len; };

                    if (checkFunc(4)) {
                        _LOG_DEBUGH(_T("CHECK-POINT-4"));
                        CHECK_LIST_EMPTY(4);
                        extract_and_copy_for_tail_n(key, 4);
                        _LOG_DEBUGH(_T("simpleDic4: resultList.size()={}"), resultList.Size());
                    }
                    bListEmpty = IS_LIST_EMPTY();

                    if ((bAll || bListEmpty) && (!bIsRomanKey || keySize <= 3)) {
                        if (checkFunc(3)) {
                            _LOG_DEBUGH(_T("CHECK-POINT-3"));
                            CHECK_LIST_EMPTY(3);
                            extract_and_copy_for_tail_n(key, 3);
                            _LOG_DEBUGH(_T("simpleDic3: resultList.size()={}"), resultList.Size());
                        }
                        bListEmpty = IS_LIST_EMPTY();
                        if ((bAll || bListEmpty) && (!bIsRomanKey || keySize <= 2)) {
                            if (checkFunc(2)) {
                                _LOG_DEBUGH(_T("CHECK-POINT-2"));
                                CHECK_LIST_EMPTY(2);
                                extract_and_copy_for_tail_n(key, 2);
                                _LOG_DEBUGH(_T("simpleDic2: resultList.size()={}"), resultList.Size());
                            }
                            bListEmpty = IS_LIST_EMPTY();
                            if ((bAll || bListEmpty) && (!bIsRomanKey || keySize <= 1) &&
                                (allowSingleAsciiMap || !is_ascii_char(tail_char(key)))) {
                                // 末尾1文字がASCII文字のものは、英数モードから明示的に変換した場合だけ対象とする
                                if (checkFunc(1)) {
                                    _LOG_DEBUGH(_T("CHECK-POINT-1"));
                                    CHECK_LIST_EMPTY(1);
                                    extract_and_copy_for_tail_n(key, 1);
                                    _LOG_DEBUGH(_T("simpleDic1: resultList.size()={}"), resultList.Size());
                                }
                            }
                        }
                    }
                }

                _LOG_DEBUGH(_T("CHECK-POINT-F"));
                if (resultList.Empty()) {
                    _LOG_DEBUGH(_T("CHECK-POINT-G: resultList.Empty"));
                    // 履歴検索で結果がなかった場合
                    if (is_ascii_str(key)) {
                        _LOG_DEBUGH(_T("CHECK-POINT-H: find ASCII key: {}"), to_wstr(key));
                        // 英大文字で区切って検索、なければローマ字化
                        auto words = splitByCapitalLetter(key);
                        if (words.size() > 1) {
                            _LOG_DEBUGH(_T("CHECK-POINT-I: splitted words={}"), to_wstr(utils::join(words, ':')));
                            MString joinedWord;
                            for (const auto& w : words) {
                                if (w.size() > 1) {
                                    // ここで候補取得処理の再帰呼び出し
                                    GetCandidates(w, resultKey);
                                    //const SimpleDicResult& hr = resultList.findSameResultMapKey(w);
                                    const MString& rw = resultList.GetNthWord(0);
                                    size_t pos = rw.find_first_of(VERT_BAR);
                                    if (pos + 1 < rw.size()) {
                                        ++pos;
                                        if (rw[pos] == HASH_MARK) ++pos;
                                        // 取得した結果を連結
                                        joinedWord.append(rw.substr(pos));
                                    }
                                    resultKey.clear();
                                } else if (!w.empty()) {
                                    // 1文字なら変換せずにそのまま使用
                                    joinedWord.append(1, w[0]);
                                }
                            }
                            _LOG_DEBUGH(_T("CHECK-POINT-J: Join Katakana"));
                            resultList.ClearKeyInfo();
                            resultList.PushEntry(key, key + MSTR_VERT_BAR + MSTR_HASH_MARK + joinedWord);
                        } else {
                            _LOG_DEBUGH(_T("CHECK-POINT-K"));
                            pushRomanEntry(key);
                        }
                        _LOG_DEBUGH(_T("CHECK-POINT-L"));
                        resultKey = key;   // 全体がマッチ
                    } else {
                        _LOG_DEBUGH(_T("CHECK-POINT-M"));
                        resultKey.clear();
                    }
                } else {
                    _LOG_DEBUGH(_T("CHECK-POINT-N"));
                    resultKey = resultKeyLen == 0 ? key : utils::last_substr(key, resultKeyLen);    // resultKeyLen == 0 なら全体がマッチ
                }
                _LOG_DEBUGH(_T("CHECK-POINT-O: resultKey={}, resultKeyLen={}, resultList.size()={}"), to_wstr(resultKey), resultKeyLen, resultList.Size());
            }

            if (SETTINGS->simpleDicMoveShortestAt2nd) {
                // 最短語を少なくとも先頭から2番目に移動する
                _LOG_DEBUGH(_T("CHECK-POINT-P"));
                resultList.MoveShortestResultAt2nd();
            }

            _LOG_DEBUGH(_T("LEAVE: resultKey={}, resultList.size()={}"), to_wstr(resultKey), resultList.Size());
            return resultList;
        }

        // 使用した単語をリストの先頭に追加or移動
        void UseWord(const MString& word) override {
            usedList.PushEntry(word, 0);    // 1文字の単語についても優先順の移動をするため、ここの minlen は1以下にしておく
        }

        // 使用辞書の読み込み
        void ReadUsedFile(const std::vector<String>& lines) override {
            LOG_INFOH(_T("CALLED"));
            usedList.ReadFile(lines);
        }

        // 使用辞書内容の保存
        void WriteUsedFile(utils::OfstreamWriter& writer) override {
            LOG_SAVE_DICT(_T("CALLED: Save User Entries"));
            usedList.WriteFile(writer);
        }

        bool IsUsedDicDirty() const override {
            return usedList.IsDirty();
        }

    private:
    };
    DEFINE_CLASS_LOGGER(SimpleDictionaryImpl);

} // namespace

// -------------------------------------------------------------------
DEFINE_CLASS_LOGGER(SimpleDictionary);

std::unique_ptr<SimpleDictionary> SimpleDictionary::Singleton;

namespace {
    DEFINE_NAMESPACE_LOGGER(SimpleDic_Local);

    String _systemRomanDicPath;

    typedef void (SimpleDictionary::* READ_FUNC)(const std::vector<String>& lines);

    // 履歴ファイルの読み込み
    void readFile(StringRef path, READ_FUNC func, bool bWarn = true) {
        LOG_INFOH(_T("ENTER: open simple dic file: {}"), path);
        utils::IfstreamReader reader(path);
        if (reader.success()) {
            // ファイル読み込み
            (SIMPLE_DIC.get()->*func)(reader.getAllLines());
            LOG_INFOH(_T("close simple dic file: {}"), path);
        } else {
            if (bWarn) LOG_WARN(_T("Can't read simple dic file: {}"), path);
        }
        LOG_INFOH(_T("LEAVE"));
    };

    typedef void (SimpleDictionary::* WRITE_FUNC)(utils::OfstreamWriter&);

    // 辞書ファイルの内容の書き出し
    void writeFile(StringRef path, WRITE_FUNC func) {
        LOG_SAVE_DICT(_T("ENTER: path={}"), path);
        if (!path.empty() && SIMPLE_DIC) {
            utils::OfstreamWriter writer(path);
            if (writer.success()) {
                (SIMPLE_DIC.get()->*func)(writer);
            }
        }
        LOG_SAVE_DICT(_T("LEAVE: path={}"), path);
    }

    // 簡易辞書を読み込む
    void readSimpleDictionary(StringRef dicFile, StringRef sysRomanPath) {
        LOG_INFOH(_T("ENTER: dicFile={}, sysRomanPath={}"), dicFile, sysRomanPath);

        // 辞書ファイルが無くても辞書インスタンスは作成する
        hashToStrMap.Clear();
        SimpleDictionary::Singleton.reset(new SimpleDictionaryImpl());

        if (!sysRomanPath.empty()) {
            // システムローマ字辞書ファイルの読み込み
            _systemRomanDicPath = sysRomanPath;
            LOG_DEBUGH(_T("open system roman file: {}"), sysRomanPath);
            readFile(sysRomanPath, &SimpleDictionary::ReadDicFileAsReadOnlyFirstPreferred);
        }

        if (!dicFile.empty()) {
            String filename = dicFile;
            auto path = utils::joinPath(USER_FILES_FOLDER, filename);
            LOG_DEBUGH(_T("open simple dic file: {}"), path);

            readFile(path, &SimpleDictionary::ReadDicFileAsReadOnlyLastPreferred);
            readFile(path + _T(".recent"), &SimpleDictionary::ReadUsedFile);
        }
        LOG_INFOH(_T("LEAVE"));
    }
}

// 簡易辞書ファイルを読み込んで、辞書を作成する
// エラーがあったら例外を投げる
int SimpleDictionary::CreateSimpleDictionary(StringRef dicFile, StringRef _NDEBUG_SENT(sysRomanPath)) {
    LOG_INFOH(_T("ENTER: dicFile={}"), dicFile);

    if (Singleton != 0) {
        LOG_DEBUGH(_T("already created: simple dic file: {}"), dicFile);
        return 0;
    }

    _NDEBUG_SENT(_systemRomanDicPath = sysRomanPath);
    readSimpleDictionary(dicFile, _systemRomanDicPath);

    LOG_INFOH(_T("LEAVE"));
    return 0;
}

// 簡易辞書を読み込む
int SimpleDictionary::ReloadSimpleDictionary() {
    readSimpleDictionary(SETTINGS->simpleDicFile, _systemRomanDicPath);
    return 0;
}

// 最近使用辞書ファイルの内容の書き出し
void SimpleDictionary::WriteSimpleDictionary(StringRef dicFile) {
    LOG_SAVE_DICT(_T("ENTER: dicFile={}"), dicFile);
    if (Singleton) {
        auto path = utils::joinPath(USER_FILES_FOLDER, dicFile) + _T(".recent");
        if (Singleton->IsUsedDicDirty()) {
            LOG_SAVE_DICT(_T("path={}"), path);
            writeFile(path, &SimpleDictionary::WriteUsedFile);
        }
    }
    LOG_SAVE_DICT(_T("LEAVE: path={}"), dicFile);
}

// 辞書ファイルの内容の書き出し
void SimpleDictionary::WriteSimpleDictionary() {
    WriteSimpleDictionary(SETTINGS->simpleDicFile);
}
