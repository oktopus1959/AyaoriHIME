#pragma once

#include "string_utils.h"

#include "Constants.h"
#include "FunctionNode.h"
#include "OneShot/PostRewriteOneShot.h"
#include "Logger.h"

// -------------------------------------------------------------------
// 単語素片と、それの出力にかかった打鍵数
class WordPiece {
    // 打鍵数
    int _strokeLen;
    // 書き換えノード
    const PostRewriteOneShotNode* _rewriteNode;
    // 単語素片
    MString _pieceStr;
    // 書き換え対象文字数
    //size_t _rewritableLen;
    // 削除文字数
    int _numBS;

public:
    WordPiece(const MString& ms, int len)
        : WordPiece(ms, len, -1) {
    }

    WordPiece(const MString& ms, int len, int nBS)
        : _rewriteNode(0), _pieceStr(ms), _strokeLen(len), _numBS(nBS) {
    }

    WordPiece(const PostRewriteOneShotNode* rewriteNode, int len)
        : _rewriteNode(rewriteNode), _strokeLen(len), _numBS(0) {
    }

    bool equals(const MString& ms, int len, int nBS) const {
        return _pieceStr == ms && _strokeLen == len && _numBS == nBS;
    }
    bool equals(const PostRewriteOneShotNode* rewriteNode, int len) const {
        return _rewriteNode == rewriteNode && _strokeLen == len;
    }

    static WordPiece paddingPiece() {
        return WordPiece(EMPTY_MSTR, -1);
    }

    static WordPiece BSPiece() {
        return WordPiece(EMPTY_MSTR, 1, 1);
    }

    const PostRewriteOneShotNode* rewriteNode() const {
        return _rewriteNode;
    }

    MString getString() const {
        return _rewriteNode ? _rewriteNode->getString() : _pieceStr;
    }

    int strokeLen() const {
        return _strokeLen;
    }

    void setStrokeLen(int len) {
        _strokeLen = len;
    }

    int numBS() const {
        return _numBS;
    }

    bool isPadding() const {
        return _strokeLen < 0;
    }

    bool isBS() const {
        return getString().empty() && strokeLen() == 1 && numBS() == 1;
    }

    String debugString() const;
};

// ラティスから取得した文字列と、修正用のBS数
struct LatticeResult {
    MString outStr;
    size_t numBS;

    LatticeResult(const MString& s, size_t n)
        : outStr(s), numBS(n) {
    }

    static LatticeResult emptyResult() {
        return LatticeResult(EMPTY_MSTR, 0);
    }

    String debugString() const;
};

//#define WORD_LATTICE Lattice::Singleton

// Lattice2
class Lattice2 {
    DECLARE_CLASS_LOGGER;

public:
    // デストラクタ
    virtual ~Lattice2() { }

    // 単語素片リストの追加(単語素片が得られなかった場合も含め、各打鍵ごとに呼び出すこと)
    // 単語素片(WordPiece): 打鍵後に得られた出力文字列と、それにかかった打鍵数
    // return: 出力文字列と、修正用のBS数
    virtual LatticeResult addPieces(const std::vector<WordPiece>& pieces, bool useMorphAnalyzer, bool strokeBack, bool bKatakanaConversion) = 0;

    virtual void syncBaseString(const MString& base) = 0;

    virtual void clearAll() = 0;

    virtual void clear() = 0;

    virtual void removeOtherThanKBest() = 0;

    virtual void removeOtherThanFirst() = 0;

    virtual void setKanjiPreferredNext() = 0;

    virtual bool isKanjiPreferredNext() const = 0;

    virtual void setHiraganaPreferredNext() = 0;

    virtual bool isHiraganaPreferredNext() const = 0;

    virtual void clearKanjiXorHiraganaPreferredNext() = 0;

    virtual bool isEmpty() = 0;

    virtual MString getFirst() const = 0;

    virtual void selectFirst() = 0;

    virtual void selectNext() = 0;

    virtual void selectPrev() = 0;

    virtual void raiseAndLowerByCandSelection() = 0;

    virtual void updateByBushuComp() = 0;

    virtual void updateByMazegaki() = 0;

    virtual bool hasMultiCandidates() const = 0;

    virtual void enableCandidateLog(bool enabled) = 0;

    virtual bool isCandidateLogEnabled() = 0;

    virtual void saveCandidateLog() = 0;

    static void createLattice();

    static void reloadNgramFiles();

    static void reloadUserCostFile();

    static void updateRealtimeNgram(const MString& str);

    static void increaseRealtimeNgramByGUI(const MString& str);

    static void decreaseRealtimeNgramByGUI(const MString& str);
    
    static void saveRealtimeNgramFile();

    static void saveLatticeRelatedFiles();

    static void reloadGlobalPostRewriteMapFile();

    static std::unique_ptr<Lattice2> Singleton;

    //static void loadCostFile();

//    static void runTest();
};

#define WORD_LATTICE Lattice2::Singleton
