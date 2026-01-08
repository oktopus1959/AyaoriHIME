#include "util/string_utils.h"

#define JOYO_KANJI_FILE   L"joyo-kanji-plus.txt"
#define KANJI_BIGRAM_FILE L"kanji-bigram.txt"
//#define KANJI_BIGRAM_FILE L"kanji-bigram.top500.txt"

namespace MazegakiPreprocessor{
    void initialize(StringRef joyoKanjiFile, StringRef kanjiBigramFile);

    void output(std::vector<String>& results);

    // 交ぜ書き辞書の準備と展開
    // @param mazeResrcDir joyo-kanji-plus.txtとkanji-bigram.txtを格納する、交ぜ書きリソースディレクトリのパス
    // @param dicSrcPath 交ぜ書き辞書ソースファイルのパス
    Vector<String> preprocessMazegakiDic(StringRef mazeResrcDir, StringRef dicSrcPath, bool bUserDic);
}