#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <windows.h>
#include "DyMazinLib/src/DyMazinLib.h"
#include "reporting/ErrorHandler.h"

#include "util/regex_utils.h"

namespace {
    /**
    * Convert an UTF8 string to a wide Unicode String
    */
    inline std::wstring utf8_decode(const std::string& str)
    {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo((size_t)size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }
}

int main(int argc, const char** argv) {
    std::vector<std::wstring> args;

    for (int i = 0; i < argc; ++i) {
        args.push_back(utf8_decode(argv[i]));
    }

    String logFile = RegexUtil(L"exe$").replace(args[0], L"log");

    size_t ac = 0;
    std::vector<const wchar_t*> av;
    for (const auto& ws : args) {
        av.push_back(ws.c_str());
        ++ac;
    }

    int result = 0;

    const int ARRAY_SIZE = 1024;
    wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };

    if (args.size() > 1 && RegexUtil(L"^make[_\\-]dict").search(args[1])) {
        result = DymazinMakeDictIndex(ac - 1, av.data() + 1, logFile.c_str(), errMsgBuf, ARRAY_SIZE, true);
        if (result == ErrorHandler::LEVEL_ERROR) {
            std::cerr << "make_dict_index: result=" << result << std::endl;
        }
    } else if (args.size() > 1 && RegexUtil(L"^expand").search(args[1])) {
        result = DymazinExpandDictLine(ac - 1, av.data() + 1, logFile.c_str(), errMsgBuf, ARRAY_SIZE, true);
        if (result == ErrorHandler::LEVEL_ERROR) {
            std::cerr << "expand line: result=" << result << std::endl;
        }
    } else if (args.size() > 1 && RegexUtil(L"^test").search(args[1])) {
        result = DymazinInitialize(ac - 1, av.data() + 1, logFile.c_str(), errMsgBuf, ARRAY_SIZE, true);
        if (result != ErrorHandler::LEVEL_ERROR) {
            DymazinAnalyze(L"私は学生です。", nullptr, 0, 0, 0, true, true, errMsgBuf, ARRAY_SIZE);
        } else {
            std::cerr << "initialize: result=" << result << std::endl;
        }
    } else {

        result = DymazinInitialize(ac, av.data(), logFile.c_str(), errMsgBuf, ARRAY_SIZE, true);

        if (result != ErrorHandler::LEVEL_ERROR) {
            //std::cout << "CALL DymazinAnalyze" << std::endl;
            //DymazinAnalyze(L"私は学生です。", nullptr, 0, 0, true);
            DymazinAnalyze(nullptr, nullptr, 0, 0, 0, true, true, errMsgBuf, ARRAY_SIZE);
        } else {
            std::cerr << "initialize: result=" << result << std::endl;
        }
    }

    DymazinSaveLog(errMsgBuf, ARRAY_SIZE);

    //std::cout << "CALL DymazinFinalize" << std::endl;
    DymazinFinalize(errMsgBuf, ARRAY_SIZE);

    return result;
}