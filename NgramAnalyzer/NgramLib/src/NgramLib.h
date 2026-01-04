#ifdef __cplusplus
extern "C" {
#endif

#ifdef DLL_EXPORT
#   define NGRAM_DLL_EXTERN  __declspec(dllexport)
#else
#   define NGRAM_DLL_EXTERN  __declspec(dllimport)
#endif

// コマンドライン引数による形態素解析ライブラリの初期化
NGRAM_DLL_EXTERN int NgramInitialize(size_t argc, const wchar_t** argv, const wchar_t* logFile, wchar_t* errMsgBuf, size_t bufsiz, bool showError = false);

// ユーザー辞書の再オープン
NGRAM_DLL_EXTERN int NgramReloadUserDics(wchar_t* errMsgBuf, size_t bufsiz);

// ユーザー辞書のコンパイルと再オープン
NGRAM_DLL_EXTERN int NgramCompileAndLoadUserDic(const wchar_t* dicDir, const wchar_t* srcFilePath, const wchar_t* outputFilePath, wchar_t* errMsgBuf, size_t bufsiz);

// 辞書の作成
NGRAM_DLL_EXTERN int NgramMakeDictIndex(size_t argc, const wchar_t** argv, const wchar_t* logFile, wchar_t* errMsgBuf, size_t bufsiz, bool showError = false);

// 形態素解析の実行(コストを返す)
NGRAM_DLL_EXTERN int NgramAnalyze(const wchar_t* sentence, wchar_t* wakati_buf, size_t bufsize, bool bStdout, wchar_t* errMsgBuf, size_t bufsiz);

// ログレベルの設定
NGRAM_DLL_EXTERN void NgramSetLogLevel(int logLevel);

// ログの書き出し
NGRAM_DLL_EXTERN void NgramSaveLog(wchar_t* errMsgBuf, size_t bufsiz);

// 形態素解析ライブラリの終了
NGRAM_DLL_EXTERN int NgramFinalize(wchar_t* errMsgBuf, size_t bufsiz);

#ifdef __cplusplus
}
#endif
