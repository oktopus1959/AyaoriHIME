#pragma once

#include "string_utils.h"

namespace DymazinBridge {
    int dymazinInitialize(StringRef rcfile, StringRef dicdir, int unkMax, int mazePenalty = 1000, int mazeConnPenalty = 1000, int nonTerminalCost = 5000);

    void dymazinFinalize();

    void dymazinSetLogLevel(int logLevel);

    void dymazinSaveLog();

    int dymazinCalcCost(const MString& str, std::vector<MString>& words, int mazePenalty, int mazeConnPenalty, bool allowNonTerminal);
}
