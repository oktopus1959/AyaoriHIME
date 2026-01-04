#pragma once

#include "util/string_utils.h"
#include "Logger.h"

// エラーハンドラ
class ErrorHandler {
    DECLARE_CLASS_LOGGER;

    int errorLevel = 0;

    String errorMsg;

public:
    // 正常終了
    static const int LEVEL_SUCCESS = 0;
    // 正常終了だが、情報メッセージあり
    static const int LEVEL_INFO = -1;
    // 正常終了だが、警告メッセージあり
    static const int LEVEL_WARN = -2;
    // 異常終了
    static const int LEVEL_ERROR = -3;

public:
    // 初期化
    void Clear();

    // エラー情報を格納
    int Error(StringRef msg);

    // 警告情報を格納する
    int Warn(StringRef msg);

    // Info情報を格納する
    int Info(StringRef msg);

    // エラー情報を格納する
    int SetErrorInfo(int level, StringRef msg);

    // 異常終了したかどうか
    inline bool HasError() const { return errorLevel == LEVEL_ERROR; }

    // エラーレベルを取得する
    inline int GetErrorLevel() const { return errorLevel; }

    // エラーメッセージを取得する
    inline StringRef GetErrorMsg() const { return errorMsg; }

    // エラー情報を取得する。戻り値はエラーコード
    int GetErrorInfo(wchar_t* errMsgBuf, size_t bufsize) const;

private:
    static std::unique_ptr<ErrorHandler> _singleton;

public:
    static const std::unique_ptr<ErrorHandler>& Singleton();

};

#define ERROR_HANDLER (ErrorHandler::Singleton())
