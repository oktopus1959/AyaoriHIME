#include "DyMazinLib.h"
#include "reporting/Logger.h"
#include "reporting/ErrorHandler.h"
#include "exception.h"

#include "OptHandler.h"
#include "analyzer/Model.h"
#include "analyzer/Tagger.h"
#include "compiler/DictionaryBuilder.h"
#include "dict/MazegakiPreprocessor.h"

#include "file_utils.h"

using namespace analyzer;
using namespace compiler;
using Logger = Reporting::Logger;

#if 0 || defined(_DEBUG)
#define _LOG_DEBUGH_FLAG true
#undef _LOG_DEBUGH
#define _LOG_DEBUGH LOG_INFOH
#else
#define _LOG_DEBUGH_FLAG false
#endif

namespace {
    DEFINE_LOCAL_LOGGER(DyMazinLib);

    OptHandlerPtr opts;

    ModelPtr model;

    TaggerPtr tagger;

    bool bShowError = false;

    void printError(const RuntimeException& ex) {
        std::wcerr << ex.getMessage() << std::endl;
    }

    void doOneSentence() {

    }
}

// 初期化
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int DymazinInitialize(size_t argc, const wchar_t** argv, const wchar_t* logFilePath, wchar_t* errMsgBuf, size_t bufsiz, bool showError) {
    ERROR_HANDLER->Clear();
    bShowError = showError;

    String _logFilePath = logFilePath ? logFilePath : L"DyMazinLib.log";
    opts = util::OptHandler::CreateOptHandler(argc, argv, _logFilePath.c_str());


    if (opts) {
        try {
            LOG_INFOH(L"ENTER: showError={}", showError);

            LOG_INFOH(L"rcfile={}", opts->getValue(L"rcfile"));
            LOG_INFOH(L"dicdir={}", opts->getValue(L"dicdir"));

            opts->loadDictionaryResource();
            if (!ERROR_HANDLER->HasError()) {
                model = MakeShared<Model>(opts);
                tagger = MakeShared<Tagger>(model);
            }
        } catch (RuntimeException ex) {
            ERROR_HANDLER->Error(ex.getMessage());
            if (showError) printError(ex);
        } catch (...) {
            auto msg = L"Unknown exception occurred";
            LOG_ERROR(msg);
            ERROR_HANDLER->Error(msg);
            if (showError) std::wcerr << msg << std::endl;
        }
    } else {
        ERROR_HANDLER->Error(L"Failed to parse arguments: logFilePath: " + _logFilePath);
        if (showError) std::wcerr << ERROR_HANDLER->GetErrorMsg() << std::endl;
    }

    if (ERROR_HANDLER->HasError()) {
        return ERROR_HANDLER->GetErrorInfo(errMsgBuf, bufsiz);
    }

    LOG_INFOH(L"LEAVE: SUCCESS");
    return 0;
}

namespace {
    // ユーザー辞書の再ロード
    // @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
    int reloadUserDics(wchar_t* errMsgBuf, size_t bufsiz) {
        LOG_INFOH(L"ENTER");

        try {
            if (model) {
                // ユーザー辞書の再ロード
                model->reload_userdics();
            } else {
                ERROR_HANDLER->Warn(L"model not created");
            }
        } catch (RuntimeException ex) {
                ERROR_HANDLER->Error(ex.getMessage());
        } catch (...) {
            auto msg = L"Unknown exception occurred";
            LOG_ERROR(msg);
            ERROR_HANDLER->Error(msg);
        }

        LOG_INFOH(L"LEAVE: SUCCESS");
        return ERROR_HANDLER->GetErrorInfo(errMsgBuf, bufsiz);
    }
}
// ユーザー辞書の再ロード
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int DymazinReloadUserDics(wchar_t* errMsgBuf, size_t bufsiz) {
    ERROR_HANDLER->Clear();
    return reloadUserDics(errMsgBuf, bufsiz);
}

// ユーザー辞書のコンパイルとロード
// @param filePath ユーザー辞書のファイルパス
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int DymazinCompileAndLoadUserDic(const wchar_t* dicDir, const wchar_t* srcFilePath, const wchar_t* outputFilePath, wchar_t* errMsgBuf, size_t bufsiz) {
    ERROR_HANDLER->Clear();

    Vector<String> dic_lines = MazegakiPreprocessor::preprocessMazegakiDic(dicDir, srcFilePath);
    if (dic_lines.empty() || ERROR_HANDLER->HasError()) {
        return ERROR_HANDLER->GetErrorInfo(errMsgBuf, bufsiz);
    }

    opts = util::OptHandler::CreateDefaultHandler(L"make_user_dict");
    opts->set(L"dicdir", dicDir);
    // ユーザー辞書のコンパイル
    int result = DictionaryBuilder::make_user_dict(opts, dic_lines, utils::joinPath(dicDir, outputFilePath));
    if (result != 0) {
        ERROR_HANDLER->Error(std::format(L"Failed to compile user dictionary: {}", srcFilePath));
        return ERROR_HANDLER->GetErrorInfo(errMsgBuf, bufsiz);
    }
    return reloadUserDics(errMsgBuf, bufsiz);
}

// 辞書行の展開(標準出力へ出力)
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int DymazinExpandDictLine(size_t argc, const wchar_t** argv, const wchar_t* logFilePath, wchar_t* errMsgBuf, size_t bufsiz, bool showError) {
    ERROR_HANDLER->Clear();

    opts = util::OptHandler::CreateOptHandler(argc, argv, logFilePath);

    String mazeResrcDir = L"/Dev/Text/kanji-table";
    String filePath = (opts && !opts->restArgs().empty()) ? opts->restArgs().front() : L"-";

    Vector<String> results = MazegakiPreprocessor::preprocessMazegakiDic(mazeResrcDir, filePath);
    if (ERROR_HANDLER->HasError()) {
        if (showError) std::wcerr << ERROR_HANDLER->GetErrorMsg() << std::endl;
        return ERROR_HANDLER->GetErrorInfo(errMsgBuf, bufsiz);
    }
    for (const auto& result : results) {
        std::cout << utils::utf8_encode(result) << std::endl;
    }
    LOG_INFOH(L"LEAVE");
    return ERROR_HANDLER->GetErrorInfo(errMsgBuf, bufsiz);
}

// 辞書の作成
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int DymazinMakeDictIndex(size_t argc, const wchar_t** argv, const wchar_t* logFilePath, wchar_t* errMsgBuf, size_t bufsiz, bool showError) {
    ERROR_HANDLER->Clear();
    bShowError = showError;

    opts = util::OptHandler::CreateOptHandler(argc, argv, logFilePath);

    if (!opts) {
        if (showError) std::wcerr << "logFilePath: " << logFilePath << std::endl;
        return 1;
    }

    try {
        LOG_INFOH(L"ENTER: showError={}", showError);
        DictionaryBuilder::make_dict_index(opts);
        if (ERROR_HANDLER->HasError()) {
            if (showError) std::wcerr << ERROR_HANDLER->GetErrorMsg() << std::endl;
        }
    } catch (RuntimeException ex) {
        ERROR_HANDLER->Error(ex.getMessage());
        if (showError) printError(ex);
    } catch (...) {
        auto msg = L"Unknown exception occurred";
        LOG_ERROR(msg);
        ERROR_HANDLER->Error(msg);
        if (showError) std::wcerr << msg << std::endl;
    }

    LOG_INFOH(L"LEAVE");
    return ERROR_HANDLER->GetErrorInfo(errMsgBuf, bufsiz);
}

/**
 * 形態素解析の実行(コストを返す)
 * @param mazePenalty 交ぜ書きエントリに対するペナルティ(0 ならデフォルト値を使う)
 * @return 解のコスト(非負値; 実行時エラーがある場合は負値を返す)
 */
int DymazinAnalyze(const wchar_t* sentence, wchar_t* wakati_buf, size_t bufsize, int mazePenalty, int mazeConnPenalty, bool allowNonTerminal, bool bStdout, wchar_t* errMsgBuf, size_t bufsiz) {
    LOG_INFO(L"ENTER: sentence={}, mazePenalty={}, mazeConnPenalty={}, allowNonTerminal={}", sentence ? sentence : L"null", mazePenalty, mazeConnPenalty, allowNonTerminal);
    ERROR_HANDLER->Clear();

    try {
        int cost = 0;
        int nBest = opts->getInt(L"nbest", 1);
        if (mazePenalty == 0) {
            mazePenalty = opts->getInt(L"maze-penalty");
            LOG_INFO(L"mazePenalty={}", mazePenalty);
        }
        if (mazeConnPenalty == 0) {
            mazeConnPenalty = opts->getInt(L"maze-conn-penalty");
            LOG_INFO(L"mazeConnPenalty={}", mazeConnPenalty);
        }
        if (sentence) {
            if (wakati_buf) wakati_buf[0] = L'\0';
            Vector<String> results;
            cost = tagger->parseNBest(sentence, results, nBest, mazePenalty, mazeConnPenalty, allowNonTerminal);
            bool bFirst = true;
            for (const auto& result : results) {
                if (wakati_buf && bFirst) {
                    wcsncpy_s(wakati_buf, bufsize, result.c_str(), _TRUNCATE);
                }
                if (bStdout) {
                    std::cout << utils::utf8_encode(result) << std::endl;
                    if (bFirst) {
                        std::cout << "totalCost: " << cost << std::endl;
                    }
                }
                bFirst = false;
            }
        } else {
            String filePath = (opts && !opts->restArgs().empty()) ? opts->restArgs().front() : L"-";
            utils::IfstreamReader reader(filePath);
            while (true) {
                auto [line, eof] = reader.getLine();
                _LOG_DEBUGH(L"line={}, eof={}", line, eof);
                if (eof || (line == L"." && filePath == L"-")) break;
                //if (line.empty()) continue;
                //String result = tagger->parse(line, nBest, mazePenalty);
                Vector<String> results;
                tagger->parseNBest(line, results, nBest, mazePenalty, mazeConnPenalty, allowNonTerminal);
                for (const auto& result : results) {
                    std::cout << "----------------" << std::endl;
                    std::cout << utils::utf8_encode(result) << std::endl;
                }
                std::cout << "----------------------------------------------------------------" << std::endl;
            }
        }
        if (ERROR_HANDLER->HasError()) {
            if (bShowError) std::wcerr << ERROR_HANDLER->GetErrorMsg() << std::endl;
            LOG_INFO(L"LEAVE: ERROR");
            return ERROR_HANDLER->GetErrorInfo(errMsgBuf, bufsiz);
        }
        LOG_INFO(L"LEAVE: cost={}", cost);
        return cost;
    } catch (RuntimeException ex) {
        ERROR_HANDLER->Error(ex.getMessage());
        if (bShowError) printError(ex);
    } catch (...) {
        auto msg = L"Unknown exception occurred";
        LOG_ERROR(msg);
        ERROR_HANDLER->Error(msg);
        if (bShowError) std::wcerr << msg << std::endl;
    }
    return ERROR_HANDLER->GetErrorInfo(errMsgBuf, bufsiz);
}

void DymazinSetLogLevel(int logLevel) {
    ERROR_HANDLER->Clear();
    Reporting::Logger::SetLogLevel(logLevel);
}

void DymazinSaveLog(wchar_t* errMsgBuf, size_t bufsiz) {
    ERROR_HANDLER->Clear();
    Reporting::Logger::SaveLog();
}

// 終了
// @return ErrorLevel 0: 成功, -1: 成功(情報メッセージあり), -2: 警告, -3: エラー
int DymazinFinalize(wchar_t* errMsgBuf, size_t bufsiz) {
    LOG_INFOH(L"\n==== END DyMazinLib ====\n");
    ERROR_HANDLER->Clear();
    Logger::Close();
    return 0;
}

