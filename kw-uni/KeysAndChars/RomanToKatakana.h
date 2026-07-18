#include "string_utils.h"

namespace RomanToKatakana {
    // デフォルトのローマ字定義ファイルを読み込む
    void ReadDefaultRomanDefFile();

    // ローマ字定義ファイルを読み込む
    void ReadRomanDefFile(StringRef defFilePath);

    // ローマ字をカタカタナに変換する
    MString convertRomanToKatakana(const MString& s);
}

