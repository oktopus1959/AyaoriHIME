#include "Tagger.h"

namespace analyzer {
    DEFINE_CLASS_LOGGER(Tagger);

    // Constructor
    Tagger::Tagger(ModelPtr model) : model(model) {
        LOG_INFO(L"CALLED: ctor: model");
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
    int Tagger::parseNBest(StringRef sentence, const std::map<String, int>& realtimeEntries, StringRef tempEntries, Vector<String>& results, size_t nBest, bool needResults) {
        //warnings.clear();
        LOG_INFO(L"CALLED: sentence={}, nBest={}", sentence, nBest);

        //return model->analyze(sentence, tempEntries, nBest)->getSolutions(results, needResults);
        return 0;
    }

} // namespace analyzer
