#ifdef __cplusplus
extern "C" {
#endif

#ifdef DLL_EXPORT
#   define DYMAZIN_DLL_EXTERN  __declspec(dllexport)
#else
#   define DYMAZIN_DLL_EXTERN  __declspec(dllimport)
#endif

// コマンドライン引数による形態素解析ライブラリの初期化
DYMAZIN_DLL_EXTERN int DymazinInitialize(size_t argc, const wchar_t** argv, const wchar_t* logFile, wchar_t* errMsgBuf, size_t bufsiz, bool showError = false);

// ユーザー辞書の再オープン
DYMAZIN_DLL_EXTERN int DymazinReloadUserDics(wchar_t* errMsgBuf, size_t bufsiz);

// ユーザー辞書のコンパイルと再オープン
DYMAZIN_DLL_EXTERN int DymazinCompileAndLoadUserDic(const wchar_t* dicDir, const wchar_t* srcFilePath, const wchar_t* outputFilePath, wchar_t* errMsgBuf, size_t bufsiz);

// 辞書行の展開
DYMAZIN_DLL_EXTERN int DymazinExpandDictLine(size_t argc, const wchar_t** argv, const wchar_t* logFile, wchar_t* errMsgBuf, size_t bufsiz, bool showError = false);

// 辞書の作成
DYMAZIN_DLL_EXTERN int DymazinMakeDictIndex(size_t argc, const wchar_t** argv, const wchar_t* logFile, wchar_t* errMsgBuf, size_t bufsiz, bool showError = false);

// 形態素解析の実行(コストを返す)
DYMAZIN_DLL_EXTERN int DymazinAnalyze(const wchar_t* sentence, wchar_t* wakati_buf, size_t bufsize, int mazePenalty, int mazeConnPenalty, bool allowNonTerminal, bool bStdout, wchar_t* errMsgBuf, size_t bufsiz);

// ログレベルの設定
DYMAZIN_DLL_EXTERN void DymazinSetLogLevel(int logLevel);

// ログの書き出し
DYMAZIN_DLL_EXTERN void DymazinSaveLog(wchar_t* errMsgBuf, size_t bufsiz);

// 形態素解析ライブラリの終了
DYMAZIN_DLL_EXTERN int DymazinFinalize(wchar_t* errMsgBuf, size_t bufsiz);

#ifdef __cplusplus
}
#endif
