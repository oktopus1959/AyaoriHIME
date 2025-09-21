#pragma once

#include "Reporting/Logger.h"
#include "DyMazin/DymazinBridge.h"
#include "Reporting/ErrorHandler.h"

#include "utils/path_utils.h"

#define USE_MORPHER 1
#define USE_DYMAZIN 1

#define DYMAZIN_MORPHRC _T("dymazin/etc/morphrc")
#define DYMAZIN_DICDIR  _T("dymazin/dic/mazedic")

namespace MorphBridge {
    inline DEFINE_NAMESPACE_LOGGER(MorphBridge);

    inline bool initializeSucceeded = false;

    inline void morphInitialize() {
#if USE_MORPHER
        int unkMax = 3;
        auto rcfile = utils::joinPath(SETTINGS->rootDir, DYMAZIN_MORPHRC);
        auto dicdir = utils::joinPath(SETTINGS->rootDir, DYMAZIN_DICDIR);
        int ret = DymazinBridge::dymazinInitialize(rcfile, dicdir, unkMax, SETTINGS->morphMazeEntryPenalty, SETTINGS->morphMazeConnectionPenalty, SETTINGS->morphNonTerminalCost);
        initializeSucceeded = (ret != ErrorHandler::LEVEL_ERROR);
        if (!initializeSucceeded) {
            LOG_ERROR(_T("Morpher Initialize FAILED: rcfile={}, dicdir={}, unMax={}"), rcfile, dicdir, unkMax);
            auto detailedMsg = ERROR_HANDLER->GetErrorMsg();
            ERROR_HANDLER->Clear();
            ERROR_HANDLER->Error(L"形態素解析器の初期化に失敗しました。\r\n詳細: " + detailedMsg);
        }
#endif //USE_MORPHER
    }

    inline void morphFinalize() {
#if USE_MORPHER
        return DymazinBridge::dymazinFinalize();
#endif //USE_MORPHER
    }

    inline void morphReopenUserDics() {
        if (initializeSucceeded) {
            DymazinBridge::dymazinReopenUserDics();
        }
    }

    inline int morphCompileAndLoadUserDic(StringRef dicDir, StringRef filePath) {
        LOG_INFO(_T("CALLED: dicDir={}, filePath={}"), dicDir, filePath);
        if (!initializeSucceeded) {
            return ERROR_HANDLER->Warn(L"形態素解析器の初期化に失敗しています。");
        }
        int result = DymazinBridge::dymazinCompileAndLoadUserDic(dicDir, filePath);
        auto detailedMsg = ERROR_HANDLER->GetErrorMsg();
        ERROR_HANDLER->Clear();
        if (result == ErrorHandler::LEVEL_ERROR) {
            ERROR_HANDLER->Error(L"ユーザー交ぜ書き辞書のコンパイルと読み込みに失敗しました: file=" + filePath + L"\r\n詳細: " + detailedMsg);
        } else {
            ERROR_HANDLER->Info(L"ユーザー交ぜ書き辞書をインポート: file=" + filePath);
            if (!detailedMsg.empty()) {
                ERROR_HANDLER->Info(detailedMsg);
            }
        }
        return result;
    }

    inline void morphSetLogLevel(int logLevel) {
#if USE_MORPHER
        return DymazinBridge::dymazinSetLogLevel(logLevel);
#endif //USE_MORPHER
    }

    inline void morphSaveLog() {
#if USE_MORPHER
        return DymazinBridge::dymazinSaveLog();
#endif //USE_MORPHER
    }

    inline int morphCalcCost(const MString& str, std::vector<MString>& morphs, int mazePenalty, int mazeConnPenalty, bool allowNonTerminal) {
        if (!initializeSucceeded) return 0;

#if USE_MORPHER
        return DymazinBridge::dymazinCalcCost(str, morphs, mazePenalty, mazeConnPenalty, allowNonTerminal);
#else
        return 0;
#endif //USE_MORPHER
    }
}
