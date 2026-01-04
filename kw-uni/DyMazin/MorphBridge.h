#pragma once

#include "string_utils.h"

namespace MorphBridge {
    void morphInitialize();

    void morphFinalize();

    void morphReopenUserDics();

    int morphCompileAndLoadUserDic(StringRef dicDir, StringRef filePath);

    void morphSetLogLevel(int logLevel);

    void morphSaveLog();

    int morphCalcCost(const MString& str, std::vector<MString>& morphs, int mazePenalty, int mazeConnPenalty, bool allowNonTerminal);
}
