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

        // 辞書検索中か
        bool isInSearch = false;

        // 現在、履歴選択に使われているキー
        MString currentKey;

        int currentLen = 0;

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
        // 履歴検索キー設定をクリアする
        void ClearKeyInfo() override {
            candidates.ClearKeyInfo();
            currentKey.clear();
            currentAllowSingleAsciiMap = false;
            isInSearch = false;
        }

        bool IsInSearch() override {
            return isInSearch;
        }

        const MString& GetOrigKey() override {
            return candidates.GetOrigKey();
        }

        // 指定のキーで始まる候補を取得する (len > 0 なら指定の長さの候補だけを取得, len < 0 なら Abs(len)以下の長さの候補を取得)
        const std::vector<SimpleDicResult>& GetCandidates(const MString& key, int len,
                                                     bool allowSingleAsciiMap = false) override {
            isInSearch = true;
            DelayedPushFrontSelectedWord();
            currentLen = len;
            currentAllowSingleAsciiMap = allowSingleAsciiMap;
            candidates = SIMPLE_DIC->GetCandidates(key, currentKey, len, allowSingleAsciiMap);  // ここで currentKey は変更される (currentKey = resultKey)
            results.clear();
            utils::append(results, candidates.GetResults());
            _LOG_DEBUGH(_T("cands num={}, new currentKey={}"), results.size(), to_wstr(currentKey));
            return results;
        }

        const std::vector<MString> GetCandWords(const MString& key, int len,
                                                bool allowSingleAsciiMap = false) override {
            _LOG_DEBUGH(_T("CALLED: key={}, len={}, allowSingleAsciiMap={}"), to_wstr(key), len, allowSingleAsciiMap);
            GetCandidates(key, len, allowSingleAsciiMap);
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

        // 次の履歴を選択する
        const SimpleDicResult GetNext() const override {
            incSelectPos();
            return getSelectedResult();
        }

        // 前の履歴を選択する
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

        // 取得済みの履歴入力候補リストから指定位置の候補を返す
        // 選択された候補は使用履歴の先頭に移動する
        const SimpleDicResult SelectNth(size_t n) override {
            _LOG_DEBUGH(_T("ENTER: n={}, results={}"), n, results.size());
            ClearSelectPos();
            if (n >= results.size()) {
                _LOG_DEBUGH(_T("LEAVE: empty"));
                return emptyResult;
            }

            SimpleDicResult result = results[n];
            SIMPLE_DIC->UseWord(result.Word);
            GetCandidates(currentKey, currentLen, currentAllowSingleAsciiMap);
            _LOG_DEBUGH(_T("LEAVE: OrigKey={}, Key={}, Word={}"), to_wstr(result.OrigKey), to_wstr(result.Key), to_wstr(result.Word));
            return result;
        }

    };
    DEFINE_CLASS_LOGGER(SimpleDicCandidatesImpl);

} // namespace

// SimpleDicCandidates::Singleton
std::unique_ptr<SimpleDicCandidates> SimpleDicCandidates::Singleton;

void SimpleDicCandidates::CreateSingleton() {
    Singleton.reset(new SimpleDicCandidatesImpl());
}
