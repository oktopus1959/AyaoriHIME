#pragma once

#include "Reporting/Logger.h"
#include "DyMazin/DymazinBridge.h"
#include "Reporting/ErrorHandler.h"

#include "utils/path_utils.h"

#ifndef _DEBUG
#define USE_MORPHER 1
#else
#define USE_MORPHER 1
#endif
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
        initializeSucceeded = (ret == 0);
        if (!initializeSucceeded) {
            LOG_ERROR(_T("Morpher Initialize FAILED: rcfile={}, dicdir={}, unMax={}"), rcfile, dicdir, unkMax);
        }
#endif //_DEBUG
    }

    inline void morphFinalize() {
#if USE_MORPHER
        return DymazinBridge::dymazinFinalize();
#endif //_DEBUG
    }

    inline void morphReopenUserDics() {
        if (initializeSucceeded) {
            DymazinBridge::dymazinReopenUserDics();
        }
    }

    inline int morphCompileAndLoadUserDic(StringRef dicDir, StringRef filePath) {
        LOG_INFO(_T("CALLED: dicDir={}, filePath={}"), dicDir, filePath);
        if (!initializeSucceeded) {
            ERROR_HANDLER->Warn(L"形態素解析器の初期化に失敗しています。");
            return 1;
        }
        int result = DymazinBridge::dymazinCompileAndLoadUserDic(dicDir, filePath);
        if (result < 0) ERROR_HANDLER->Warn(L"ユーザー交ぜ書き辞書のコンパイルと読み込みに失敗しました: file=" + filePath);
        ERROR_HANDLER->Info(L"ユーザー交ぜ書き辞書をインポート: file=" + filePath + L"\r\n行数: " + std::to_wstring(result));
        return result;
    }

    inline void morphSetLogLevel(int logLevel) {
#if USE_MORPHER
        return DymazinBridge::dymazinSetLogLevel(logLevel);
#endif //_DEBUG
    }

    inline void morphSaveLog() {
#if USE_MORPHER
        return DymazinBridge::dymazinSaveLog();
#endif //_DEBUG
    }

    inline int morphCalcCost(const MString& str, std::vector<MString>& morphs, int mazePenalty, int mazeConnPenalty, bool allowNonTerminal) {
        if (!initializeSucceeded) return 0;

#if USE_MORPHER
        return DymazinBridge::dymazinCalcCost(str, morphs, mazePenalty, mazeConnPenalty, allowNonTerminal);
#else
        return 0;
#endif //_DEBUG
    }
}
