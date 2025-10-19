#pragma once

#include "string_utils.h"

#include "Lattice.h"

class RewriteInfo;
class PostRewriteOneShotNode;

namespace lattice2 {

    // グローバルな後置書き換えマップファイルの読み込み
    void readGlobalPostRewriteMapFile();

    // 候補文字列
    class CandidateString {
        DECLARE_CLASS_LOGGER;

        MString _str;
        int _strokeLen;
        int _morphCost;
        int _ngramCost;
        int _penalty;
        bool _isNonTerminal;
        MString _mazeFeat;
        //float _llama_loss = 0.0f;

        // 末尾文字列にマッチする RewriteInfo を取得する
        std::tuple<const RewriteInfo*, int> matchWithTailString(const PostRewriteOneShotNode* rewriteNode) const;

        // 複数文字が設定されているストロークを1文字ずつに分解
        std::vector<MString> split_piece_str(const MString& s) const;

        MString convertoToKatakanaIfAny(const MString& str, bool bKatakana) const;

        // グローバルな後置書き換えを適用する
        MString applyGlobalPostRewrite(const MString& adder) const;

    public:
        CandidateString()
            : _strokeLen(0), _morphCost(0), _ngramCost(0), _penalty(0), _isNonTerminal(false) {
        }

        CandidateString(const MString& s, int len)
            : _str(s), _strokeLen(len), _morphCost(0), _ngramCost(0), _penalty(0), _isNonTerminal(false) {
        }

        CandidateString(const MString& s, int len, const MString& mazeFeat)
            : _str(s), _strokeLen(len), _morphCost(0), _ngramCost(0), _penalty(0), _isNonTerminal(false), _mazeFeat(mazeFeat) {
        }

        CandidateString(const CandidateString& cand)
            : _str(cand._str), _strokeLen(cand._strokeLen), _morphCost(cand._morphCost), _ngramCost(cand._ngramCost), _penalty(cand._penalty), _isNonTerminal(cand._isNonTerminal), _mazeFeat(cand._mazeFeat) {
        }

        CandidateString(const CandidateString& cand, int strokeDelta)
            : _str(cand._str), _strokeLen(cand._strokeLen + strokeDelta), _morphCost(cand._morphCost), _ngramCost(cand._ngramCost), _penalty(cand._penalty), _isNonTerminal(false), _mazeFeat(cand._mazeFeat) {
        }

        // 自動部首合成の実行
        std::tuple<MString, int> applyAutoBushu(const WordPiece& piece, int strokeCount) const;

        // 部首合成の実行
        MString applyBushuComp() const;

        // 交ぜ書き変換の実行
        std::vector<CandidateString> applyMazegaki() const;

        void getDifficultMazeCands(const MString& surf, const MString& mazeCands, std::vector<MString>& result) const;

        CandidateString makeCandWithFeat(const std::vector<MString>& items, int strkLen) const;

        // 単語素片を末尾に適用してみる
        std::vector<MString> applyPiece(const WordPiece& piece, int strokeCount, int paddingLen, bool isStrokeBS, bool bKatakanaConversion) const;

        inline const MString& string() const {
            return _str;
        }

        inline const mchar_t tailChar() const {
            return _str.empty() ? 0 : _str.back();
        }

        inline const bool isKanjiTailChar() const {
            return utils::is_kanji(tailChar());
        }

        inline const int strokeLen() const {
            return _strokeLen;
        }

        inline void addStrokeCount(int delta) {
            _strokeLen += delta;
        }

        inline int totalCost() const {
            return _morphCost + _ngramCost + _penalty;
        }

        inline void addCost(int morphCost, int ngramCost) {
            _morphCost += morphCost;
            _ngramCost += ngramCost;
        }

        //inline void zeroCost() {
        //    _penalty = -_cost;
        //}

        inline int penalty() const {
            return _penalty;
        }

        inline void setPenalty(int penalty) {
            _penalty = penalty;
        }

        void zeroPenalty() {
            _penalty = 0;
        }

        inline bool isNonTerminal() const {
            return _isNonTerminal;
        }

        inline void setNonTerminal(bool flag = true) {
            _isNonTerminal = flag;
        }

        inline const MString& mazeFeat() const {
            return _mazeFeat;
        }

        //inline float llama_loss() const {
        //    return _llama_loss;
        //}

        //inline void llama_loss(float loss) {
        //    _llama_loss = loss;
        //}

        String debugString() const;
    };

} // namespace lattice2

