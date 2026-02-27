#include "../NgramCoreLib/src/NgramCoreLib.h"
#include "NgramLib.h"

// 初期化
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int NgramInitialize(size_t argc, const wchar_t** argv, const wchar_t* logFilePath, wchar_t* errMsgBuf, size_t bufsiz, bool showError) {
    return NgramCoreLib::NgramInitialize(argc, argv, logFilePath, errMsgBuf, bufsiz, showError);
}

// ユーザー辞書の再ロード
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int NgramReloadUserDics(wchar_t* errMsgBuf, size_t bufsiz) {
    return NgramCoreLib::NgramReloadUserDics(errMsgBuf, bufsiz);
}

// ユーザー辞書のコンパイルとロード
// @param filePath ユーザー辞書のファイルパス
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int NgramCompileAndLoadUserDic(const wchar_t* dicDir, const wchar_t* srcFilePath, const wchar_t* outputFilePath, wchar_t* errMsgBuf, size_t bufsiz) {
    return NgramCoreLib::NgramCompileAndLoadUserDic(dicDir, srcFilePath, outputFilePath, errMsgBuf, bufsiz);
}

// 辞書の作成
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int NgramMakeDictIndex(size_t argc, const wchar_t** argv, const wchar_t* logFilePath, wchar_t* errMsgBuf, size_t bufsiz, bool showError) {
    return NgramCoreLib::NgramMakeDictIndex(argc, argv, logFilePath, errMsgBuf, bufsiz, showError);
}

/**
 * Ngram解析の実行(コストを返す)
 * @param tempEntries 一時的なユーザー辞書エントリ ("|" 区切り)
 * @param mazePenalty 交ぜ書きエントリに対するペナルティ(0 ならデフォルト値を使う)
 * @return 解のコスト(非負値; 実行時エラーがある場合は負値を返す)
 */
int NgramAnalyze(const wchar_t* sentence, const wchar_t* tempEntries, wchar_t* wakati_buf, size_t bufsize, bool bStdout, wchar_t* errMsgBuf, size_t bufsiz) {
    return NgramCoreLib::NgramAnalyze(sentence, tempEntries, wakati_buf, bufsize, bStdout, errMsgBuf, bufsiz);
}

void NgramSetLogLevel(int logLevel) {
    NgramCoreLib::NgramSetLogLevel(logLevel);
}

void NgramSaveLog(wchar_t* errMsgBuf, size_t bufsiz) {
    NgramCoreLib::NgramSaveLog(errMsgBuf, bufsiz);
}

// 終了
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int NgramFinalize(wchar_t* errMsgBuf, size_t bufsiz) {
    return NgramCoreLib::NgramFinalize(errMsgBuf, bufsiz);
}
