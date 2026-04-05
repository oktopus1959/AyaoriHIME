#include "Logger.h"
#include "settings.h"
#include "StateCommonInfo.h"

#include "Llama/LlamaBridge.h"

#include "Lattice.h"
#include "Lattice2_Common.h"
#include "Lattice2_CandidateString.h"
#include "Lattice2_Kbest.h"
#include "Lattice2_Ngram.h"
#include "Lattice2_Morpher.h"

namespace {
    DEFINE_LOGGER(Lattice2_Kbest);
}

namespace lattice2 {
    // 漢字用の余裕
    //int EXTRA_KANJI_BEAM_SIZE = 3;

    //// 多ストロークの範囲 (stroke位置的に組み合せ不可だったものは、strokeCount が範囲内なら残しておく)
    //int AllowedStrokeRange = 5;

    //// 末尾がここで設定した長さ以上に同じ候補は、先頭だけを残して削除
    //int LastSameLen = 99;

    // 末尾が同じ候補と見なす末尾長
    size_t SAME_TAIL_LEN = 3;

    //// 解の先頭部分が指定の長さ以上で同じなら、それらだけを残す
    //int SAME_LEADER_LEN = 99;

    // 形態素解析の結果、交ぜ書きエントリであった場合のペナルティ
    //int MAZE_PENALTY_2 = 5000;      // 見出し2文字以下
    //int MAZE_PENALTY_3 = 3000;      // 見出し3文字
    //int MAZE_PENALTY_4 = 1000;      // 見出し4文字以上

    // SingleHitの高頻度助詞が、マルチストロークに使われているケースのコスト
    int SINGLE_HIT_HIGH_FREQ_JOSHI_KANJI_COST = 3000;

    // 先頭の1文字だけの場合に、複数の候補があれば、2つ目以降には初期コストを与える
    int HEAD_SINGLE_CHAR_ORDER_COST = 5000;

    // 非優先候補に与えるペナルティ
    int NON_PREFERRED_PENALTY = 1000000;

    //// パディング候補に与えるペナルティ(ペナルティの中で最大値; 候補のCostがこれ以上なら、その候補に Padding Piece が含まれていることが分かるようにする)
    //int PADDING_PENALTY = 100000000;

    // 上位保持する際の末尾の差分の長さ
    int tailDiffLenForPromotion = 8;

    //--------------------------------------------------------------------------------------
    inline bool isHighFreqJoshi(mchar_t mc) {
        return mc == L'が' || mc == L'を' || mc == L'に' || mc == L'の' || mc == L'で' || mc == L'は';
    }

    inline bool isCommitChar(mchar_t mch) {
        //return utils::is_punct_or_commit_char(mch) || utils::is_paren(mch);
        return !utils::is_japanese_char_except_nakaguro(mch) && !is_numeral((wchar_t)mch) && mch != L'　' && mch != ' ' && mch != L'「' && mch != L'」' && mch != L'『' && mch != L'』';
    }

    MString substringBetweenPunctuations(const MString& str) {
        int endPos = (int)(str.size());
        // まず末尾の句読点をスキップ
        for (; endPos > 0; --endPos) {
            if (!utils::is_punct_or_commit_char(str[endPos - 1])) break;
        }
        // 前方に向かって句読点を検索
        int startPos = endPos - 1;
        for (; startPos > 0; --startPos) {
            if (utils::is_punct_or_commit_char(str[startPos - 1])) break;
        }
        return utils::safe_substr(str, startPos, endPos - startPos);
    }

    MString substringBetweenNonJapaneseChars(const MString& str) {
        int endPos = (int)(str.size());
        // まず末尾の非日本語文字をスキップ
        for (; endPos > 0; --endPos) {
            if (!isCommitChar(str[endPos - 1])) break;
        }
        // 前方に向かって非日本語文字を検索
        int startPos = endPos - 1;
        for (; startPos > 0; --startPos) {
            if (isCommitChar(str[startPos - 1])) break;
        }
        return utils::safe_substr(str, startPos, endPos - startPos);
    }

    MString removeHeadSubstring(const MString& str, size_t headLen) {
        if (str.size() <= headLen) {
            return MString();
        } else {
            return utils::safe_substr(str, headLen);
        }
    }

    // 末尾が漢字1文字で、その前が漢字でない場合に true を返す
    bool isTailIsolatedKanji(const MString& str) {
        if (str.size() >= 1) {
            size_t pos = str.size() - 1;
            if (utils::is_kanji(str[pos])) {
                // 末尾が漢字1文字
                if (pos == 0 || !utils::is_kanji(str[pos - 1])) {
                    // その前が漢字でない
                    return true;
                }
            }
        }
        return false;
    }

    // 先頭候補以外に、非優先候補ペナルティを与える (先頭候補のペナルティは 0 にし、PaddingDerivedをfalseにする)
    void arrangePenalties(std::vector<CandidateString>& candidates, size_t nSameLen) {
        _LOG_DETAIL(_T("CALLED"));
        candidates.front().clean();
        for (size_t i = 1; i < nSameLen; ++i) {
            candidates[i].setPenalty(NON_PREFERRED_PENALTY * (int)i);
        }
    }

    // ストローク長の同じ候補の数を返す
    size_t getNumOfSameStrokeLen(const std::vector<CandidateString>& candidates) {
        size_t nSameLen = 0;
        if (candidates.size() > 1) {
            int strokeLen = candidates.front().strokeLen();
            ++nSameLen;
            for (auto iter = candidates.begin() + 1; iter != candidates.end() && iter->strokeLen() == strokeLen; ++iter) {
                ++nSameLen;
            }
        }
        return nSameLen;
    }

    // 先頭候補を最優先候補にする
    void selectFirst(std::vector<CandidateString>& candidates) {
        size_t nSameLen = getNumOfSameStrokeLen(candidates);
        if (nSameLen > 1) {
            arrangePenalties(candidates, nSameLen);
            LOG_INFO(_T("CALLED: First candidate preferred."));
        }
    }

    inline MString removeLastSpace(const MString& str) {
        MString result = str;
        auto pos = result.find_last_of(' ');
        if (pos != std::string::npos) {
            result.erase(pos, 1);
        }
        return result;
    }

    //--------------------------------------------------------------------------------------
    // K-best な文字列を格納する
    class KBestListImpl : public KBestList {
        DECLARE_CLASS_LOGGER;

        std::vector<CandidateString> _candidates;

        //std::vector<MString> _bestStack;

        //bool _prevBS = false;

        // 次候補/前候補選択された時の元の先頭候補位置 (-1なら候補選択されていないことを示す)
        int _origFirstCand = -1;

        // 次候補/前候補選択された時の選択候補位置
        int _selectedCandPos = 0;

        // 末尾漢字などの追加で一時的に拡大された候補数
        //int _extendedCandNum = 0;

        static bool _isEmpty(const std::vector<CandidateString> cands) {
            //return cands.empty() || (cands.size() == 1 && cands.front().string().empty());
            return cands.empty() || (cands.front().string().empty());
        }

        String _debugLog;

    public:
        // コンストラクター
        KBestListImpl() {
        }

        ~KBestListImpl() override {
            clearKbests(true);
        }

    private:
        std::vector<bool> _highFreqJoshiStroke;
        std::vector<bool> _rollOverStroke;
        MString _prevTopCandidateString;
        std::vector<int> _leaderPrefixStableCounts;
        size_t _fixedLeaderPrefixLen = 0;

        void setHighFreqJoshiStroke(int count, mchar_t ch) {
            if (count >= 0 && count < 1024) {
                if (count >= (int)_highFreqJoshiStroke.size()) {
                    _highFreqJoshiStroke.resize(count + 1);
                }
                if (isHighFreqJoshi(ch)) _highFreqJoshiStroke[count] = true;
            }
        }
        void setRollOverStroke(int count, bool flag) {
            if (count >= 0 && count < 1024) {
                if (count >= (int)_rollOverStroke.size()) {
                    _rollOverStroke.resize(count + 1);
                }
                _rollOverStroke[count] = flag;
            }
        }

        bool isSingleHitHighFreqJoshi(int count) const {
            return (size_t)count < _highFreqJoshiStroke.size() && (size_t)count < _rollOverStroke.size() ? _highFreqJoshiStroke[count] &&  !_rollOverStroke[count] : false;
        }

        static size_t getMinimumCandidateLength(const std::vector<CandidateString>& candidates) {
            size_t minLen = SIZE_MAX;
            for (const auto& cand : candidates) {
                if (cand.string().size() < minLen) {
                    minLen = cand.string().size();
                }
            }
            return minLen == SIZE_MAX ? 0 : minLen;
        }

        // 先頭部固定の判定状態をリセットする
        void resetFixedLeaderPrefixState() {
            _prevTopCandidateString.clear();
            _leaderPrefixStableCounts.clear();
            _fixedLeaderPrefixLen = 0;
        }

        // 先頭候補の各文字位置について、同じ文字が連続して現れた回数を更新し、固定 prefix 長を再計算する
        void updateFixedLeaderPrefixState(const MString& topStr) {
            int threshold = SETTINGS->fixLeaderCharStrokeCount;
            if (threshold <= 0 || topStr.empty()) {
                resetFixedLeaderPrefixState();
                return;
            }

            std::vector<int> counts(topStr.size(), 1);
            size_t commonLen = 0;
            while (commonLen < topStr.size() && commonLen < _prevTopCandidateString.size() &&
                topStr[commonLen] == _prevTopCandidateString[commonLen]) {
                ++commonLen;
            }

            for (size_t i = 0; i < commonLen; ++i) {
                counts[i] = i < _leaderPrefixStableCounts.size() ? _leaderPrefixStableCounts[i] + 1 : 2;
            }

            _prevTopCandidateString = topStr;
            _leaderPrefixStableCounts = std::move(counts);
            _fixedLeaderPrefixLen = 0;
            while (_fixedLeaderPrefixLen < _leaderPrefixStableCounts.size() &&
                _leaderPrefixStableCounts[_fixedLeaderPrefixLen] >= threshold) {
                ++_fixedLeaderPrefixLen;
            }
            _LOG_DETAIL(L"topStr={}, fixedLeaderPrefixLen={}, stableCounts={}",
                to_wstr(topStr), _fixedLeaderPrefixLen, utils::join(_leaderPrefixStableCounts, L", "));
        }

        // 固定済み prefix を持たない候補を候補列から除外する
        void applyFixedLeaderPrefixFilter() {
            if (_fixedLeaderPrefixLen == 0 || _candidates.size() <= 1) return;

            MString prefix = utils::safe_substr(_candidates.front().string(), 0, _fixedLeaderPrefixLen);
            int topStrokeLen = _candidates.front().strokeLen();
            size_t idx = 1;
            while (idx < _candidates.size()) {
                if (_candidates[idx].strokeLen() < topStrokeLen) {
                    // これより後は、ストローク長が短い候補しかないので、ループを抜ける
                    break;
                } else if (!utils::startsWith(_candidates[idx].string(), prefix)) {
                    _LOG_DETAIL(L"erase by fixed leader prefix: prefix={}, cand={}", to_wstr(prefix), _candidates[idx].debugString());
                    _candidates.erase(_candidates.begin() + idx);
                } else {
                    ++idx;
                }
            }
        }

    public:
        //void setPrevBS(bool flag) override {
        //    _prevBS = flag;
        //}

        //bool isPrevBS() const override {
        //    return _prevBS;
        //}

        // 候補選択による、リアルタイムNgramの蒿上げと抑制
        void raiseAndLowerByCandSelection() override {
            _LOG_DETAIL(L"CALLED: _origFirstCand={}, _candidates.size={}", _origFirstCand, _candidates.size());
            if (_origFirstCand > 0 && (size_t)_origFirstCand < _candidates.size()) {
                _LOG_DETAIL(L"ENTER");
                // 候補選択がなされていて、元の先頭候補以外が選択された
                updateSelectedNgramByUserSelect(_candidates[_origFirstCand].string(), _candidates[0].string());
                updateMazegakiPreference(_candidates[0], _candidates[_origFirstCand]);
                removeOtherThanFirstForEachStroke();
                _origFirstCand = -1;
                _LOG_DETAIL(L"LEAVE: CAND_SELECT:\nkBest:\n{}", debugCandidates(10));
            }
        }

        void clearKbests(bool clearAll) override {
            _LOG_DETAIL(L"ENTER: clearAll={}", clearAll);
            _candidates.clear();
            //_bestStack.clear();
            _highFreqJoshiStroke.clear();
            _rollOverStroke.clear();
            _origFirstCand = -1;
            resetFixedLeaderPrefixState();
            //_extendedCandNum = 0;
            if (clearAll) {
                //clearKanjiXorHiraganaPreferredNext();
            }
            _LOG_DETAIL(L"LEAVE");
        }

        void removeOtherThanKBest() override {
            _LOG_DETAIL(L"ENTER");
            if ((int)_candidates.size() > SETTINGS->multiStreamBeamSize) {
                _candidates.erase(_candidates.begin() + SETTINGS->multiStreamBeamSize, _candidates.end());
            }
            _LOG_DETAIL(L"LEAVE");
        }

        void removeOtherThanFirst() override {
            _LOG_DETAIL(L"ENTER");
            if (_candidates.size() > 0) {
                _candidates.erase(_candidates.begin() + 1, _candidates.end());
                _candidates.front().clean();
            }
            _LOG_DETAIL(L"LEAVE");
        }

        void removeOtherThanFirstForEachStroke() override {
            _LOG_DETAIL(L"ENTER");
            if (_candidates.size() > 0) {
                const MString& first = _candidates[0].string();
                size_t n = 1;
                while (n < _candidates.size()) {
                    if (!utils::startsWith(first, _candidates[n].string())) {
                        _candidates.erase(_candidates.begin() + n);
                    } else {
                        _candidates[n].clean();
                        ++n;
                    }
                }
            }
            _LOG_DETAIL(L"LEAVE");
        }

        void removeOtherThanLongestStrokeCandidate() override {
            _LOG_DETAIL(L"ENTER");
            if (_candidates.size() > 1) {
                int topLen = _candidates.front().strokeLen();
                size_t n = 0;
                while (n < _candidates.size()) {
                    if (_candidates[n].strokeLen() < topLen) break;
                }
                if (n < _candidates.size()) {
                    _candidates.erase(_candidates.begin() + n, _candidates.end());
                }
            }
        }

        bool isEmpty() const override {
            return _isEmpty(_candidates);
        }

        int size() const override {
            return _candidates.size();
        }

        MString getTopString() override {
            _LOG_DETAIL(L"_candidates.size={}, topString=\"{}\"", _candidates.size(), to_wstr(_candidates.empty() ? MString() : _candidates[0].string()));
            return _candidates.empty() ? MString() : _candidates[0].string();
        }

        int origFirstCand() const override {
            return _origFirstCand;
        }

        int selectedCandPos() const override {
            return _origFirstCand < 0 ? _origFirstCand : _selectedCandPos;
        }

        void setSelectedCandPos(int nSameLen) override {
            _selectedCandPos = _origFirstCand == 0 ? _origFirstCand : nSameLen - _origFirstCand;
        }

        int cookedOrigFirstCand() const override {
            return _origFirstCand >= 0 ? _origFirstCand : 0;
        }

        void resetOrigFirstCand() override {
            _origFirstCand = -1;
            //_extendedCandNum = 0;
        }

        void incrementOrigFirstCand(int nSameLen) override {
            if (_origFirstCand < 0) {
                _origFirstCand = 1;
            } else {
                ++_origFirstCand;
                if (_origFirstCand >= (int)nSameLen) _origFirstCand = 0;
            }
            setSelectedCandPos(nSameLen);
            //_extendedCandNum = nSameLen;
        }

        void decrementOrigFirstCand(int nSameLen) override {
            --_origFirstCand;
            if (_origFirstCand < 0) _origFirstCand = nSameLen - 1;
            setSelectedCandPos(nSameLen);
            //_extendedCandNum = nSameLen;
        }

        // ストローク長の同じ候補の数を返す
        size_t getNumOfSameStrokeLen() const override {
            return lattice2::getNumOfSameStrokeLen(_candidates);
        }

        std::vector<MString> getTopCandStrings() const override {
            _LOG_DETAIL(L"ENTER: _candidates.size={}, _origFirstCand={}, _selectedCandPos={}", _candidates.size(), _origFirstCand, _selectedCandPos);
            std::vector<MString> result;
            int nSameLen = getNumOfSameStrokeLen();
            int first = cookedOrigFirstCand();
            if (first > nSameLen) first = nSameLen;
            _LOG_DETAIL(L"nSameLen={}, first={}", nSameLen, first);
            for (int i = first; i < nSameLen; ++i) {
                result.push_back(_candidates[i].string());
            }
            for (int i = 0; i < first; ++i) {
                result.push_back(_candidates[i].string());
            }

            size_t maxLen = 0;
            for (const auto& s : result) {
                if (maxLen < s.size()) maxLen = s.size();
            }
            size_t dispLen = 12;
            size_t pos = maxLen > dispLen ? maxLen - dispLen : 0;
            if (pos > 0) {
                for (auto& s : result) {
                    s = s.substr(pos);
                }
            }
            _LOG_DETAIL(L"LEAVE: result.size()={}, _origFirstCand={}, _selectedCandPos={}", result.size(), _origFirstCand, _selectedCandPos);
            return result;
        }

        // 同一ストローク長の複数候補が存在するか
        bool hasMultiCandStringsWithSameStrokeLen() const override {
            int nSameLen = getNumOfSameStrokeLen();
            _LOG_DETAIL(L"CALLED: result={}: nSameLen={}", nSameLen > 1, nSameLen);
            return nSameLen > 1;
        }

        // 選択位置を含む、同一ストローク長の候補ブロック(最大10件)を返す
        std::vector<MString> getCandStringsInSelectedBlock() const override {
            // ブロックサイズ (最初は最小数, 選択開始後は最大数)
            const int BLOCK_SIZE = selectedCandPos() < 0 ? SETTINGS->mergerCandidateMin : SETTINGS->mergerCandidateMax;
            _LOG_DETAIL(L"ENTER: _candidates.size={}, BLOCK_SIZE={}, _origFirstCand={}, _selectedCandPos={}", _candidates.size(), BLOCK_SIZE, _origFirstCand, _selectedCandPos);
            std::vector<MString> result;
            int nSameLen = getNumOfSameStrokeLen();
            if (nSameLen <= 0 || BLOCK_SIZE <= 0) {
                _LOG_DETAIL(L"LEAVE: nSameLen({})<=0 || BLOCKSIZE({})<=0", nSameLen, BLOCK_SIZE);
                return result;
            }

            int first = cookedOrigFirstCand();
            if (first > nSameLen) first = nSameLen;

            // cookedOrigFirstCand() を始点とする順序で並べ替えた「表示用順序」配列を作る
            std::vector<MString> ordered;
            ordered.reserve(nSameLen);
            for (int i = first; i < nSameLen; ++i) ordered.push_back(_candidates[i].string());
            for (int i = 0; i < first; ++i) ordered.push_back(_candidates[i].string());

            // 選択位置 (表示用順序でのインデックス)
            int selPos = selectedCandPos();   // -1 の場合は未選択
            if (selPos < 0 || selPos >= (int)ordered.size()) selPos = 0;

            // ブロック計算
            int blockStart = (selPos / BLOCK_SIZE) * BLOCK_SIZE;
            int blockEnd = std::min(blockStart + BLOCK_SIZE, (int)ordered.size());

            for (int i = blockStart; i < blockEnd; ++i) result.push_back(ordered[i]);

            // 末尾最大12文字表示にトリミング (getTopCandStrings と同様)
            size_t maxLen = 0;
            for (const auto& s : result) if (maxLen < s.size()) maxLen = s.size();
            size_t dispLen = 12;
            size_t cutPos = maxLen > dispLen ? maxLen - dispLen : 0;
            if (cutPos > 0) {
                for (auto& s : result) {
                    if (s.size() > cutPos) s = s.substr(cutPos);
                }
            }

            _LOG_DETAIL(L"LEAVE: result.size()={}: nSameLen={}, first={}, selPos={}, blockStart={}, blockEnd={}",
                result.size(), nSameLen, first, selPos, blockStart, blockEnd);
            return result;
        }

        String debugCandidates(size_t maxLn = 100000) const override {
            String result;
            for (size_t i = 0; i < _candidates.size() && i < maxLn; ++i) {
                result.append(std::to_wstring(i));
                result.append(_T(": "));
                result.append(_candidates[i].debugString());
                result.append(_T("\n"));
            }
            return result;
        }

        String debugKBestString(size_t maxLn = 100000) const override {
            String result;
            result.append(_debugLog);
            result.append(std::format(L"\nTotal candidates={}\n", _candidates.size()));
            result.append(L"\nKBest:\n");
            result.append(debugCandidates(maxLn));
            return result;
        }

    private:
        float calcLlamaLoss(const MString& s) {
            std::vector<float> logits;
            auto loss = LlamaBridge::llamaCalcCost(s, logits);
            LOG_INFO(L"{}: loss={}, logits={}", to_wstr(s), loss, utils::join_primitive(logits, L","));
            return loss;
        }

        int calcLlamaCost(const MString& s) {
            std::vector<float> logits;
            auto loss = s.size() > 1 ? LlamaBridge::llamaCalcCost(s, logits) : 2.0f;
            int cost = (int)((loss / s.size()) * 1000);
            LOG_INFO(L"{}: loss={}, cost={}, logits={}", to_wstr(s), loss, cost, utils::join_primitive(logits, L","));
            return cost;
        }

        // 2つの文字列の末尾文字列の共通部分が指定の長さより長いか、または全く同じ文字列か
        bool hasLongerCommonSuffixThanOrSameStr(const MString& str1, const MString& str2, int len) {
            LOG_DEBUGH(_T("ENTER: str1={}, str2={}, len={}"), to_wstr(str1), to_wstr(str2), len);
            int n1 = (int)str1.size();
            int n2 = (int)str2.size();
            bool result = false;
            if (len <= n1 && len <= n2) {
                while (n1 > 0 && n2 > 0 && len > 0) {
                    if (str1[n1 - 1] != str2[n2 - 1]) break;
                    --n1;
                    --n2;
                    --len;
                }
                result = len == 0 || (n1 == 0 && n2 == 0);
            } else {
                result = (n1 == n2 && str1 == str2);
            }
            LOG_DEBUGH(_T("LEAVE: remainingLen: {}: str1={}, str2={}, common={}"), utils::boolToString(result), n1, n2, len);
            return result;
        }

        // 候補のコストを計算
        // minLenは、形態素解析やNgram解析の際に、長い候補文字列に対して共通の先頭部分を避けるために使用する
        // @returns 末尾の形態素の長さ (末尾の形態素がある場合)
        int calcCandidateCost(CandidateString& newCandStr, size_t minLen, bool useMorphAnalyzer, bool isStrokeBS) {
            _LOG_DETAIL(_T("\nENTER: newCandStr={}, useMorphAnalyzer={}, isStrokeBS={}"), newCandStr.debugString(), useMorphAnalyzer, isStrokeBS);

            const MString& candStr = newCandStr.string();
            //MString targetStr = substringBetweenPunctuations(candStr);
            //MString targetStr = substringBetweenNonJapaneseChars(candStr);
            //int analyzeMorphLen = SETTINGS->analyzeMorphLen;
            int analyzeMorphLen = (SETTINGS->variableTailLength * 3) / 2;
            if (analyzeMorphLen < SETTINGS->analyzeMorphLen) analyzeMorphLen = SETTINGS->analyzeMorphLen;
            int headLen = minLen > (size_t)analyzeMorphLen + 1 ? (int)minLen - analyzeMorphLen : 0;
            MString subStr = removeHeadSubstring(candStr, headLen);
            _LOG_DETAIL(_T("candStr={}, minLen={}, headLen={}, subStr={}"), to_wstr(candStr), minLen, headLen, to_wstr(subStr));

            std::vector<MString> morphs;

            int myTotalCost = newCandStr.totalCost();

            int tailMorphLen = 0;

            if (useMorphAnalyzer) {
                // 形態素解析コスト
                // 1文字以下なら、形態素解析しない(過|禍」で「禍」のほうが優先されて出力されることがあるため（「禍」のほうが単語コストが低いため）)
                //int morphCost = !SETTINGS->useMorphAnalyzer || subStr.size() <= 1 ? 5000 : calcMorphCost(subStr, morphs);
                int morphCost = subStr.empty() ? 0 : calcMorphCost(subStr, morphs);
                if (subStr.size() == 1) {
                    if (utils::is_katakana(subStr[0])) morphCost += 5000; // 1文字カタカナならさらに上乗せ
                }
                if (!morphs.empty()) {
                    if (isNonTerminalMorph(morphs.back())) {
                        _LOG_DETAIL(_T("NON TERMINAL morph={}"), to_wstr(morphs.back()));
                        newCandStr.setNonTerminal();
                    }
                    size_t delimiterPos = morphs.back().find(L'\t');
                    if (delimiterPos != MString::npos) {
                        tailMorphLen = (int)delimiterPos;
                    }
                }

                // Ngramコスト
                int ngramCost = subStr.empty() ? 0 : getNgramCost(subStr, morphs) * SETTINGS->ngramCostFactor;
                //int morphCost = 0;
                //int ngramCost = candStr.empty() ? 0 : getNgramCost(candStr);
                //int llamaCost = candStr.empty() ? 0 : calcLlamaCost(candStr) * SETTINGS->ngramCostFactor;

                // llamaコスト
                //int llamaCost = 0;

                // 総コスト
                newCandStr.addCost(morphCost, ngramCost);

                //// 「漢字+の+漢字」のような場合はペナルティを解除
                //size_t len = candStr.size();
                //if (len >= 3 && candStr[len - 2] == L'の' && !utils::is_hiragana(candStr[len - 3]) && !utils::is_hiragana(candStr[len - 1])) {
                //    newCandStr.clean();
                //}

                int candCost = morphCost + ngramCost;   // +llamaCost;
                myTotalCost = newCandStr.totalCost();
                _LOG_DETAIL(_T("CALC: candStr={}, myTotalCost={}, candCost={} (morph={}[<{}>], ngram={})"),
                    to_wstr(candStr), myTotalCost, candCost, morphCost, utils::reReplace(to_wstr(utils::join(morphs, to_mstr(L"> <"))), L"\t", L" "), ngramCost);

                if (WORD_LATTICE->isCandidateLogEnabled()) {
                    if (!isStrokeBS) _debugLog.append(std::format(L"candStr={}, myTotalCost={}, candCost={} (morph={} [<{}>] , ngram = {})\n",
                        to_wstr(candStr), myTotalCost, candCost, morphCost, utils::reReplace(to_wstr(utils::join(morphs, to_mstr(L"> <"))), L"\t", L" "), ngramCost));
                }
            }

            // 末尾の形態素の長さを返す
            return tailMorphLen;

            //if (!bAdded) {
            //    // 末尾に追加
            //    newCandidates.push_back(newCandStr);
            //    bAdded = true;
            //    LOG_DEBUGH(_T("ADDED at TAIL"));
            //}
            //if (bAdded) {
            //    _LOG_DETAIL(_T("    ADD candidate: {}, myTotalCost={}"), to_wstr(candStr), myTotalCost);
            //} else {
            //    _LOG_DETAIL(_T("    ABANDON candidate: {}, myTotalCost={}"), to_wstr(candStr), myTotalCost);
            //}
            //_LOG_DETAIL(_T("LEAVE: {}, newCandidates.size={}"), bAdded ? L"ADD" : L"ABANDON", newCandidates.size());
            //return bAdded;
        }

        String debugString(std::vector<CandidateString>& candidates) const {
            String result;
            result.append(_T("--------\n"));
            for (size_t i = 0; i < candidates.size(); ++i) {
                result.append(std::to_wstring(i));
                result.append(_T(": "));
                result.append(candidates[i].debugString());
                result.append(_T("\n"));
            }
            result.append(_T("--------"));
            return result;
        }

        void appendGeneratedCandidate(std::vector<CandidateString>& newCandidates, std::vector<bool>& promotedFlags, const CandidateString& cand, bool promote) {
            newCandidates.push_back(cand);
            promotedFlags.push_back(promote);
        }

        bool isMultiChoicePiece(const MString& s) {
            return s.size() > 1 && s.find('|') != MString::npos;
        }

        // reorder 後の先頭候補と比べて、末尾側でどれだけ違うかを測る
        size_t calcTailDifferenceLen(const MString& a, const MString& b) {
            size_t commonSuffixLen = 0;
            while (commonSuffixLen < a.size() && commonSuffixLen < b.size() &&
                a[a.size() - commonSuffixLen - 1] == b[b.size() - commonSuffixLen - 1]) {
                ++commonSuffixLen;
            }
            return std::max(a.size() - commonSuffixLen, b.size() - commonSuffixLen);
        }

        // ユーザーによるNgram選択をtotalCostに反映して、候補の順序を totalCost の昇順にソート
        void reorderCandidates(std::vector<CandidateString>& newCandidates, std::vector<bool>& promotedFlags) {
            // CandidateString::totalCost() の昇順にソート
            _LOG_DETAIL(_T("ENTER: newCandidates:\n{}"), debugString(newCandidates));
            // ユーザー選択Ngramペアを探して、totalCostを調整
            std::map<MString, std::tuple<int, std::set<size_t>, std::set<size_t>>> foundNgram;   // <Ngram, <bonusPoint, positiveCandidateIndexes, negativeCandidateIndexes>>
            for (size_t i = 0; i < newCandidates.size(); ++i) {
                _LOG_DETAIL(_T("cand[{}]={}"), i, newCandidates[i].debugString());
                const MString& candStr = newCandidates[i].string();
                //const std::set<SelectedNgramPairBonus> currentSet = findNgramPairBonus(candStr);
                // TODO: 「あい|阿井」「あいう|阿井宇」のようなSelectedNgram対がある場合は、両者ともマッチしてしまうケースもある(現在は両方とも計算に入ってしまう)
                //_LOG_DETAIL(_T("candStr={}"), to_wstr(candStr));
                for (const auto& current : findNgramPairBonus(candStr)) {
                    if (current.isValid(SETTINGS->hiraganaBigramEnabled)) {
                        auto iter = foundNgram.find(current.ngramPair);
                        if (iter != foundNgram.end()) {
                            // 既に見つかっている場合
                            if (current.bonusPoint > 0) {
                                // 今回の候補は正のボーナスポイント
                                _LOG_DETAIL(_T("positive bonus info: [{}] {}"), i, current.debugString());
                                std::get<1>(iter->second).insert(i);   // positiveCandidateIndexes に今回の候補位置を追加
                            } else if (current.bonusPoint < 0) {
                                // 今回の候補は負のボーナスポイント
                                _LOG_DETAIL(_T("negative bonus info: [{}] {}"), i, current.debugString());
                                std::get<2>(iter->second).insert(i);   // negativeCandidateIndexes に今回の候補位置を追加
                            }
                        } else {
                            if (current.bonusPoint > 0) {
                                foundNgram[current.ngramPair] = { current.bonusPoint, {i}, {} };
                            } else if (current.bonusPoint < 0) {
                                foundNgram[current.ngramPair] = { -current.bonusPoint, {}, {i} };
                            }
                        }
                    }
                }
            }

            // totalCost を再計算
            for (const auto& entry : foundNgram) {
                const auto& bonusInfo = entry.second;
                int bonus = calcNgramBonus(std::get<0>(bonusInfo));
                const auto& posiCandIndex = std::get<1>(bonusInfo);
                const auto& negaCandIndex = std::get<2>(bonusInfo);
                if (bonus > 0 && !posiCandIndex.empty() && !negaCandIndex.empty()) {
                    for (const auto& ci : posiCandIndex) {
                        newCandidates[ci].addNgramCost(-bonus);
                        _LOG_DETAIL(_T("positive selected Ngram: {}, bonus={}, cand=[{}] {}"), to_wstr(entry.first), bonus, ci, newCandidates[ci].debugString());
                    }
                    for (const auto& ci : negaCandIndex) {
                        newCandidates[ci].addNgramCost(bonus);
                        _LOG_DETAIL(_T("negative selected Ngram: {}, bonus={}, cand=[{}] {}"), to_wstr(entry.first), bonus, ci, newCandidates[ci].debugString());
                    }
                }
            }

            // 調整された totalCost に基づいてソート
            std::vector<std::pair<CandidateString, bool>> zippedCandidates;
            zippedCandidates.reserve(newCandidates.size());
            for (size_t i = 0; i < newCandidates.size(); ++i) {
                zippedCandidates.emplace_back(newCandidates[i], i < promotedFlags.size() ? promotedFlags[i] : false);
            }
            std::sort(zippedCandidates.begin(), zippedCandidates.end(),
                [](const auto& a, const auto& b) {
                    return a.first.totalCost() < b.first.totalCost();
                });
            for (size_t i = 0; i < zippedCandidates.size(); ++i) {
                newCandidates[i] = std::move(zippedCandidates[i].first);
                if (i < promotedFlags.size()) {
                    promotedFlags[i] = zippedCandidates[i].second;
                }
            }
            _LOG_DETAIL(_T("LEAVE"));
        }

        // reorder 後の候補列から、代表候補だけを先頭候補の直後へ寄せ直す
        void promoteRepresentativeCandidates(std::vector<CandidateString>& newCandidates, std::vector<bool>& promotedFlags) {
            _LOG_DETAIL(_T("ENTER: newCandidates.size={}, promotedFlags.size={}"), newCandidates.size(), promotedFlags.size());
            if (newCandidates.size() <= 1 || promotedFlags.size() != newCandidates.size()) return;

            std::vector<CandidateString> promoted;
            std::vector<bool> promotedMarks;
            std::vector<CandidateString> others;
            std::vector<bool> otherMarks;
            promoted.reserve(newCandidates.size());
            others.reserve(newCandidates.size());

            CandidateString firstCand = newCandidates.front();
            bool firstMark = promotedFlags.front();
            const MString& topStr = firstCand.string();

            for (size_t i = 1; i < newCandidates.size(); ++i) {
                // 先頭候補と末尾が近すぎるものは前寄せせず、通常のコスト順に任せる (とりあえず4文字以上違うものだけを前寄せの対象とする)
                if (promotedFlags[i] && calcTailDifferenceLen(topStr, newCandidates[i].string()) >= tailDiffLenForPromotion) {
                    promoted.push_back(std::move(newCandidates[i]));
                    promotedMarks.push_back(true);
                } else {
                    others.push_back(std::move(newCandidates[i]));
                    otherMarks.push_back(false);
                }
            }

            if (promoted.empty()) {
                _LOG_DETAIL(_T("LEAVE: no representative candidates"));
                return;
            }

            // 先頭候補はそのまま残し、その直後へ代表候補群を安定挿入する
            newCandidates.clear();
            promotedFlags.clear();
            newCandidates.reserve(1 + promoted.size() + others.size());
            promotedFlags.reserve(1 + promotedMarks.size() + otherMarks.size());
            newCandidates.push_back(std::move(firstCand));
            promotedFlags.push_back(firstMark);
            for (size_t i = 0; i < promoted.size(); ++i) {
                newCandidates.push_back(std::move(promoted[i]));
                promotedFlags.push_back(promotedMarks[i]);
            }
            for (size_t i = 0; i < others.size(); ++i) {
                newCandidates.push_back(std::move(others[i]));
                promotedFlags.push_back(otherMarks[i]);
            }
            _LOG_DETAIL(_T("LEAVE: promotedCount={}, newCandidates.size={}"), promotedMarks.size(), newCandidates.size());
        }

        // 同じ末尾部分を持つ候補が連続しないように移動する
        void rotateSameTailCandidates(std::vector<CandidateString>& newCandidates) {
            std::set<MString> candStrs;
            if (newCandidates.empty() || newCandidates.front().string().size() < SAME_TAIL_LEN) return;

            candStrs.insert(utils::safe_tailstr(newCandidates.front().string(), SAME_TAIL_LEN));
            for (size_t i = 1; i < newCandidates.size() && i < 3; ++i) {
                for (auto iter = newCandidates.begin() + i; iter != newCandidates.end(); ++iter) {
                    auto tail = utils::safe_tailstr(iter->string(), SAME_TAIL_LEN);
                    if (!candStrs.contains(tail)) {
                        if (iter != newCandidates.begin() + i) {
                            std::rotate(newCandidates.begin() + i, iter, iter + 1);
                        }
                        candStrs.insert(tail);
                        break;
                    }
                }
            }
        }

        // beamSize を超えた部分を削除する
        // また、SETTINGS->fixLeaderCharStrokeCount で指定されたストローク数以上、先頭部が同じ文字の場合にその部分を固定する
        // Padding piece を含まない候補が見つかったら、Padding piece を含む候補は削除する
        void truncateTailCandidates(std::vector<CandidateString>& newCandidates, std::vector<bool>& promotedFlags) {
            _LOG_DETAIL(_T("ENTER: newCandidates.size={}, beamSieze={}, extraBeamSizeRate={}"), newCandidates.size(), SETTINGS->multiStreamBeamSize, SETTINGS->extraBeamSizeRate);
            std::set<MString> triGrams;
            std::set<MString> biGrams;
            std::set<MString> uniGrams;
            const size_t beamSize = SETTINGS->multiStreamBeamSize;
            const size_t beamSize2 = beamSize + (int)(beamSize * SETTINGS->extraBeamSizeRate);
            size_t candCount = 0;
            size_t pickCount = 0;
            if (!newCandidates.empty()) {
                // Padding piece を含まない候補(「燃」など)を見つける
                bool bNonPaddingFound = std::any_of(newCandidates.begin(), newCandidates.end(),
                    [](const CandidateString& cand) { return !cand.isPaddingDerived(); });

                MString topHead;
                while (candCount < newCandidates.size()) {
                    auto iter = newCandidates.begin() + candCount;
                    bool isPromoted = candCount < promotedFlags.size() ? promotedFlags[candCount] : false;
                    const MString& str = iter->string();
                    const MString uni = str.size() >= 1 ? utils::safe_tailstr(str, 1) : EMPTY_MSTR;
                    const MString bi = str.size() >= 2 ? utils::safe_tailstr(str, 2) : EMPTY_MSTR;
                    const MString tri = str.size() >= 3 ? utils::safe_tailstr(str, 3) : EMPTY_MSTR;
                    if (topHead.size() == 0 || utils::startsWith(str, topHead)) {
                        // Padding piece を含まない候補(「燃」など)が見つかったら、Padding piece を含む候補は削除する
                        if (!bNonPaddingFound || !iter->isPaddingDerived()) {
                            if (isPromoted && pickCount < beamSize2) {
                                if (!uni.empty()) uniGrams.insert(uni);
                                if (!bi.empty()) biGrams.insert(bi);
                                if (!tri.empty()) triGrams.insert(tri);
                                _LOG_DETAIL(_T("[{}] PICK: {}: representative"), candCount, iter->debugString());
                                ++candCount;
                                ++pickCount;
                                continue;
                            }
                            if (candCount < beamSize) {
                                if (!uni.empty()) uniGrams.insert(uni);
                                if (!bi.empty()) biGrams.insert(bi);
                                if (!tri.empty()) triGrams.insert(tri);
                                _LOG_DETAIL(_T("[{}] PICK: {}: OK"), candCount, iter->debugString());
                                ++candCount;
                                ++pickCount;
                                continue;
                            } else {
                                if (IS_LOG_DEBUGH_ENABLED && candCount == beamSize) {
                                    _LOG_DETAIL(_T("count reached at beamSize={}, beamSize2={}"), beamSize, beamSize2);
                                    _LOG_DETAIL(_T("tail 1grams={}"), to_wstr(utils::join(uniGrams, ',')));
                                    _LOG_DETAIL(_T("tail 2gams={}"), to_wstr(utils::join(biGrams, ',')));
                                    _LOG_DETAIL(_T("tail 3grams={}"), to_wstr(utils::join(triGrams, ',')));
                                }
                                if (iter->isNonTerminal()) {
                                    // 非終端は残す(pickCountしない)
                                    _LOG_DETAIL(_T("[{}]: PICK: {}: non terminal"), candCount, iter->debugString());
                                    ++candCount;
                                    continue;
                                }
                                if (!uni.empty() && uniGrams.find(uni) == uniGrams.end()) {
                                    // 末尾文字の未採用バリエーションは最低1件残す
                                    if (!uni.empty()) uniGrams.insert(uni);
                                    if (!bi.empty()) biGrams.insert(bi);
                                    if (!tri.empty()) triGrams.insert(tri);
                                    _LOG_DETAIL(_T("[{}] PICK: {}: tailCharVariation({})"), candCount, iter->debugString(), to_wstr(uni));
                                    ++candCount;
                                    ++pickCount;
                                    continue;
                                }
                                if (pickCount < beamSize2) {
                                    // まだ余裕がある
                                    if (!tri.empty() && triGrams.find(tri) == triGrams.end()) {
                                        // 未見のtrigramであり、まだ余裕がある
                                        triGrams.insert(tri);
                                        _LOG_DETAIL(_T("[{}] PICK: {}: triGram({}) OK, triGrams.size={}"), candCount, iter->debugString(), to_wstr(tri), triGrams.size());
                                        ++candCount;
                                        ++pickCount;
                                        continue;
                                    }
                                    if (!bi.empty() && biGrams.find(bi) == biGrams.end()) {
                                        // 未見のbigramであり、まだ余裕がある
                                        biGrams.insert(bi);
                                        _LOG_DETAIL(_T("[{}] PICK: {}: biGram({}) OK, biGrams.size={}"), candCount, iter->debugString(), to_wstr(bi), biGrams.size());
                                        ++candCount;
                                        ++pickCount;
                                        continue;
                                    }
                                }
                            }
                        }
                    }
                    _LOG_DETAIL(_T("[{}] ERASE: {}"), candCount, iter->debugString());
                    newCandidates.erase(iter);
                    if (candCount < promotedFlags.size()) {
                        promotedFlags.erase(promotedFlags.begin() + candCount);
                    }
                }
            }
            if (candCount < newCandidates.size()) {
                newCandidates.resize(candCount);
                if (candCount < promotedFlags.size()) {
                    promotedFlags.resize(candCount);
                }
            }
            _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
            if (newCandidates.size() > maxCandidatesSize) {
                maxCandidatesSize = newCandidates.size();
                _LOG_DETAIL(_T("max newCandidates.size updated: {}"), maxCandidatesSize);
            }
        }

    private:
        // 漢字xorひらがなを優先すべき状況で、条件を満たしているか
        bool isKanjiOrHiraganaPreferenceSatisfied(const CandidateString& cand, const WordPiece& piece) {
            _LOG_DETAIL(L"cand.string()=\"{}\", piece={}", to_wstr(cand.string()), piece.debugString());
            bool result = true;
            if (!cand.isAnyFollowed() && !piece.isAnyPadding()) {
                const MString& pieceStr = piece.getString();
                if (piece.numBS() <= 0 && !pieceStr.empty()
                    && ((cand.isHiraganaFollowed() && !utils::is_hiragana(pieceStr[0])) || (cand.isKanjiFollowed() && !utils::is_kanji(pieceStr[0])))) {
                    // 漢字xorひらがな優先条件を満たさなかった
                    _LOG_DETAIL(_T("{} preference not satisfied"), cand.isKanjiFollowed() ? L"Kanji" : L"Hiragana");
                    result = false;
                }
            }
            _LOG_DETAIL(_T("LEAVE: result={}"), result);
            return result;
        }

    public:
        virtual FollowingPreferenceType getFollowingPreferenceType() const override {
            if (_candidates.empty()) {
                return FollowingPreferenceType::Any;
            }
            return _candidates.front().followingPreferenceType();
        }

    private:
        // 素片のストロークと適合する候補だけを追加
        void addOnePiece(std::vector<CandidateString>& newCandidates, std::vector<bool>& promotedFlags, const WordPiece& piece, FollowingPreferenceType prefType, bool useMorphAnalyzer, int strokeCount, int paddingLen, bool bKatakanaConversion) {
            _LOG_DETAIL(_T("ENTER: _candidates.size={}, piece={}, useMorphAnalyzer={}"), _candidates.size(), piece.debugString(), useMorphAnalyzer);
            bool bAutoBushuFound = false;           // 自動部首合成は一回だけ実行する
            bool isStrokeBS = piece.numBS() > 0;
            const MString& pieceStr = piece.getString();
            //int topStrokeLen = -1;

            if (pieceStr.size() == 1) setHighFreqJoshiStroke(strokeCount, pieceStr[0]);

            int singleHitHighFreqJoshiCost = piece.strokeLen() > 1 && isSingleHitHighFreqJoshi(strokeCount - (piece.strokeLen() - 1)) ? SINGLE_HIT_HIGH_FREQ_JOSHI_KANJI_COST : 0;

            std::vector<CandidateString> targetCandidates;
            // 素片のストロークと適合する候補を抽出
            for (const auto& cand : _candidates) {
                //if (topStrokeLen < 0) topStrokeLen = cand.strokeLen();
                bool bStrokeCountMatched = (piece.isAnyPadding() && cand.strokeLen() + paddingLen == strokeCount) || (!piece.isAnyPadding() && cand.strokeLen() + piece.strokeLen() == strokeCount);
                _LOG_DETAIL(_T("cand.string={}, cand.strokeLen={}, piece.strokeLen={}, strokeCount={}, paddingLen={}: {}"),
                    to_wstr(cand.string()), cand.strokeLen(), piece.strokeLen(), strokeCount, paddingLen, (bStrokeCountMatched ? L"MATCH" : L"UNMATCH"));
                if (isStrokeBS || bStrokeCountMatched) {
                    targetCandidates.push_back(cand);
                }
            }
            if (targetCandidates.empty()) {
                LOG_WARNH(_T("No target candidates matched.: piece.strokeLen()={}, strokeCount={}, paddingLen={}"), piece.strokeLen(), strokeCount, paddingLen);
                for (int i = 0; i < (int)_candidates.size() && i < SETTINGS->multiStreamBeamSize * 2; ++i) {
                    LOG_WARNH(_T("cand={}"), _candidates[i].debugString());
                }
            }
            if (targetCandidates.size() > 1) {
                // ターゲットの候補が複数ある場合は、形態素解析とNgram解析を行う
                _LOG_DETAIL(_T("targetCandidates more thran 1. use Morph and Ngram Analyzer."));
                useMorphAnalyzer = true;
            }

            // 素片のストロークと適合する候補の最小ストローク長を取得
            // minLenは、形態素解析やNgram解析の際に、長い候補文字列に対して共通の先頭部分を避けるために使用する
            size_t minLen = getMinimumCandidateLength(targetCandidates);
            std::map<int, int> representativeStrokeCounts;
            int recentKeepStrokeCount = std::max(0, SETTINGS->recentConnectionKeepStrokeCount);
            bool multiChoicePiece = isMultiChoicePiece(pieceStr);

            for (const auto& cand : targetCandidates) {
                _LOG_DETAIL(L"targetCand={}", cand.infoString());
                // 現在打鍵から近い接続元だけを代表候補の候補にする
                bool isRecentConnection = !piece.isAnyPadding() && !isStrokeBS && strokeCount - cand.strokeLen() <= recentKeepStrokeCount;
                bool isRepresentativeSource = false;
                if (isRecentConnection) {
                    // 同じ strokeLen からは、現在の並び順で先頭2件までを代表接続元として扱う
                    int& count = representativeStrokeCounts[cand.strokeLen()];
                    if (count < 2) {
                        isRepresentativeSource = true;
                        ++count;
                    }
                }
                // 素片のストロークと適合する候補
                int penalty = cand.penalty();
                bool bPaddingDerived = cand.isPaddingDerived() || piece.isExStrokePadding();
                if (bPaddingDerived) {
                    _LOG_DETAIL(L"PADDING piece({}) or derived({})", piece.isExStrokePadding(), cand.isPaddingDerived());
                }
                if (singleHitHighFreqJoshiCost > 0) {
                    // 複数ストロークによる入力で、2打鍵目がロールオーバーでなかったらペナルティ
                    penalty += singleHitHighFreqJoshiCost;
                    _LOG_DETAIL(L"Non rollover multi stroke penalty, total penalty={}", penalty);
                }

                // 漢字 xor ひらがな優先条件を満たしていなければスキップ
                if (!isKanjiOrHiraganaPreferenceSatisfied(cand, piece)) continue;

                bool representativeMarked = false;
                if (!bAutoBushuFound && !bPaddingDerived) {
                    MString s;
                    int numBS;
                    std::tie(s, numBS) = cand.applyAutoBushu(piece, strokeCount);  // 自動部首合成
                    if (!s.empty()) {
                        _LOG_DETAIL(_T("AutoBush FOUND"));
                        CandidateString newCandStr(s, strokeCount, cand.mazeFeat());
                        newCandStr.setPenalty(penalty);
                        newCandStr.setPaddingDerived(bPaddingDerived);
                        // ここで形態素解析やNgram解析をしてコストを計算し、末尾に追加する
                        // 素片が追加されたことになるので、強制的に形態素解析を行う
                        useMorphAnalyzer = true;
                        calcCandidateCost(newCandStr, minLen, useMorphAnalyzer, isStrokeBS);
                        _LOG_DETAIL(_T("add newCandStr={}"), newCandStr.debugString());
                        appendGeneratedCandidate(newCandidates, promotedFlags, newCandStr, isRepresentativeSource);
                        representativeMarked = isRepresentativeSource;
                        bAutoBushuFound = true;
                        if (!SETTINGS->multiCandidateMode) break;  // 複数候補モードでなければ、自動部首合成を見つけたら終了
                    }
                }
                // 複数文字指定などの解析も行う
                std::vector<MString> ss = cand.applyPiece(piece, strokeCount, paddingLen, isStrokeBS, bKatakanaConversion);
                int prevKanjiCandCost = INT_MIN;
                for (MString s : ss) {
                    CandidateString newCandStr(s, strokeCount, cand.mazeFeat());
                    newCandStr.setPenalty(penalty);
                    newCandStr.setPaddingDerived(bPaddingDerived);
                    newCandStr.setFollowingPreferenceType(prefType);
                    // ここで形態素解析やNgram解析をしてコストを計算し、末尾に追加する
                    //MString subStr = substringBetweenNonJapaneseChars(s);
                    int tailMorphLen = calcCandidateCost(newCandStr, minLen, useMorphAnalyzer, isStrokeBS);
                    if (tailMorphLen == 1 && isTailIsolatedKanji(s)) {
                        // 末尾が孤立した漢字なら、出現順で初期コストを加算する(「過|禍」で「禍」のほうが優先されて出力されることがあるため)
                        int cost = newCandStr.totalCost();
                        _LOG_DETAIL(_T("ISOLATED KANJI={}, cost={}, prevCost={}"), to_wstr(s), cost, prevKanjiCandCost);
                        if (prevKanjiCandCost == INT_MIN) {
                            prevKanjiCandCost = cost;
                        } else if (cost > prevKanjiCandCost) {
                            prevKanjiCandCost = cost;
                        } else {
                            newCandStr.addNgramCost(prevKanjiCandCost - cost + 1);
                            _LOG_DETAIL(_T("ngramCost adjusted. newCandStr.totalCost={}"), newCandStr.totalCost());
                        }
                    }
                    // 多値 piece では通常候補は全展開しつつ、代表候補としては先頭に対応する1件だけを前寄せ対象にする
                    bool promoteThisCandidate = isRepresentativeSource && !representativeMarked;
                    _LOG_DETAIL(_T("add newCandStr={}"), newCandStr.debugString());
                    appendGeneratedCandidate(newCandidates, promotedFlags, newCandStr, promoteThisCandidate);
                    if (promoteThisCandidate || multiChoicePiece) representativeMarked = true;
                }
                // pieceが確定文字の場合
                if (pieceStr.size() == 1 && lattice2::isCommitChar(pieceStr[0])) {
                    _LOG_DETAIL(_T("pieceStr[0] is commit char"));
                    // 先頭候補だけを残す
                    break;
                }

            }
            _LOG_DETAIL(_T("LEAVE: {}\n"), piece.debugString());
        }

        //// 指定の打鍵回数分、解の先頭部分が同じなら、それらだけを残す
        //void commitOnlyWithSameLeaderString() {
        //    int challengeNum = SETTINGS->challengeNumForSameLeader;
        //    if (challengeNum > 0 && !_candidates.empty()) {
        //        MString firstStr = _candidates.front().string();
        //        if (_bestStack.empty()) {
        //            _bestStack.push_back(firstStr);
        //        } else if (_bestStack.back() != firstStr) {
        //            _bestStack.push_back(firstStr);
        //            if ((int)_bestStack.size() > challengeNum) {
        //                int basePos = (int)_bestStack.size() - challengeNum - 1;
        //                auto baseStr = _bestStack[basePos];
        //                //if ((int)baseStr.size() >= SAME_LEADER_LEN) {
        //                //    bool sameFlag = true;
        //                //    for (int i = basePos + 1; sameFlag && i < (int)_bestStack.size(); ++i) {
        //                //        sameFlag = utils::startsWith(_bestStack[i], baseStr);
        //                //    }
        //                //    if (sameFlag) {
        //                //        _LOG_DETAIL(L"_bestStack.size={}, challengeNum={}, baseStr={}", _bestStack.size(), challengeNum, to_wstr(baseStr));
        //                //        std::vector<CandidateString> tempCands;
        //                //        auto iter = _candidates.begin();
        //                //        tempCands.push_back(*iter++);
        //                //        for (; iter != _candidates.end(); ++iter) {
        //                //            if (utils::startsWith(iter->string(), baseStr)) {
        //                //                // 先頭部分が一致する候補だけを残す
        //                //                tempCands.push_back(*iter);
        //                //            }
        //                //        }
        //                //        _candidates = std::move(tempCands);
        //                //    }
        //                //}
        //            }
        //        }
        //    }
        //}

        // CurrentStroke の候補を削除する
        void removeCurrentStrokeCandidates(std::vector<CandidateString>& newCandidates, int strokeCount, int removeLen) {
            _LOG_DETAIL(L"ENTER: _candidates.size={}, strokeCount={}, removeLen={}", _candidates.size(), strokeCount, removeLen);
            if (!_candidates.empty()) {
                const auto& firstCand = _candidates.front();
                int delta = removeLen + 1;
                for (const auto& cand : _candidates) {
                    if (cand.strokeLen() + delta <= strokeCount) {
                        if (cand.string() == firstCand.string()) {
                            // 先頭と同じ文字列の候補だったら、そのストローク数の他の候補も残さない
                            ++delta;
                            continue;
                        }
                        // 1ストローク以上前の候補を残す
                        CandidateString newCand(cand, delta);
                        newCand.clean();    // ペナルティとPaddingDerivedをクリアしておく
                        _LOG_DETAIL(L"add cand={}", newCand.debugString());
                        newCandidates.push_back(newCand);
                    }
                }
                if (!newCandidates.empty() && newCandidates.front().strokeLen() != strokeCount) {
                    // 一つ前のストローク候補が無くなっていたら、それ以下のものを繰り上げる
                    delta = strokeCount - newCandidates.front().strokeLen();
                    _LOG_DETAIL(L"raise candidates: frontCand.strokeLen={}, delta={}", newCandidates.front().strokeLen(), delta);
                    for (auto& cand : newCandidates) {
                        cand.addStrokeCount(delta);
                    }
                }
            }
            _LOG_DETAIL(L"LEAVE: {} candidates", newCandidates.size());
        }

        // 末尾文字を削除して残った文字列と同じ候補があれば、そのストローク長まで候候を削除する
        void removeLastCharCandidate(std::vector<CandidateString>& newCandidates, int strokeCount) {
            if (!_candidates.empty()) {
                int removeLen = 0;
                const auto& firstCand = _candidates.front();
                // str: 末尾文字を削除した文字列
                const MString str = utils::safe_substr(firstCand.string(), 0, -1);
                _LOG_DETAIL(L"_candidates.size={}, targetStr={}, origCand: {}", _candidates.size(), to_wstr(str), firstCand.infoString());
                for (const auto& cand : _candidates) {
                    _LOG_DETAIL(L"cand: {}", cand.infoString());
                    if (str == cand.string()) {
                        _LOG_DETAIL(L"FOUND same candidate: {}", cand.debugString());
                        removeLen = firstCand.strokeLen() - cand.strokeLen();
                        break;
                    }
                }
                if (removeLen > 0) {
                    // 末尾文字を削除したものと同じ候補が見つかったら、そのストローク長まで候補を削除する
                    // (英字⇒カタカナ変換などで、末尾文字を削除したものと同じ候補が見つからない場合は、何もしない→通常のBS動作となる)
                    removeCurrentStrokeCandidates(newCandidates, strokeCount, removeLen);
                }
            }
        }

        //// Strokeを一つ進める
        //void advanceStroke(std::vector<CandidateString>& newCandidates) {
        //    LOG_INFO(L"ENTER: _candidates.size={}", _candidates.size());
        //    for (const auto& cand : _candidates) {
        //        CandidateString newCand(cand, 1);
        //        _LOG_DETAIL(L"add cand={}", newCand.debugString());
        //        newCandidates.push_back(newCand);
        //    }
        //    LOG_INFO(L"LEAVE: {} candidates", newCandidates.size());
        //}

        // llama-loss の小さい順に候補を並べ直す
        //void sortByLlamaLoss(std::vector<CandidateString>& newCandidates) {
        //    std::for_each(newCandidates.begin(), newCandidates.end(), [this](CandidateString& c) {
        //        c.llama_loss(calcLlamaLoss(c.string()));
        //    });
        //    std::sort(newCandidates.begin(), newCandidates.end(), [](const CandidateString& a, const CandidateString& b) {
        //        return a.llama_loss() < b.llama_loss();
        //    });
        //}

        bool isKanjiKatakanaConsecutive(const CandidateString& cand) {
            MString str = cand.string();
            size_t len = str.size();
            return len >= 2 && (utils::is_kanji(str[len - 1]) && utils::is_kanji(str[len - 2]) || utils::is_katakana(str[len - 1]) && utils::is_katakana(str[len - 2]));
        }

        // return: strokeBack による戻しがあったら、先頭を優先する
        std::vector<CandidateString> _updateKBestList_sub(const std::vector<WordPiece>& pieces, FollowingPreferenceType prefType, bool useMorphAnalyzer, int strokeCount, int paddingLen, bool strokeBack, bool bKatakanaConversion) {
            _LOG_DETAIL(_T("ENTER: pieces.size={}, strokeCount={}, useMorphAnalyzer={}, strokeBack={}, paddingLen={}"),
                pieces.size(), strokeCount, useMorphAnalyzer, strokeBack, paddingLen);
            std::vector<CandidateString> newCandidates;
            std::vector<bool> promotedFlags;
            if (strokeBack) {
                // strokeBackの場合
                _LOG_DETAIL(L"strokeBack");
                //removeCurrentStrokeCandidates(newCandidates, strokeCount, 1);
                removeLastCharCandidate(newCandidates, strokeCount);
                if (!newCandidates.empty()) {
                    _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
                    return newCandidates;
                }
                // 以前のストロークの候補が無ければ、通常のBSの動作とする
                removeOtherThanFirst();
            }
            bool isPaddingPiece = pieces.size() == 1 && pieces.front().isAnyPadding();
            bool isBSpiece = pieces.size() == 1 && pieces.front().isBS();
            _LOG_DETAIL(_T("newCandidates.size={}, isPaddingPiece={}, isBSpiece={}"), newCandidates.size(), isPaddingPiece, isBSpiece);

            //if (isBSpiece) {
            //    // 末尾文字を削除して残った文字列と同じ候補があれば、そのストローク長まで候補を削除する
            //    removeLastCharCandidate(newCandidates, strokeCount);
            //    //removeCurrentStrokeCandidates(newCandidates, strokeCount, 1);
            //    if (!newCandidates.empty()) {
            //        _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
            //        return newCandidates;
            //    }
            //    //// 以前のストロークの候補が無ければ、通常のBSの動作とする
            //    //removeOtherThanFirst();
            //}
            //_prevBS = isBSpiece;

            if (!isPaddingPiece && !isBSpiece) {
                raiseAndLowerByCandSelection();
            }

            _LOG_DETAIL(_T("A: newCandidates.size={}"), newCandidates.size());
            // 適用する素片が複数ある場合、形態素解析を行う
            useMorphAnalyzer = useMorphAnalyzer || pieces.size() > 1;
            // BS でないか、以前の候補が無くなっていた
            for (const auto& piece : pieces) {
                // 素片のストロークと適合する候補だけを追加
                addOnePiece(newCandidates, promotedFlags, piece, prefType, useMorphAnalyzer, strokeCount, paddingLen, bKatakanaConversion);
            }
            if (!isPaddingPiece) {
                // ユーザーによるNgram選択をtotalCostに反映して、候補の順序を totalCost の昇順にソート
                reorderCandidates(newCandidates, promotedFlags);
                promoteRepresentativeCandidates(newCandidates, promotedFlags);
                _LOG_DETAIL(_T("B: newCandidates.size={}"), newCandidates.size());

                //rotateSameTailCandidates(newCandidates);
                truncateTailCandidates(newCandidates, promotedFlags);
            }
            _LOG_DETAIL(_T("C: newCandidates.size={}"), newCandidates.size());

            //sortByLlamaLoss(newCandidates);

            // 残りの _candidates のうち、stroke位置的に組み合せ不可だったものは、strokeCount が範囲内なら残しておく
            if (!isBSpiece /* && !_isAnyPadding(newCandidates)*/) {     // isAnyPadding()だったら、BSなどで先頭のものだけが残されたということ
                _LOG_DETAIL(_T("D: newCandidates.size={}, _candidates.size={}, remainingStrokeSize={}, strokeCount={}"),
                    newCandidates.size(), _candidates.size(), SETTINGS->remainingStrokeSize, strokeCount);
                for (const auto& cand : _candidates) {
                    if (cand.strokeLen() + SETTINGS->remainingStrokeSize > strokeCount) {
                        _LOG_DETAIL(_T("cand.str={}, cand.strokeLen={}"), to_wstr(cand.string()), cand.strokeLen());
                        newCandidates.push_back(cand);
                    }
                }
            }
            _LOG_DETAIL(_T("E: newCandidates.size={}"), newCandidates.size());
            if (!isPaddingPiece || isBSpiece) {
                // 新しく候補が作成された
                resetOrigFirstCand();
            }
            _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
            return newCandidates;
        }

        // 先頭の1ピースだけの挿入(1ピースが複数文字集合の場合、順序を変更しない)
        std::vector<CandidateString> _updateKBestList_initial(const CandidateString& dummyCand, const WordPiece& piece, FollowingPreferenceType prefType, int strokeCount, int paddingLen, bool bKatakanaConversion) {
            _LOG_DETAIL(_T("ENTER: dummyCand={}, piece={}, prefType={}, strokeCount={}, paddingLen={}"),
                dummyCand.debugString(), piece.debugString(), to_string(prefType), strokeCount, paddingLen);
            std::vector<CandidateString> newCandidates;
            if (isKanjiOrHiraganaPreferenceSatisfied(dummyCand, piece)) {
                // 漢字 xor ひらがな優先条件を満たしている
                bool bPaddingDerived = piece.isExStrokePadding();
                if (bPaddingDerived) _LOG_DETAIL(L"PADDING piece. set PADDING DERIVED");
                std::vector<MString> ss = dummyCand.applyPiece(piece, strokeCount, paddingLen, false, bKatakanaConversion);
                for (MString s : ss) {
                    CandidateString newCandStr(s, strokeCount);
                    //newCandStr.setPenalty(penalty);
                    newCandStr.setPaddingDerived(bPaddingDerived);
                    newCandStr.setFollowingPreferenceType(prefType);
                    _LOG_DETAIL(L"Add initial cand={}", newCandStr.debugString());
                    newCandidates.emplace_back(newCandStr);
                }
            }
            newCandidates.push_back(dummyCand);
            _LOG_DETAIL(_T("LEAVE: newCandidates.size()={}"), newCandidates.size());
            return newCandidates;
        }

    public:
        // currentStrokeCount: lattice に最初に addPieces() した時からの相対的なストローク数
        void updateKBestList(const std::vector<WordPiece>& pieces, FollowingPreferenceType prefType, bool useMorphAnalyzer, int currentStrokeCount, bool strokeBack, bool bKatakanaConversion) override {
            _LOG_DETAIL(_T("ENTER: _candidates.size()={}, pieces.size()={}, prefType={}, useMorphAnalyzer={}, currentStrokeCount={}, strokeBack={}"),
                _candidates.size(), pieces.size(), to_string(prefType), useMorphAnalyzer, currentStrokeCount, strokeBack);
            _debugLog.clear();
            bool bCandidateSelecting = _origFirstCand >= 0;

            setRollOverStroke(currentStrokeCount - 1, STATE_COMMON->IsRollOverStroke());

            // 候補リストが空の場合は、追加される piece と組み合わせるための、先頭を表すダミーを用意しておく
            addDummyCandidate();

            int paddingLen = 0;
            if (pieces.front().isAnyPadding()) {
                paddingLen = currentStrokeCount - _candidates.front().strokeLen();
                _LOG_DETAIL(_T("paddingLen={}"), paddingLen);
            }
            if (!strokeBack && _candidates.size() == 1 && _candidates.front().string().empty() && pieces.size() == 1) {
                // 先頭の1ピースだけの挿入(複数文字の場合、順序を変更しない)
                _candidates = std::move(_updateKBestList_initial(_candidates.front(), pieces.front(), prefType, currentStrokeCount, paddingLen, bKatakanaConversion));
            } else {
                _candidates = std::move(_updateKBestList_sub(pieces, prefType, useMorphAnalyzer, currentStrokeCount, paddingLen, strokeBack, bKatakanaConversion));
            }

            if (_candidates.empty()) {
                LOG_WARN(_T("KBestList is empty after update. Padding empty cand."));
                // 何らかの理由で候補が空になった場合は、先頭を表すダミーを用意しておく
                _candidates.push_back(CandidateString(EMPTY_MSTR, currentStrokeCount));
                _LOG_DETAIL(_T("padding empty cand={}"), _candidates.front().debugString());
            } else if (_candidates.front().strokeLen() < currentStrokeCount) {
                // 先頭候補のストローク長が currentStrokeCount (漢字後接なら +4ストローク) 未満なら padding する
                if (!_candidates.front().isKanjiFollowed() || _candidates.front().strokeLen() + 4 < currentStrokeCount) {
                    _candidates.push_back(CandidateString(_candidates.front(), currentStrokeCount - _candidates.front().strokeLen()));
                    _LOG_DETAIL(_T("padding cand={}"), _candidates.front().debugString());
                }
            }

            //// 漢字またはカタカナが2文字以上連続したら、その候補を優先する
            //if (!_candidates.empty()) {
            //    if (isKanjiKatakanaConsecutive(_candidates.front())) selectFirst();
            //}

            // ストローク戻しや候補選択中は、先頭候補の安定判定を引き継がない
            if (strokeBack || bCandidateSelecting) {
                resetFixedLeaderPrefixState();
            } else if (!_candidates.empty()) {
                // 通常更新時は先頭候補の安定度を更新し、固定済み prefix を持たない候補を抑制する
                updateFixedLeaderPrefixState(_candidates.front().string());
                applyFixedLeaderPrefixFilter();
            }
            _LOG_DETAIL(_T("LEAVE: _candidates.size()={}"), _candidates.size());
        }

    private:
        // 先頭候補以外に、非優先候補ペナルティを与える (先頭候補のペナルティは 0 にする)
        void arrangePenalties(size_t nSameLen) {
            //_candidates.front().clean();
            //for (size_t i = 1; i < nSameLen; ++i) {
            //    _candidates[i].penalty(NON_PREFERRED_PENALTY * (int)i);
            //}
            return lattice2::arrangePenalties(_candidates, nSameLen);
        }

    public:
        // 先頭を表すダミーを用意しておく
        void addDummyCandidate() override {
            if (_candidates.empty()) {
                _candidates.push_back(CandidateString());
            }
        }

        void setBaseString(const MString& base) override {
            _LOG_DETAIL(_T("ENTER: base={}"), to_wstr(base));
            clearKbests(true);
            if (!base.empty()) {
                _candidates.push_back(CandidateString(base, 0));
            }
            _LOG_DETAIL(_T("LEAVE: _candidates.size()={}"), _candidates.size());
        }

        // 先頭候補を取得する
        MString getFirst() const override {
            _LOG_DETAIL(_T("CALLED"));
            if (_candidates.empty()) return MString();
            return _candidates.front().string();
        }

        // 先頭候補を最優先候補にする
        void selectFirst() override {
            _LOG_DETAIL(_T("ENTER: _origFirstCand={}"), _origFirstCand);
            //size_t nSameLen = getNumOfSameStrokeLen();
            //if (nSameLen > 1) {
            //    arrangePenalties(nSameLen);
            //    LOG_INFO(_T("CALLED: First candidate preferred."));
            //}
            lattice2::selectFirst(_candidates);
            resetOrigFirstCand();
            _LOG_DETAIL(_T("LEAVE: _origFirstCand={}"), _origFirstCand);
        }

        // 次候補を選択する (一時的に優先するだけなので、他の候補を削除したりはしない)
        void selectNext() override {
            _LOG_DETAIL(_T("ENTER: _origFirstCand={}"), _origFirstCand);
            size_t nSameLen = getNumOfSameStrokeLen();
            _LOG_DETAIL(_T("candidates.size={}, nSameLen={}"), _candidates.size(), nSameLen);
            if (nSameLen > 1) {
                auto begin = _candidates.begin();
                std::rotate(begin, begin + 1, begin + nSameLen);
                arrangePenalties(nSameLen);
                decrementOrigFirstCand((int)nSameLen);
            }
            _LOG_DETAIL(_T("LEAVE: _origFirstCand={}, _selectedCandPos={}"), _origFirstCand, _selectedCandPos);
        }

        // 前候補を選択する (一時的に優先するだけなので、他の候補を削除したりはしない)
        void selectPrev() override {
            _LOG_DETAIL(_T("ENTER: _origFirstCand={}"), _origFirstCand);
            size_t nSameLen = getNumOfSameStrokeLen();
            _LOG_DETAIL(_T("candidates.size={}, nSameLen={}"), _candidates.size(), nSameLen);
            if (nSameLen > 1) {
                auto begin = _candidates.begin();
                std::rotate(begin, begin + nSameLen - 1, begin + nSameLen);
                arrangePenalties(nSameLen);
                incrementOrigFirstCand((int)nSameLen);
            }
            _LOG_DETAIL(_T("LEAVE: _origFirstCand={}, _selectedCandPos={}"), _origFirstCand, _selectedCandPos);
        }

    private:
        void insertCandidate(const CandidateString& cand) {
            _candidates.insert(_candidates.begin(), cand);
            size_t nSameLen = getNumOfSameStrokeLen();
            arrangePenalties(nSameLen);
        }

        //void insertCandidate(const MString& cand) {
        //    insertCandidate(CandidateString(cand, _candidates.front().strokeLen(), 0, 0));
        //}

        void updateByConversion(const MString& s) {
            if (!s.empty()) {
                selectFirst();
                insertCandidate(CandidateString(s, _candidates.front().strokeLen()));
            }
        }

        //void insertCandidatesReverse(const std::vector<CandidateString>& convs) {
        //    for (auto iter = convs.rbegin(); iter != convs.rend(); ++iter) {
        //        insertCandidate(*iter);
        //    }
        //}

    public:
        // 部首合成
        void updateByBushuComp() override {
            _LOG_DETAIL(_T("CALLED"));
            if (!_candidates.empty()) {
                updateByConversion(_candidates.front().applyBushuComp());
            }
        }

        // 交ぜ書き変換
        void updateByMazegaki() override {
            _LOG_DETAIL(_T("ENTER"));
            if (!_candidates.empty()) {
                raiseAndLowerByCandSelection();
                removeOtherThanFirst();
                CandidateString& srcCand = _candidates.front();
                // 交ぜ書き変換の実行
                _LOG_DETAIL(L"src: {}", to_wstr(srcCand.string()));
                // 交ぜ書き変換の実行 (ここで文字列の末尾が空白の場合、除去されてしまうので注意)
                auto canditates = srcCand.applyMazegaki();
                _LOG_DETAIL(L"applyMazegaki: RESULT candidates num={}", canditates.size());
                const MString& srcStr = utils::strip_tail(srcCand.string());
                MString srcStr2 = removeLastSpace(srcStr);  // 先頭が空白の形態素を変換すると空白が除去されるので、それも除外する
                _LOG_DETAIL(L"src: <{}>, src2=<{}>", to_wstr(srcStr), to_wstr(srcStr2));
                for (auto iter = canditates.rbegin(); iter != canditates.rend(); ++iter) {
                    _LOG_DETAIL(L"applyMazegaki: RESULT: {}", to_wstr(iter->string()));
                    if (iter->string() != srcStr && iter->string() != srcStr2) {
                        insertCandidate(*iter);
                    } else {
                        // 変換元と同一のものは除く
                        _LOG_DETAIL(L"applyMazegaki: SKIP same as source");
                    }
                }
                _LOG_DETAIL(L"LEAVE\nkBest:\n{}", debugCandidates(SETTINGS->multiStreamBeamSize));
            }
        }

    };
    DEFINE_CLASS_LOGGER(KBestListImpl);

    std::unique_ptr<KBestList> KBestList::createKBestList() {
        return std::make_unique<KBestListImpl>();
    }

} // namespace lattice2
