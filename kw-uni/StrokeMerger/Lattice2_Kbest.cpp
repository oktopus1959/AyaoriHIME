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

namespace lattice2 {
    DECLARE_LOGGER;     // defined in Lattice2.cpp

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

    // パディング候補に与えるペナルティ(ペナルティの中で最大値; 候補のCostがこれ以上なら、その候補に Padding Piece が含まれていることが分かるようにする)
    int PADDING_PENALTY = 100000000;

    //--------------------------------------------------------------------------------------
    inline bool isHighFreqJoshi(mchar_t mc) {
        return mc == L'が' || mc == L'を' || mc == L'に' || mc == L'の' || mc == L'で' || mc == L'は';
    }

    inline bool isCommitChar(mchar_t mch) {
        //return utils::is_punct_or_commit_char(mch) || utils::is_paren(mch);
        return !utils::is_japanese_char_except_nakaguro(mch) && !is_numeral((wchar_t)mch) && mch != L'　' && mch != ' ';
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

    // 先頭候補以外に、非優先候補ペナルティを与える (先頭候補のペナルティは 0 にする)
    void arrangePenalties(std::vector<CandidateString>& candidates, size_t nSameLen) {
        _LOG_DETAIL(_T("CALLED"));
        candidates.front().zeroPenalty();
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

    //--------------------------------------------------------------------------------------
    // K-best な文字列を格納する
    class KBestListImpl : public KBestList {
        DECLARE_CLASS_LOGGER;

        std::vector<CandidateString> _candidates;

        std::vector<MString> _bestStack;

        //bool _prevBS = false;

        // 次候補/前候補選択された時の元の先頭候補位置 (-1なら候補選択されていないことを示す)
        int _origFirstCand = -1;

        // 次候補/前候補選択された時の選択候補位置
        int _selectedCandPos = 0;

        // 末尾漢字などの追加で一時的に拡大された候補数
        //int _extendedCandNum = 0;

        // 次のストロークをスキップする候補文字列
        std::set<MString> _kanjiPreferredNextCands;

        static bool _isEmpty(const std::vector<CandidateString> cands) {
            //return cands.empty() || cands.size() == 1 && cands.front().string().empty();
            return cands.empty() || cands.front().string().empty();
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

    public:
        //void setPrevBS(bool flag) override {
        //    _prevBS = flag;
        //}

        //bool isPrevBS() const override {
        //    return _prevBS;
        //}

        // 候補選択による、リアルタイムNgramの蒿上げと抑制
        void raiseAndLowerByCandSelection() override {
            if (_origFirstCand > 0 && (size_t)_origFirstCand < _candidates.size()) {
                _LOG_DETAIL(L"ENTER");
                // 候補選択がなされていて、元の先頭候補以外が選択された
                updateRealtimeNgramForDiffPart(_candidates[_origFirstCand].string(), _candidates[0].string());
                updateMazegakiPreference(_candidates[0], _candidates[_origFirstCand]);
                removeOtherThanFirstForEachStroke();
                _origFirstCand = -1;
                _LOG_DETAIL(L"LEAVE: CAND_SELECT:\nkBest:\n{}", debugCandidates(10));
            }
        }

        void clearKbests(bool clearAll) override {
            //LOG_INFO(L"CALLED: clearAll={}", clearAll);
            _candidates.clear();
            _bestStack.clear();
            _highFreqJoshiStroke.clear();
            _rollOverStroke.clear();
            _origFirstCand = -1;
            //_extendedCandNum = 0;
            if (clearAll) clearKanjiPreferredNextCands();
        }

        void removeOtherThanKBest() override {
            if ((int)_candidates.size() > SETTINGS->multiStreamBeamSize) {
                _candidates.erase(_candidates.begin() + SETTINGS->multiStreamBeamSize, _candidates.end());
            }
        }

        void removeOtherThanFirst() override {
            if (_candidates.size() > 0) {
                _candidates.erase(_candidates.begin() + 1, _candidates.end());
                _candidates.front().zeroPenalty();
            }
        }

        void removeOtherThanFirstForEachStroke() override {
            if (_candidates.size() > 0) {
                const MString& first = _candidates[0].string();
                size_t n = 1;
                while (n < _candidates.size()) {
                    if (!utils::startsWith(first, _candidates[n].string())) {
                        _candidates.erase(_candidates.begin() + n);
                    } else {
                        _candidates[n].zeroPenalty();
                        ++n;
                    }
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

        // 選択位置を含む、同一ストローク長の候補ブロック(最大10件)を返す
        std::vector<MString> getCandStringsInSelectedBlock() const override {
            _LOG_DETAIL(L"ENTER: _candidates.size={}, _origFirstCand={}, _selectedCandPos={}", _candidates.size(), _origFirstCand, _selectedCandPos);
            std::vector<MString> result;
            // ブロックサイズ (最初は最小数, 選択開始後は最大数)
            const int BLOCK_SIZE = selectedCandPos() < 0 ? SETTINGS->mergerCandidateMin : SETTINGS->mergerCandidateMax;
            int nSameLen = getNumOfSameStrokeLen();
            if (nSameLen <= 0 || BLOCK_SIZE <= 0) {
                _LOG_DETAIL(L"LEAVE: nSameLen<=0");
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

            _LOG_DETAIL(L"LEAVE: nSameLen={}, first={}, selPos={}, blockStart={}, blockEnd={}, result.size()={}",
                nSameLen, first, selPos, blockStart, blockEnd, result.size());
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
            String result = L"kanjiPreferredNextCands=" + kanjiPreferredNextCandsDebug() + L"\n\n";
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

        // 新しい候補を追加
        // targetStr は日本語としての妥当性をチェックするための文字列 (句読点や記号を除いたもの)
        bool addCandidate(std::vector<CandidateString>& newCandidates, CandidateString& newCandStr, const MString& targetStr, bool isStrokeBS) {
            _LOG_DETAIL(_T("\nENTER: newCandStr={}, isStrokeBS={}"), newCandStr.debugString(), isStrokeBS);
            bool bAdded = false;
            //bool bIgnored = false;
            const MString& candStr = newCandStr.string();
            //MString targetStr = substringBetweenPunctuations(candStr);
            //MString targetStr = substringBetweenNonJapaneseChars(candStr);
            _LOG_DETAIL(_T("targetStr={}"), to_wstr(targetStr));

            std::vector<MString> morphs;

            // 形態素解析コスト
            // 1文字以下なら、形態素解析しない(過|禍」で「禍」のほうが優先されて出力されることがあるため（「禍」のほうが単語コストが低いため）)
            //int morphCost = !SETTINGS->useMorphAnalyzer || targetStr.size() <= 1 ? 5000 : calcMorphCost(targetStr, morphs);
            int morphCost = !SETTINGS->useMorphAnalyzer || targetStr.empty() ? 0 : calcMorphCost(targetStr, morphs);
            if (targetStr.size() == 1) {
                if (utils::is_katakana(targetStr[0])) morphCost += 5000; // 1文字カタカナならさらに上乗せ
            }
            if (!morphs.empty() && isNonTerminalMorph(morphs.back())) {
                _LOG_DETAIL(_T("NON TERMINAL morph={}"), to_wstr(morphs.back()));
                newCandStr.setNonTerminal();
            }

            // Ngramコスト
            int ngramCost = targetStr.empty() ? 0 : getNgramCost(targetStr) * SETTINGS->ngramCostFactor;
            //int morphCost = 0;
            //int ngramCost = candStr.empty() ? 0 : getNgramCost(candStr);
            //int llamaCost = candStr.empty() ? 0 : calcLlamaCost(candStr) * SETTINGS->ngramCostFactor;

            // llamaコスト
            int llamaCost = 0;

            // 総コスト
            newCandStr.addCost(morphCost, ngramCost);

            //// 「漢字+の+漢字」のような場合はペナルティを解除
            //size_t len = candStr.size();
            //if (len >= 3 && candStr[len - 2] == L'の' && !utils::is_hiragana(candStr[len - 3]) && !utils::is_hiragana(candStr[len - 1])) {
            //    newCandStr.zeroPenalty();
            //}

            int totalCost = newCandStr.totalCost();

            int candCost = morphCost + ngramCost + llamaCost;
            _LOG_DETAIL(_T("CALC: candStr={}, totalCost={}, candCost={} (morph={}[<{}>], ngram={})"),
                to_wstr(candStr), totalCost, candCost, morphCost, utils::reReplace(to_wstr(utils::join(morphs, to_mstr(L"> <"))), L"\t", L" "), ngramCost);

            if (IS_LOG_DEBUGH_ENABLED) {
                if (!isStrokeBS) _debugLog.append(std::format(L"candStr={}, totalCost={}, candCost={} (morph={} [<{}>] , ngram = {})\n",
                    to_wstr(candStr), totalCost, candCost, morphCost, utils::reReplace(to_wstr(utils::join(morphs, to_mstr(L"> <"))), L"\t", L" "), ngramCost));
            }

            if (!newCandidates.empty()) {
                for (auto iter = newCandidates.begin(); iter != newCandidates.end(); ++iter) {
                    int otherCost = iter->totalCost();
                    //LOG_DEBUGH(_T("    otherStr={}, otherCost={}"), to_wstr(iter->string()), otherCost);
                    if (totalCost < otherCost) {
                        iter = newCandidates.insert(iter, newCandStr);    // iter は挿入したノードを指す
                        bAdded = true;
                        // TODO LastSameLen は使わない
                        //// 下位のノードで末尾文字列の共通部分が指定の長さより長いものを探し、あればそれを削除
                        //for (++iter; iter != newCandidates.end(); ++iter) {
                        //    if (hasLongerCommonSuffixThanOrSameStr(candStr, iter->string(), LastSameLen)) {
                        //        // 末尾文字列の共通部分が指定の長さより長いか、同じ文字列
                        //        newCandidates.erase(iter);
                        //        LOG_DEBUGH(_T("    REMOVE second best or lesser candidate"));
                        //        break;
                        //    }
                        //}
                        break;
                    //} else if (hasLongerCommonSuffixThanOrSameStr(candStr, iter->string(), LastSameLen)) {
                    //    // 末尾文字列の共通部分が指定の長さより長いか、同じ文字列
                    //    bIgnored = true;
                    //    break;
                    }
                }
            }
#if 0
            int beamSize = SETTINGS->multiStreamBeamSize + (newCandStr.isKanjiTailChar() ? EXTRA_KANJI_BEAM_SIZE : 0);
            if (beamSize < _extendedCandNum) beamSize = _extendedCandNum;
            if (!bAdded && /*!bIgnored &&*/ (int)newCandidates.size() < beamSize) {
                // 余裕があれば末尾に追加(複数漢字候補の場合はkBestを超えても追加)
                newCandidates.push_back(newCandStr);
                bAdded = true;
            }
            if ((int)newCandidates.size() > SETTINGS->multiStreamBeamSize) {
                // kBestサイズを超えたら末尾を削除
#if 0
                newCandidates.resize(SETTINGS->multiStreamBeamSize);
                _LOG_DETAIL(_T("    REMOVE OVERFLOW ENTRY"));
#else
                size_t pos = SETTINGS->multiStreamBeamSize;
                while (pos < newCandidates.size() && pos < (size_t)(SETTINGS->multiStreamBeamSize + EXTRA_KANJI_BEAM_SIZE)) {
                    auto iter = newCandidates.begin() + pos;
                    if (pos >= (size_t)_extendedCandNum && !iter->isKanjiTailChar()) {
                        // 末尾が漢字以外のものだけを削除対象とする
                        newCandidates.erase(iter);
                        _LOG_DETAIL(_T("    REMOVE OVERFLOW ENTRY"));
                    } else {
                        ++pos;
                    }
                }
                if (pos < newCandidates.size()) {
                    newCandidates.resize(pos);
                }
#endif
            }
#else
            if (!bAdded) {
                // 末尾に追加
                newCandidates.push_back(newCandStr);
                bAdded = true;
            }
#endif
            if (bAdded) {
                _LOG_DETAIL(_T("    ADD candidate: {}, totalCost={}"), to_wstr(candStr), totalCost);
            } else {
                _LOG_DETAIL(_T("    ABANDON candidate: {}, totalCost={}"), to_wstr(candStr), totalCost);
            }
            _LOG_DETAIL(_T("LEAVE: {}, newCandidates.size={}"), bAdded ? L"ADD" : L"ABANDON", newCandidates.size());
            return bAdded;
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
        // Padding piece を含まない候補が見つかったら、Padding piece を含む候補は削除する
        void truncateTailCandidates(std::vector<CandidateString>& newCandidates) {
            _LOG_DETAIL(_T("ENTER: newCandidates.size={}"), newCandidates.size());
            if (IS_LOG_DEBUGH_ENABLED) {
                _debugLog.append(std::format(L"\ntruncateTailCandidates: ENTER: newCandidates.size={}\n", newCandidates.size()));
            }
            std::set<MString> triGrams;
            std::set<MString> biGrams;
            std::set<MString> uniGrams;
            const size_t beamSize = SETTINGS->multiStreamBeamSize;
            const size_t beamSize2 = beamSize + (int)(beamSize * SETTINGS->extraBeamSizeRate);
            bool bNonPaddingFound = false;  // Padding piece を含まない候補が見つかったか
            size_t pos = 0;
            while (pos < newCandidates.size()) {
                auto iter = newCandidates.begin() + pos;
                const MString& str = iter->string();
                const MString uni = str.size() >= 1 ? utils::safe_tailstr(str, 1) : EMPTY_MSTR;
                const MString bi = str.size() >= 2 ? utils::safe_tailstr(str, 2) : EMPTY_MSTR;
                const MString tri = str.size() >= 3 ? utils::safe_tailstr(str, 3) : EMPTY_MSTR;
                if (iter->penalty() < PADDING_PENALTY) bNonPaddingFound = true;
                if (!bNonPaddingFound || iter->penalty() < PADDING_PENALTY) {
                    if (pos < beamSize) {
                        if (!uni.empty()) uniGrams.insert(uni);
                        if (!bi.empty()) biGrams.insert(bi);
                        if (!tri.empty()) triGrams.insert(tri);
                        _LOG_DETAIL(_T("[{}] string={}: OK"), pos, to_wstr(str));
                        ++pos;
                        continue;
                    } else {
                        if (IS_LOG_DEBUGH_ENABLED && pos == beamSize) {
                            _LOG_DETAIL(_T("uniGrams={}"), to_wstr(utils::join(uniGrams, ',')));
                            _LOG_DETAIL(_T("biGrams={}"), to_wstr(utils::join(biGrams, ',')));
                            _LOG_DETAIL(_T("triGrams={}"), to_wstr(utils::join(triGrams, ',')));
                        }
                        if (iter->isNonTerminal()) {
                            // 非終端は残す
                            _LOG_DETAIL(_T("REMAIN: non terminal: {}"), to_wstr(str));
                            ++pos;
                            continue;
                        }
                        if (!tri.empty()) {
                            if (triGrams.size() < beamSize2 && triGrams.find(tri) == triGrams.end()) {
                                // 未見のtrigramであり、まだ余裕がある
                                triGrams.insert(tri);
                                _LOG_DETAIL(_T("[{}] string={}: triGram OK, triGrams.size={}"), pos, to_wstr(str), triGrams.size());
                                ++pos;
                                continue;
                            }
                        }
                        if (!bi.empty()) {
                            if (biGrams.size() < beamSize2 && biGrams.find(bi) == biGrams.end()) {
                                // 未見のbigramであり、まだ余裕がある
                                biGrams.insert(bi);
                                _LOG_DETAIL(_T("[{}] string={}: biGram OK, biGrams.size={}"), pos, to_wstr(str), biGrams.size());
                                ++pos;
                                continue;
                            }
                        }
                        if (!uni.empty()) {
                            if (uniGrams.size() < beamSize2 && uniGrams.find(uni) == uniGrams.end()) {
                                // 未見のunigramであり、まだ余裕がある
                                uniGrams.insert(uni);
                                _LOG_DETAIL(_T("[{}] string={}: uniGram OK, uniGrams.size={}"), pos, to_wstr(str), uniGrams.size());
                                ++pos;
                                continue;
                            }
                        }
                    }
                }
                _LOG_DETAIL(_T("[{}] string={}: erased"), pos, to_wstr(str));
                newCandidates.erase(iter);
            }
            if (pos < newCandidates.size()) {
                newCandidates.resize(pos);
            }
            _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
            if (IS_LOG_DEBUGH_ENABLED) {
                _debugLog.append(std::format(L"truncateTailCandidates: LEAVE: newCandidates.size={}\n\n", newCandidates.size()));
            }
            if (newCandidates.size() > maxCandidatesSize) {
                maxCandidatesSize = newCandidates.size();
                LOG_WARNH(_T("max newCandidates.size updated: {}"), maxCandidatesSize);
            }
        }

    public:
        void setKanjiPreferredNextCands() override {
            //LOG_INFO(L"ENTER");
            _kanjiPreferredNextCands.clear();
            if (!_candidates.empty()) {
                int topStrokeCnt = _candidates.front().strokeLen();
                for (const auto& c : _candidates) {
                    if (c.strokeLen() != topStrokeCnt) break;
                    _kanjiPreferredNextCands.insert(c.string());
                }
            }
            if (_kanjiPreferredNextCands.empty()) _kanjiPreferredNextCands.insert(EMPTY_MSTR);
            //LOG_INFO(L"LEAVE: kanjiPreferredNextCands={}", kanjiPreferredNextCandsDebug());
        }

        void clearKanjiPreferredNextCands() override {
            _LOG_DETAIL(_T("CALLED: _kanjiPreferredNextCands={}"), kanjiPreferredNextCandsDebug());
            _kanjiPreferredNextCands.clear();
        }

        String kanjiPreferredNextCandsDebug() const override {
            return std::to_wstring(_kanjiPreferredNextCands.size()) + L":['" + to_wstr(utils::join(_kanjiPreferredNextCands, L"', '")) + L"']";
        }

    private:
        int getKanjiPrefferredPenalty(const CandidateString& cand, const WordPiece& piece) {
            bool contained = _kanjiPreferredNextCands.contains(cand.string());
            _LOG_DETAIL(L"cand.string()=\"{}\", {} in kanjiPreferred={}", to_wstr(cand.string()), contained ? L"CONTAINED" : L"NOT", kanjiPreferredNextCandsDebug());
            int penalty = 0;
            const MString& pieceStr = piece.getString();
            if (contained
                && piece.numBS() <= 0 && !pieceStr.empty()
                && (piece.strokeLen() == 1 || utils::is_hiragana(pieceStr[0]))) {
                // 漢字優先
                _LOG_DETAIL(_T("add NON_PREFERRED_PENALTY"));
                penalty += NON_PREFERRED_PENALTY;
            }
            _LOG_DETAIL(_T("LEAVE: penalty={}"), penalty);
            return penalty;
        }

        // 素片のストロークと適合する候補だけを追加
        void addOnePiece(std::vector<CandidateString>& newCandidates, const WordPiece& piece, int strokeCount, int paddingLen, bool bKatakanaConversion) {
            _LOG_DETAIL(_T("ENTER: _candidates.size={}, piece={}"), _candidates.size(), piece.debugString());
            bool bAutoBushuFound = false;           // 自動部首合成は一回だけ実行する
            bool isStrokeBS = piece.numBS() > 0;
            const MString& pieceStr = piece.getString();
            //int topStrokeLen = -1;

            LOG_DEBUGH(L"kanjiPreferredNextCands={}", kanjiPreferredNextCandsDebug());

            if (pieceStr.size() == 1) setHighFreqJoshiStroke(strokeCount, pieceStr[0]);

            int singleHitHighFreqJoshiCost = piece.strokeLen() > 1 && isSingleHitHighFreqJoshi(strokeCount - (piece.strokeLen() - 1)) ? SINGLE_HIT_HIGH_FREQ_JOSHI_KANJI_COST : 0;

            for (const auto& cand : _candidates) {
                //if (topStrokeLen < 0) topStrokeLen = cand.strokeLen();
                bool bStrokeCountMatched = (piece.isPadding() && cand.strokeLen() + paddingLen == strokeCount) || (cand.strokeLen() + piece.strokeLen() == strokeCount);
                _LOG_DETAIL(_T("cand.strokeLen={}, piece.strokeLen()={}, strokeCount={}, paddingLen={}: {}"),
                    cand.strokeLen(), piece.strokeLen(), strokeCount, paddingLen, (bStrokeCountMatched ? L"MATCH" : L"UNMATCH"));
                if (isStrokeBS || bStrokeCountMatched) {
                    // 素片のストロークと適合する候補
                    int penalty = cand.penalty();
                    if (piece.isPadding()) {
                        // パディング素片の場合はペナルティを加算
                        penalty += PADDING_PENALTY;
                        _LOG_DETAIL(L"Padding piece penalty, total penalty={}", penalty);
                    }
                    if (singleHitHighFreqJoshiCost > 0) {
                        // 複数ストロークによる入力で、2打鍵目がロールオーバーでなかったらペナルティ
                        penalty += singleHitHighFreqJoshiCost;
                        _LOG_DETAIL(L"Non rollover multi stroke penalty, total penalty={}", penalty);
                    }

                    // 漢字優先か
                    int kanjiPrefPenalty = getKanjiPrefferredPenalty(cand, piece);
                    if (kanjiPrefPenalty > 0) continue;

                    penalty += kanjiPrefPenalty;

                    if (!bAutoBushuFound) {
                        MString s;
                        int numBS;
                        std::tie(s, numBS) = cand.applyAutoBushu(piece, strokeCount);  // 自動部首合成
                        if (!s.empty()) {
                            _LOG_DETAIL(_T("AutoBush FOUND"));
                            CandidateString newCandStr(s, strokeCount, cand.mazeFeat());
                            newCandStr.setPenalty(penalty);
                            // ここで形態素解析やNgram解析をしてコストを計算し、適切な位置に挿入する
                            addCandidate(newCandidates, newCandStr, s, isStrokeBS);
                            bAutoBushuFound = true;
                        }
                    }
                    // 複数文字指定などの解析も行う
                    std::vector<MString> ss = cand.applyPiece(piece, strokeCount, paddingLen, isStrokeBS, bKatakanaConversion);
                    int idx = 0;
                    for (MString s : ss) {
                        CandidateString newCandStr(s, strokeCount, cand.mazeFeat());
                        newCandStr.setPenalty(penalty);
                        if (isTailIsolatedKanji(s)) {
                            // 末尾が孤立した漢字なら、出現順で初期コストを加算する(「過|禍」で「禍」のほうが優先されて出力されることがあるため)
                            newCandStr.addCost(0, HEAD_SINGLE_CHAR_ORDER_COST * idx);
                            ++idx;
                        }
                        // ここで形態素解析やNgram解析をしてコストを計算し、適切な位置に挿入する
                        MString subStr = substringBetweenNonJapaneseChars(s);
                        addCandidate(newCandidates, newCandStr, subStr, isStrokeBS);
                    }
                    // pieceが確定文字の場合
                    if (pieceStr.size() == 1 && lattice2::isCommitChar(pieceStr[0])) {
                        _LOG_DETAIL(_T("pieceStr[0] is commit char"));
                        // 先頭候補だけを残す
                        break;
                    }

                }
            }
            _LOG_DETAIL(_T("LEAVE"));
        }

        // 指定の打鍵回数分、解の先頭部分が同じなら、それらだけを残す
        void commitOnlyWithSameLeaderString() {
            int challengeNum = SETTINGS->challengeNumForSameLeader;
            if (challengeNum > 0 && !_candidates.empty()) {
                MString firstStr = _candidates.front().string();
                if (_bestStack.empty()) {
                    _bestStack.push_back(firstStr);
                } else if (_bestStack.back() != firstStr) {
                    _bestStack.push_back(firstStr);
                    if ((int)_bestStack.size() > challengeNum) {
                        int basePos = (int)_bestStack.size() - challengeNum - 1;
                        auto baseStr = _bestStack[basePos];
                        //if ((int)baseStr.size() >= SAME_LEADER_LEN) {
                        //    bool sameFlag = true;
                        //    for (int i = basePos + 1; sameFlag && i < (int)_bestStack.size(); ++i) {
                        //        sameFlag = utils::startsWith(_bestStack[i], baseStr);
                        //    }
                        //    if (sameFlag) {
                        //        _LOG_DETAIL(L"_bestStack.size={}, challengeNum={}, baseStr={}", _bestStack.size(), challengeNum, to_wstr(baseStr));
                        //        std::vector<CandidateString> tempCands;
                        //        auto iter = _candidates.begin();
                        //        tempCands.push_back(*iter++);
                        //        for (; iter != _candidates.end(); ++iter) {
                        //            if (utils::startsWith(iter->string(), baseStr)) {
                        //                // 先頭部分が一致する候補だけを残す
                        //                tempCands.push_back(*iter);
                        //            }
                        //        }
                        //        _candidates = std::move(tempCands);
                        //    }
                        //}
                    }
                }
            }
        }

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
                _LOG_DETAIL(L"remove last char candidate: {}", to_wstr(_candidates.front().string()));
                const MString str = utils::safe_substr(_candidates.front().string(), 0, -1);
                for (const auto& cand : _candidates) {
                    if (str == cand.string()) {
                        _LOG_DETAIL(L"FOUND same candidate: {}", cand.debugString());
                        removeLen = firstCand.strokeLen() - cand.strokeLen();
                        break;
                    }
                }
                if (removeLen <= 0) removeLen = 1;
                removeCurrentStrokeCandidates(newCandidates, strokeCount, removeLen);
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
        std::vector<CandidateString> _updateKBestList_sub(const std::vector<WordPiece>& pieces, int strokeCount, int paddingLen, bool strokeBack, bool bKatakanaConversion) {
            _LOG_DETAIL(_T("ENTER: pieces.size={}, strokeCount={}, strokeBack={}, paddingLen={}"), pieces.size(), strokeCount, strokeBack, paddingLen);
            std::vector<CandidateString> newCandidates;
            if (strokeBack) {
                _LOG_DETAIL(_T("strokeBack"));
                // strokeBackの場合
                //_LOG_DETAIL(L"strokeBack");
                removeCurrentStrokeCandidates(newCandidates, strokeCount, 1);
                if (!newCandidates.empty()) {
                    _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
                    return newCandidates;
                }
                // 以前のストロークの候補が無ければ、通常のBSの動作とする
                removeOtherThanFirst();
            }
            bool isPaddingPiece = pieces.size() == 1 && pieces.front().isPadding();
            bool isBSpiece = pieces.size() == 1 && pieces.front().isBS();
            _LOG_DETAIL(_T("isPaddingPiece={}, isBSpiece={}"), isPaddingPiece, isBSpiece);

            //if (isBSpiece) {
            //    // 末尾文字を削除して残った文字列と同じ候補があれば、そのストローク長まで候補を削除する
            //    //removeLastCharCandidate(newCandidates, strokeCount);
            //    removeCurrentStrokeCandidates(newCandidates, strokeCount, 1);
            //    if (!newCandidates.empty()) {
            //        _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
            //        return newCandidates;
            //    }
            //    // 以前のストロークの候補が無ければ、通常のBSの動作とする
            //    removeOtherThanFirst();
            //}
            //_prevBS = isBSpiece;

            if (!isPaddingPiece && !isBSpiece) {
                raiseAndLowerByCandSelection();
            }

            // BS でないか、以前の候補が無くなっていた
            for (const auto& piece : pieces) {
                // 素片のストロークと適合する候補だけを追加
                addOnePiece(newCandidates, piece, strokeCount, paddingLen, bKatakanaConversion);
            }

            if (!isPaddingPiece) {
                //rotateSameTailCandidates(newCandidates);
                truncateTailCandidates(newCandidates);
            }

            //sortByLlamaLoss(newCandidates);

            // stroke位置的に組み合せ不可だったものは、strokeCount が範囲内なら残しておく
            if (!isBSpiece /* && !_isPadding(newCandidates)*/) {     // isPadding()だったら、BSなどで先頭のものだけが残されたということ
                for (const auto& cand : _candidates) {
                    if (cand.strokeLen() + SETTINGS->remainingStrokeSize > strokeCount) {
                        newCandidates.push_back(cand);
                    }
                }
            }
            if (!isPaddingPiece || isBSpiece) {
                // 新しく候補が作成された
                resetOrigFirstCand();
            }
            _LOG_DETAIL(_T("LEAVE: newCandidates.size={}"), newCandidates.size());
            return newCandidates;
        }

        // 先頭の1ピースだけの挿入(複数文字の場合、順序を変更しない)
        std::vector<CandidateString> _updateKBestList_initial(const CandidateString& dummyCand, const WordPiece& piece, int strokeCount, int paddingLen, bool bKatakanaConversion) {
            _LOG_DETAIL(_T("ENTER: dummyCand.string()=\"{}\", piece.string()={}, strokeCount={}, paddingLen={}"),
                to_wstr(dummyCand.string()), to_wstr(piece.getString()), strokeCount, paddingLen);
            std::vector<CandidateString> newCandidates;
            int penalty = getKanjiPrefferredPenalty(dummyCand, piece);
            if (penalty == 0) {
                if (piece.isPadding()) {
                    // パディング素片の場合はペナルティを加算
                    penalty += PADDING_PENALTY;
                    _LOG_DETAIL(L"Padding piece penalty, total penalty={}", penalty);
                }
                std::vector<MString> ss = dummyCand.applyPiece(piece, strokeCount, paddingLen, false, bKatakanaConversion);
                for (MString s : ss) {
                    CandidateString newCandStr(s, strokeCount);
                    newCandStr.setPenalty(penalty);
                    newCandidates.push_back(newCandStr);
                }
            }
            newCandidates.push_back(dummyCand);
            _LOG_DETAIL(_T("LEAVE: newCandidates.size()={}"), newCandidates.size());
            return newCandidates;
        }

    public:
        // strokeCount: lattice に最初に addPieces() した時からの相対的なストローク数
        void updateKBestList(const std::vector<WordPiece>& pieces, int strokeCount, bool strokeBack, bool bKatakanaConversion) override {
            _LOG_DETAIL(_T("ENTER: _candidates.size()={}, pieces.size()={}, strokeCount={}, strokeBack={}"), _candidates.size(), pieces.size(), strokeCount, strokeBack);
            _debugLog.clear();

            setRollOverStroke(strokeCount - 1, STATE_COMMON->IsRollOverStroke());

            // 候補リストが空の場合は、追加される piece と組み合わせるための、先頭を表すダミーを用意しておく
            addDummyCandidate();

            int paddingLen = 0;
            if (pieces.front().isPadding()) {
                paddingLen = strokeCount - _candidates.front().strokeLen();
                _LOG_DETAIL(_T("paddingLen={}"), paddingLen);
            }
            if (!strokeBack && _candidates.size() == 1 && _candidates.front().string().empty() && pieces.size() == 1) {
                // 先頭の1ピースだけの挿入(複数文字の場合、順序を変更しない)
                _candidates = std::move(_updateKBestList_initial(_candidates.front(), pieces.front(), strokeCount, paddingLen, bKatakanaConversion));
            } else {
                _candidates = std::move(_updateKBestList_sub(pieces, strokeCount, paddingLen, strokeBack, bKatakanaConversion));
            }

            //// 漢字またはカタカナが2文字以上連続したら、その候補を優先する
            //if (!_candidates.empty()) {
            //    if (isKanjiKatakanaConsecutive(_candidates.front())) selectFirst();
            //}
            // 指定の打鍵回数分、解の先頭部分が同じなら、それらだけを残す (ただし、候補の選択状態でない場合)
            if (_origFirstCand < 0) commitOnlyWithSameLeaderString();
            _LOG_DETAIL(_T("LEAVE: _candidates.size()={}"), _candidates.size());
        }

    private:
        // 先頭候補以外に、非優先候補ペナルティを与える (先頭候補のペナルティは 0 にする)
        void arrangePenalties(size_t nSameLen) {
            //_candidates.front().zeroPenalty();
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
                removeOtherThanFirst();
                const CandidateString& firstCand = _candidates.front();
                auto canditates = firstCand.applyMazegaki();
                MString firstStr = firstCand.string();
                for (auto iter = canditates.rbegin(); iter != canditates.rend(); ++iter) {
                    if (iter->string() != firstStr) {
                        // 変換元と同一のものは除く
                        insertCandidate(*iter);
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
