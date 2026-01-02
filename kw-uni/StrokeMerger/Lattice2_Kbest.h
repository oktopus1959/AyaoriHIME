#pragma once

namespace lattice2 {

    // K-best な文字列を格納する
    class KBestList {

    public:
    // デストラクタ
        virtual ~KBestList() { }

        //virtual void setPrevBS(bool flag) = 0;

        //virtual bool isPrevBS() const = 0;

        // 候補選択による、リアルタイムNgramの蒿上げと抑制
        virtual void raiseAndLowerByCandSelection() = 0;

        virtual void clearKbests(bool clearAll) = 0;

        virtual void removeOtherThanKBest() = 0;

        virtual void removeOtherThanFirst() = 0;

        virtual void removeOtherThanFirstForEachStroke() = 0;

        virtual bool isEmpty() const = 0;

        virtual int size() const = 0;

        virtual MString getTopString() = 0;

        virtual int origFirstCand() const = 0;

        virtual int selectedCandPos() const = 0;

        virtual void setSelectedCandPos(int nSameLen) = 0;

        virtual int cookedOrigFirstCand() const = 0;

        virtual void resetOrigFirstCand() = 0;

        virtual void incrementOrigFirstCand(int nSameLen) = 0;

        virtual void decrementOrigFirstCand(int nSameLen) = 0;

        // ストローク長の同じ候補の数を返す
        virtual size_t getNumOfSameStrokeLen() const = 0;

        virtual std::vector<MString> getTopCandStrings() const = 0;

        virtual std::vector<MString> getCandStringsInSelectedBlock() const = 0;

        virtual String debugCandidates(size_t maxLn = 100000) const = 0;

        virtual String debugKBestString(size_t maxLn = 100000) const = 0;

    public:
        virtual void setKanjiXorHiraganaPreferredNextCands(bool) = 0;

        virtual void clearKanjiXorHiraganaPreferredNextCands() = 0;

        virtual String kanjiXorHiraganaPreferredNextCandsDebug() const = 0;

    public:
        // strokeCount: lattice に最初に addPieces() した時からの相対的なストローク数
        virtual void updateKBestList(const std::vector<WordPiece>& pieces, bool useMorphAnalyzer, int strokeCount, bool strokeBack, bool bKatakanaConversion) = 0;

    public:
        // 先頭を表すダミーを用意しておく
        virtual void addDummyCandidate() = 0;

        // 先頭候補を取得する
        virtual MString getFirst() const = 0;

        // 先頭候補を最優先候補にする
        virtual void selectFirst() = 0;

        // 次候補を選択する (一時的に優先するだけなので、他の候補を削除したりはしない)
        virtual void selectNext() = 0;

        // 前候補を選択する (一時的に優先するだけなので、他の候補を削除したりはしない)
        virtual void selectPrev() = 0;

    public:
        // 部首合成
        virtual void updateByBushuComp() = 0;

        // 交ぜ書き変換
        virtual void updateByMazegaki() = 0;

    public:
        static std::unique_ptr<KBestList> createKBestList();
    };

} // namespace lattice2
