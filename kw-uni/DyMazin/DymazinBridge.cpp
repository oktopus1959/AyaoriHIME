#include "Logger.h"
#include "DymazinBridge.h"
#include "../DyMazin/DyMazinLib/src/DyMazinLib.h"
#include "Settings/Settings.h"
#include "Reporting/ErrorHandler.h"

#include "path_utils.h"

#if 0
#undef _LOG_DEBUGH
#define _LOG_DEBUGH LOG_INFOH
#endif

namespace DymazinBridge {
    DEFINE_LOCAL_LOGGER(DymazinBridge);

    int dymazinInitialize(StringRef rcfile, StringRef dicdir, int unkMax, int mazePenalty, int mazeConnPenalty, int nonTerminalCost) {
        LOG_INFOH(_T("ENTER: rcfile={}, dicdir={}, unkMax={}, mazePenalty={}, mazeConnPenalty={}, nonTerminalCost={}, costWithoutEOS={}, -O{}"),
            rcfile, dicdir, unkMax, mazePenalty, mazeConnPenalty, nonTerminalCost, SETTINGS->morphCostWithoutEOS, SETTINGS->morphMazeFormat);

        std::vector<const wchar_t*> av;
        av.push_back(L"dymazin");
        av.push_back(L"-r");
        av.push_back(rcfile.c_str());
        av.push_back(L"-d");
        av.push_back(dicdir.c_str());
        av.push_back(L"--userdic");
        String userDic = utils::join_path(dicdir, L"user.dic");
        av.push_back(userDic.c_str());
        String maxGroupSize(L"--max-grouping-size=");
        maxGroupSize.append(std::to_wstring(unkMax));
        av.push_back(maxGroupSize.c_str());
        //av.push_back(L"-Owakati");
        //av.push_back(L"-Owakati2");     // 交ぜ書きの原形を含む
        String mazeOpt = L"-O";
        mazeOpt += SETTINGS->morphMazeFormat; // maze1 , maze2, maze3
        av.push_back(mazeOpt.c_str());     // 交ぜ書きの原形を含む(w/ feature)
        String mazePenaltyOpt(L"--maze-penalty=");
        mazePenaltyOpt.append(std::to_wstring(mazePenalty));
        av.push_back(mazePenaltyOpt.c_str());
        String mazeConnPenaltyOpt(L"--maze-conn-penalty=");
        mazeConnPenaltyOpt.append(std::to_wstring(mazeConnPenalty));
        av.push_back(mazeConnPenaltyOpt.c_str());
        String nonTerminalCostOpt(L"--non-terminal-cost=");
        nonTerminalCostOpt.append(std::to_wstring(nonTerminalCost));
        av.push_back(nonTerminalCostOpt.c_str());
        if (SETTINGS->morphCostWithoutEOS) av.push_back(L"--ignore-eos");

        const int ARRAY_SIZE = 1024;
        wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };

        int result = DymazinInitialize(av.size(), av.data(), L"dymazin.log", errMsgBuf, ARRAY_SIZE);

        ERROR_HANDLER->SetErrorInfo(result, errMsgBuf);
        LOG_INFOH(_T("LEAVE: result={}, errorMsg={}"), result, errMsgBuf);
        return result;
    }

    int dymazinReopenUserDics() {
        LOG_INFOH(_T("ENTER"));
        const int ARRAY_SIZE = 1024;
        wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };
        int result = DymazinReloadUserDics(errMsgBuf, ARRAY_SIZE);
        ERROR_HANDLER->SetErrorInfo(result, errMsgBuf);
        LOG_INFOH(_T("LEAVE: result={}, errorMsg={}"), result, errMsgBuf);
        return result;
    }

    int dymazinCompileAndLoadUserDic(StringRef dicDir, StringRef srcFilePath) {
        LOG_INFOH(_T("ENTER"));
        const int ARRAY_SIZE = 1024;
        wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };
        String userDic = utils::join_path(dicDir, L"user.dic");
        int result = DymazinCompileAndLoadUserDic(dicDir.c_str(), srcFilePath.c_str(), userDic.c_str(), errMsgBuf, ARRAY_SIZE);
        if (result < 0) ERROR_HANDLER->SetErrorInfo(result, errMsgBuf);
        LOG_INFOH(_T("LEAVE: result={}, errorMsg={}"), result, errMsgBuf);
        return result;
    }

    void dymazinFinalize() {
        LOG_INFOH(_T("CALLED"));
        const int ARRAY_SIZE = 1024;
        wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };
        DymazinFinalize(errMsgBuf, ARRAY_SIZE);
    }

    void dymazinSetLogLevel(int logLevel) {
        LOG_INFOH(_T("CALLED: logLevel={}"), logLevel);
        DymazinSetLogLevel(logLevel);
    }

    void dymazinSaveLog() {
        LOG_INFOH(_T("CALLED"));
        const int ARRAY_SIZE = 1024;
        wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };
        DymazinSaveLog(errMsgBuf, ARRAY_SIZE);
    }

    // 文字列の形態素解析を行い、コストと形態素列を返す 
    // str: 入力文字列、morphs: 形態素列の出力先(改行区切り)、
    // mazePenalty: 交ぜ書きペナルティ、mazeConnPenalty: 交ぜ書き接続ペナルティ(mazePenalty < 0 の場合はボーナスとなり、他の交ぜ書き候補を含めた解を返す)
    // allowNonTerminal: 非終端形態素を許可するか
    int dymazinCalcCost(const MString& str, std::vector<MString>& morphs, int mazePenalty, int mazeConnPenalty, bool allowNonTerminal) {
        _LOG_DEBUGH(_T("ENTER: str={}"), to_wstr(str));
        const size_t BUFSIZE = 10000;
        std::vector<wchar_t> wchbuf(BUFSIZE, L'\0');
        const int ARRAY_SIZE = 1024;
        wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };
        int cost = DymazinAnalyze(to_wstr(str).c_str(), wchbuf.data(), BUFSIZE, mazePenalty, mazeConnPenalty, allowNonTerminal, false, errMsgBuf, ARRAY_SIZE);
        _LOG_DEBUGH(_T("wchbuf:\n----------------\n{}\n----------------"), wchbuf.data());
        for (const auto& s : utils::split(wchbuf.data(), L'\n')) {
            morphs.push_back(to_mstr(s));
        }
        _LOG_DEBUGH(_T("LEAVE: dymazinCost={}, morphs={}"), cost, to_wstr(utils::join(morphs, ' ')));
        return cost;
    }

}
