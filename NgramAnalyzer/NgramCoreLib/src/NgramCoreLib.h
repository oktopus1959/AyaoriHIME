
namespace NgramCoreLib {
    // コマンドライン引数による形態素解析ライブラリの初期化
    int NgramInitialize(size_t argc, const wchar_t** argv, const wchar_t* logFile, wchar_t* errMsgBuf, size_t bufsiz, bool showError = false);

    // ユーザー辞書の再オープン
    int NgramReloadUserDics(wchar_t* errMsgBuf, size_t bufsiz);

    // ユーザー辞書のコンパイルと再オープン
    int NgramCompileAndLoadUserDic(const wchar_t* dicDir, const wchar_t* srcFilePath, const wchar_t* outputFilePath, wchar_t* errMsgBuf, size_t bufsiz);

    // 辞書の作成
    int NgramMakeDictIndex(size_t argc, const wchar_t** argv, const wchar_t* logFile, wchar_t* errMsgBuf, size_t bufsiz, bool showError = false);

    // 形態素解析の実行(コストを返す)
    int NgramAnalyze(const wchar_t* sentence, const wchar_t* tempEntries, wchar_t* wakati_buf, size_t bufsize, bool bStdout, wchar_t* errMsgBuf, size_t bufsiz);

    // ログレベルの設定
    void NgramSetLogLevel(int logLevel);

    // ログの書き出し
    void NgramSaveLog(wchar_t* errMsgBuf, size_t bufsiz);

    // 形態素解析ライブラリの終了
    int NgramFinalize(wchar_t* errMsgBuf, size_t bufsiz);
}
