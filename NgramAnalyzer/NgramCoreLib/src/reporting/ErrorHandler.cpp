// エラーハンドラ
#include "ErrorHandler.h"

DEFINE_CLASS_LOGGER(ErrorHandler);

std::unique_ptr<ErrorHandler> ErrorHandler::_singleton;

const std::unique_ptr<ErrorHandler>& ErrorHandler::Singleton() {
    if (!_singleton) {
        _singleton.reset(new ErrorHandler());
    }
    return _singleton;
}

// 初期化
void ErrorHandler::Clear() {
    errorLevel = 0;
    errorMsg.clear();
}

int ErrorHandler::GetErrorInfo(wchar_t* errMsgBuf, size_t bufsize) const {
    errMsgBuf[0] = L'\0';
    if (errorLevel < 0) {
        wcsncpy_s(errMsgBuf, bufsize, errorMsg.c_str(), bufsize - 1);
        errMsgBuf[bufsize - 1] = L'\0';
    }
    return errorLevel;
}

// エラー情報を格納
int ErrorHandler::SetErrorInfo(int level, StringRef msg) {
    if (level != LEVEL_SUCCESS) {
        if (level < errorLevel) errorLevel = level;
        if (errorMsg.size() + msg.size() < 800) {
            if (level == LEVEL_ERROR || errorMsg.size() + msg.size() < 500) {
                if (!errorMsg.empty()) errorMsg.append(_T("\r\n\r\n"));
                errorMsg.append(msg);
            }
        }
    }
    return level;
}

// エラー情報を格納
int ErrorHandler::Error(StringRef msg) {
    return SetErrorInfo(LEVEL_ERROR, msg);
}

// 警告情報を格納
int ErrorHandler::Warn(StringRef msg) {
    return SetErrorInfo(LEVEL_WARN, msg);
}

// Info情報を格納
int ErrorHandler::Info(StringRef msg) {
    return SetErrorInfo(LEVEL_INFO, msg);
}

