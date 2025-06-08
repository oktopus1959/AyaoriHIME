#pragma once

#include "Reporting/Logger.h"
#include "Mecab/MecabBridge.h"
#include "DyMazin/DymazinBridge.h"

#ifndef _DEBUG
#define USE_MORPHER 1
#else
#define USE_MORPHER 1
#endif
#define USE_DYMAZIN 1

namespace MorphBridge {
    inline void morphInitialize(Reporting::Logger& logger) {
#if USE_MORPHER
        int unkMax = 3;
#if USE_DYMAZIN
        auto rcfile = utils::joinPath(SETTINGS->rootDir, _T("dymazin/etc/morphrc"));
        auto dicdir = utils::joinPath(SETTINGS->rootDir, _T("dymazin/dic/ipadic"));
        int ret = DymazinBridge::dymazinInitialize(rcfile, dicdir, unkMax, SETTINGS->morphMazeEntryPenalty, SETTINGS->morphNonTerminalCost);
#else
        auto rcfile = utils::joinPath(SETTINGS->rootDir, _T("mecab/etc/mecabrc"));
        auto dicdir = utils::joinPath(SETTINGS->rootDir, _T("mecab/dic/ipadic"));
        int ret = MecabBridge::mecabInitialize(rcfile, dicdir, unkMax);
#endif
        if (ret != 0) {
            LOG_INFOH(_T("Morpher Initialize FAILED: rcfile={}, dicdir={}, unMax={}"), rcfile, dicdir, unkMax);
        }
#endif //_DEBUG
    }

    inline void morphFinalize() {
#if USE_MORPHER
#if USE_DYMAZIN
        return DymazinBridge::dymazinFinalize();
#else
        return MecabBridge::mecabFinalize();
#endif
#endif //_DEBUG
    }

    inline void morphSetLogLevel(int logLevel) {
#if USE_MORPHER
#if USE_DYMAZIN
        return DymazinBridge::dymazinSetLogLevel(logLevel);
#endif
#endif //_DEBUG
    }

    inline void morphSaveLog() {
#if USE_MORPHER
#if USE_DYMAZIN
        return DymazinBridge::dymazinSaveLog();
#endif
#endif //_DEBUG
    }

    inline int morphCalcCost(const MString& str, std::vector<MString>& words, int mazePenalty, bool allowNonTerminal) {
#if USE_MORPHER
#if USE_DYMAZIN
        return DymazinBridge::dymazinCalcCost(str, words, mazePenalty, allowNonTerminal);
#else
        return MecabBridge::mecabCalcCost(str, words);
#endif
#else
        return 0;
#endif //_DEBUG
    }
}
