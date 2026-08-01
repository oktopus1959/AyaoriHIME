#include "Logger.h"

#include "Settings.h"
#include "State.h"

#include "SimpleDicCandidates.h"

#if 0
#undef _LOG_DEBUGH
#define _LOG_DEBUGH LOG_INFOH
#endif

namespace {
    // -------------------------------------------------------------------
    // 簡易辞書検索候補リストのクラス
    class SimpleDicCandidatesImpl : public SimpleDicCandidates {
        DECLARE_CLASS_LOGGER;

        // 簡易辞書検索候補のリスト
        SImpleDicResultList candidates;

        // 候補単語列
        std::vector<SimpleDicResult> results;

        SimpleDicResult emptyResult;

        // 現在、候補選択に使われているキー
        MString currentKey;

        // 英数モードから明示的に変換された1文字ASCIIキーの検索を許可するか
        bool currentAllowSingleAsciiMap = false;

        // 選択位置 -- -1 は未選択状態を表す
        mutable int selectPos = -1;

        // 未選択状態に戻す
        inline void resetSelectPos() {
            selectPos = -1;
        }

        inline void setSelectPos(size_t n) const {
            size_t x = std::min(candidates.Size(), SETTINGS->simpleDicHorizontalCandMax);
            selectPos = (int)(n >= 0 && n < x ? n : -1);
        }

        // 選択位置をインクリメント //(一周したら未選択状態に戻る)
        inline void incSelectPos() const {
            size_t x = std::min(candidates.Size(), SETTINGS->simpleDicHorizontalCandMax);
            selectPos = selectPos < 0 ? 0 : x <= 0 ? -1 : (selectPos + 1) % x;
        }

        // 選択位置をデクリメント //(一周したら未選択状態に戻る)
        inline void decSelectPos() const {
            int x = (int)(std::min(candidates.Size(), SETTINGS->simpleDicHorizontalCandMax));
            selectPos = selectPos <= 0 ? x - 1 : x <= 0 ? -1 : (selectPos - 1) % x;
        }

        inline int getSelectPos() const {
            return selectPos;
        }

        inline bool isSelecting() const {
            return selectPos > 0 && selectPos < (int)results.size();
        }

        inline const SimpleDicResult getSelectedResult() const {
            int n = getSelectPos();
            int x = (int)(std::min(candidates.Size(), SETTINGS->simpleDicHorizontalCandMax));
            return n >= 0 && n < x ? candidates.GetNthResult(n) : emptyResult;
        }

        inline const MString& getSelectedWord() const {
            int n = getSelectPos();
            return n >= 0 && n < (int)results.size() ? results[n].Word : EMPTY_MSTR;
        }

    public:
        ~SimpleDicCandidatesImpl() {
        }

    public:
        // 候補検索キー設定をクリアする
        void ClearKeyInfo() override {
            candidates.ClearKeyInfo();
            currentKey.clear();
            currentAllowSingleAsciiMap = false;
        }

        // 指定のキーで始まる候補を取得する
        const std::vector<SimpleDicResult>& GetCandidates(const MString& key,
                                                     bool allowSingleAsciiMap = false) override {
            DelayedPushFrontSelectedWord();
            currentAllowSingleAsciiMap = allowSingleAsciiMap;
            candidates = SIMPLE_DIC->GetCandidates(key, currentKey, allowSingleAsciiMap);  // ここで currentKey は変更される (currentKey = resultKey)
            results.clear();
            utils::append(results, candidates.GetResults());
            _LOG_DEBUGH(_T("cands num={}, new currentKey={}"), results.size(), to_wstr(currentKey));
            return results;
        }

        const std::vector<MString> GetCandWords(const MString& key,
                                                bool allowSingleAsciiMap = false) override {
            _LOG_DEBUGH(_T("CALLED: key={}, allowSingleAsciiMap={}"), to_wstr(key), allowSingleAsciiMap);
            GetCandidates(key, allowSingleAsciiMap);
            return GetCandWords();
        }

        // 取得済みの候補列を返す
        //const std::vector<SimpleDicResult>& GetCandidates() const override {
        //    return results;
        //}

        const std::vector<MString> GetCandWords() const override {
            _LOG_DEBUGH(_T("CALLED"));
            std::vector<MString> words;
            utils::transform_append(results, words, [](const SimpleDicResult& res) { return res.Word; });
            return words;
        }

        const MString& GetCurrentKey() const override {
            return currentKey;
        }

        // 次の候補を選択する
        const SimpleDicResult GetNext() const override {
            incSelectPos();
            return getSelectedResult();
        }

        // 前の候補を選択する
        const SimpleDicResult GetPrev() const override {
            decSelectPos();
            return getSelectedResult();
        }

        // 選択された単語を取得する
        const SimpleDicResult GetPositionedResult(size_t pos) const override {
            _LOG_DEBUGH(_T("CALLED: selectPos={}"), pos);
            setSelectPos(pos);
            return getSelectedResult();
        }

        // 選択された単語を取得する
        const MString& GetSelectedWord() const override {
            _LOG_DEBUGH(_T("CALLED: selectPos={}"), selectPos);
            return getSelectedWord();
        }

        // 選択されている位置を返す -- 未選択状態なら -1を返す
        int GetSelectPos() const override {
            _LOG_DEBUGH(_T("CALLED: nextSelect={}"), selectPos);
            return getSelectPos();
        }

        // 選択位置を初期化(未選択状態)する
        const SimpleDicResult ClearSelectPos() override {
            _LOG_DEBUGH(_T("CALLED: nextSelect={}"), selectPos);
            resetSelectPos();
            return emptyResult;
        }

        // 候補が選択されていれば、それを使用履歴の先頭にpushする -- selectPos は未選択状態に戻る
        void DelayedPushFrontSelectedWord() override {
            _LOG_DEBUGH(_T("ENTER"));
            if (isSelecting()) {
                SIMPLE_DIC->UseWord(GetSelectedWord());
            }
            ClearSelectPos();
            _LOG_DEBUGH(_T("LEAVE"));
        }

    };
    DEFINE_CLASS_LOGGER(SimpleDicCandidatesImpl);

} // namespace

// SimpleDicCandidates::Singleton
std::unique_ptr<SimpleDicCandidates> SimpleDicCandidates::Singleton;

void SimpleDicCandidates::CreateSingleton() {
    Singleton.reset(new SimpleDicCandidatesImpl());
}
