#include "Model.h"

#include "exception.h"

namespace analyzer {
    DEFINE_CLASS_LOGGER(Model);

    /**
     * Model class
     */
    Model::Model(OptHandlerPtr opts)
        : opts(opts), viterbi(opts)
    {
        LOG_INFOH(L"ENTER: ctor");
        bos_feature = L""; //getBosFeature(opts);
        LOG_INFOH(L"LEAVE: ctor");
    }

    /**
     * 辞書情報
     */
    DictionaryInfoPtr Model::dictionary_info() {
        THROW_RTE(L"Model::dictionary_info(): not implemented");
        return MakeShared<dict::DictionaryInfo>();
    }

    /**
     * 形態素解析の実行
     * @param sentence 解析対象文
     * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
     * @param nBest N-Best解の個数 (省略可; デフォルト=1)
     * @return 解析結果を格納したラティスオブジェクト
     */
    LatticePtr Model::analyze(StringRef sentence, StringRef tempEntries, size_t nBest) {
        LOG_INFO(L"ENTER: sentence={}, nBest={}", sentence, nBest);
        auto lattice = Lattice::CreateLattice(sentence, bos_feature, nBest);
        viterbi.analyze(lattice, tempEntries);
        LOG_INFO(L"LEAVE");
        return lattice;
    }

    /**
     * N-Best 解析の実行。N-Bestな解析結果が一つずつ格納されたArrayを返す。
     * @param sentence 解析対象文
     * @param realtimeEntries リアルタイム登録による辞書エントリ
     * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
     * @param results N-Bestな解析結果を格納するvector
     * @param nBest N-Bestの最大数
     * @param mazePenalty 交ぜ書き候補に与えるペナルティ。これが負値ならボーナスなり、他の交ぜ書き候補を含めた解を返す
     * @return 最良解析結果のコスト
     */
    int Model::parseNBest(StringRef sentence, const std::map<String, int>& realtimeEntries, StringRef tempEntries, Vector<String>& results, size_t nBest, bool needResults) {
        //warnings.clear();
        LOG_INFO(L"CALLED: sentence={}, nBest={}", sentence, nBest);

        return analyze(sentence, tempEntries, nBest)->getSolutions(results, needResults);
    }

    //String Model::getBosFeature(OptHandlerPtr opts, StringRef defval) {
    //    LOG_INFOH(L"ENTER: defval={}", defval);
    //    String bos_feat = opts->getString(L"bos-feature", defval);
    //    CHECK_OR_THROW(!bos_feat.empty(), L"bos-feature is undefined in dicrc");
    //    LOG_INFOH(L"LEAVE: result={}", bos_feat);
    //    return bos_feat;
    //}

    /** ユーザー辞書の再ロード */
    void Model::reload_userdics() {
        LOG_INFOH(L"ENTER");
        viterbi.reload_userdics();
        LOG_INFOH(L"LEAVE");
    }

} // namespace analyzer