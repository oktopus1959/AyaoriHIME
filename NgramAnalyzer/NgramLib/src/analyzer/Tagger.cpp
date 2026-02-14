#include "Tagger.h"

namespace analyzer {
    DEFINE_CLASS_LOGGER(Tagger);

    // Constructor
    Tagger::Tagger(ModelPtr model) : model(model) {
        LOG_INFO(L"CALLED: ctor: model");
    }

    /**
     * 形態素解析の実行 (最良解のみを出力)
     * @param sentence 解析対象の文
     * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
     * @return 解析結果
     */
    String Tagger::parse(StringRef sentence, StringRef tempEntries, size_t nBest) {
        LOG_INFO(L"ENTER: sentence={}", sentence);
        Vector<String> results;
        parseNBest(sentence, tempEntries, results, nBest, true);

        if (Reporting::Logger::IsInfoEnabled()) {
            if (results.empty()) {
                LOG_INFO(L"LEAVE: result: empty");
            } else {
                LOG_INFO(L"LEAVE: result:\n--------------------------------------------------\n{}\n--------------------------------------------------", results.front());
            }
        }
        return results.empty() ? L"" : results.front();
    }

    /**
     * N-Best 解析の実行。N-Bestな解析結果が一つずつ格納されたArrayを返す。
     * @param sentence 解析対象文
     * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
     * @param results N-Bestな解析結果を格納するvector
     * @param nBest N-Bestの最大数
     * @param mazePenalty 交ぜ書き候補に与えるペナルティ。これが負値ならボーナスなり、他の交ぜ書き候補を含めた解を返す
     * @return 最良解析結果のコスト
     */
    int Tagger::parseNBest(StringRef sentence, StringRef tempEntries, Vector<String>& results, size_t nBest, bool needResults) {
        //warnings.clear();
        LOG_INFO(L"CALLED: sentence={}, nBest={}", sentence, nBest);

        return model->analyze(sentence, tempEntries, nBest)->getSolutions(results, needResults);
    }

} // namespace analyzer
