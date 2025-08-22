#include "Logger.h"
#include "DymazinBridge.h"
#include "DyMazinLib.h"
#include "Settings/Settings.h"

#include "path_utils.h"

#if 1
#undef _LOG_DEBUGH
#if 1
#define _LOG_INFOH LOG_WARNH
#define _LOG_DEBUGH LOG_INFOH
#else
#define _LOG_INFOH LOG_WARN
#define _LOG_DEBUGH LOG_INFOH
#endif
#endif

namespace DymazinBridge {
    DEFINE_LOCAL_LOGGER(DymazinBridge);

    int dymazinInitialize(StringRef rcfile, StringRef dicdir, int unkMax, int mazePenalty, int nonTerminalCost) {
        _LOG_INFOH(_T("ENTER: rcfile={}, dicdir={}, unkMax={}, mazePenalty={}, nonTerminalCost={}, -O{}"),
            rcfile, dicdir, unkMax, mazePenalty, nonTerminalCost, SETTINGS->morphMazeFormat);

        std::vector<const wchar_t*> av;
        av.push_back(L"-r");
        av.push_back(rcfile.c_str());
        av.push_back(L"-d");
        av.push_back(dicdir.c_str());
        av.push_back(L"--userdic");
        av.push_back(utils::join_path(dicdir, L"user.dic").c_str());
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
        String nonTerminalCostOpt(L"--non-terminal-cost=");
        nonTerminalCostOpt.append(std::to_wstring(nonTerminalCost));
        av.push_back(nonTerminalCostOpt.c_str());
        av.push_back(L"--ignore-eos");
        int result = DymazinInitialize(av.size(), av.data(), L"dymazin.log");

        _LOG_INFOH(_T("LEAVE: result={}"), result);
        return result;
    }

    void dymazinFinalize() {
        _LOG_INFOH(_T("CALLED"));
        DymazinFinalize();
    }

    void dymazinSetLogLevel(int logLevel) {
        _LOG_INFOH(_T("CALLED: logLevel={}"), logLevel);
        DymazinSetLogLevel(logLevel);
    }

    void dymazinSaveLog() {
        _LOG_INFOH(_T("CALLED"));
        DymazinSaveLog();
    }

#if true
    int dymazinCalcCost(const MString& str, std::vector<MString>& words, int mazePenalty, bool allowNonTerminal) {
        _LOG_DEBUGH(_T("ENTER: str={}"), to_wstr(str));
        const size_t BUFSIZE = 1000;
        wchar_t wchbuf[BUFSIZE] = { '\0' };
        int cost = DymazinAnalyze(to_wstr(str).c_str(), wchbuf, BUFSIZE, mazePenalty, allowNonTerminal, false);
        for (const auto& s : utils::split(wchbuf, L'\n')) {
            words.push_back(to_mstr(s));
        }
        _LOG_DEBUGH(_T("LEAVE: dymazinCost={}, words={}"), cost, to_wstr(utils::join(words, ' ')));
        return cost;
    }
#else
    int dymazinCalcCost(const MString& str, std::vector<MString>& words) {
        return 10000;
    }
#endif
}
