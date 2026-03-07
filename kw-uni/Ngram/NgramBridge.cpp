#include "Logger.h"
#include "NgramBridge.h"
#include "../NgramAnalyzer/NgramCoreLib/src/NgramCoreLib.h"
#include "Settings/Settings.h"
#include "Reporting/ErrorHandler.h"

#include "path_utils.h"

#if 1
#undef _LOG_DEBUGH
#define _LOG_DEBUGH LOG_INFOH
#endif

#define NGRAM_DICDIR  JOIN_SYSTEM_FILES_FOLDER(_T("ngram/dic"))

namespace NgramBridge {
    DEFINE_LOCAL_LOGGER(NgramBridge);

    bool initializeSucceeded = false;

    int _ngramInitialize(StringRef dicdir, int unkMax) {
        LOG_INFOH(_T("ENTER: dicdir={}, unkMax={}"), dicdir, unkMax);

        std::vector<const wchar_t*> av;
        av.push_back(L"ngramer");
        av.push_back(L"-d");
        av.push_back(dicdir.c_str());
        if (SETTINGS->hiraganaBigramEnabled) {
            av.push_back(L"--hiragana-bigram");
        }

        const int ARRAY_SIZE = 1024;
        wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };

        int result = NgramCoreLib::NgramInitialize(av.size(), av.data(), L"ngram.log", errMsgBuf, ARRAY_SIZE);

        ERROR_HANDLER->SetErrorInfo(result, errMsgBuf);
        LOG_INFOH(_T("LEAVE: result={}, errorMsg={}"), result, errMsgBuf);
        return result;
    }

    void ngramInitialize() {
        int unkMax = 3;
        auto dicdir = utils::joinPath(SETTINGS->rootDir, NGRAM_DICDIR);
        LOG_INFOH(_T("ENTER: dicdir={}, unMax={}"), dicdir, unkMax);
        int ret = _ngramInitialize(dicdir, unkMax);
        initializeSucceeded = (ret != ErrorHandler::LEVEL_ERROR);
        if (!initializeSucceeded) {
            LOG_ERROR(_T("NgramAnalyzer Initialize FAILED: dicdir={}, unMax={}"), dicdir, unkMax);
            auto detailedMsg = ERROR_HANDLER->GetErrorMsg();
            ERROR_HANDLER->Clear();
            ERROR_HANDLER->Error(L"Ngram解析器の初期化に失敗しました。\r\n詳細: " + detailedMsg);
        }
        LOG_INFOH(L"LEAVE");
    }

    void ngramFinalize() {
        LOG_INFOH(_T("CALLED"));
        const int ARRAY_SIZE = 1024;
        wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };
        NgramCoreLib::NgramFinalize(errMsgBuf, ARRAY_SIZE);
    }

    void ngramReopenUserDics() {
        if (initializeSucceeded) {
            LOG_INFOH(_T("ENTER"));
            const int ARRAY_SIZE = 1024;
            wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };
            int result = NgramCoreLib::NgramReloadUserDics(errMsgBuf, ARRAY_SIZE);
            ERROR_HANDLER->SetErrorInfo(result, errMsgBuf);
            LOG_INFOH(_T("LEAVE: result={}, errorMsg={}"), result, errMsgBuf);
            return;
        }
    }

    int _ngramCompileAndLoadUserDic(StringRef dicDir, StringRef srcFilePath) {
        LOG_INFOH(_T("ENTER"));
        const int ARRAY_SIZE = 1024;
        wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };
        String userDic = utils::join_path(dicDir, L"user.dic");
        int result = NgramCoreLib::NgramCompileAndLoadUserDic(dicDir.c_str(), srcFilePath.c_str(), userDic.c_str(), errMsgBuf, ARRAY_SIZE);
        if (result < 0) ERROR_HANDLER->SetErrorInfo(result, errMsgBuf);
        LOG_INFOH(_T("LEAVE: result={}, errorMsg={}"), result, errMsgBuf);
        return result;
    }

    int ngramCompileAndLoadUserDic(StringRef dicDir, StringRef filePath) {
        LOG_INFO(_T("CALLED: dicDir={}, filePath={}"), dicDir, filePath);
        if (!initializeSucceeded) {
            return ERROR_HANDLER->Warn(L"Ngram解析器の初期化に失敗しています。");
        }
        int result = _ngramCompileAndLoadUserDic(dicDir, filePath);
        auto detailedMsg = ERROR_HANDLER->GetErrorMsg();
        ERROR_HANDLER->Clear();
        if (result == ErrorHandler::LEVEL_ERROR) {
            ERROR_HANDLER->Error(L"ユーザーNgram辞書のコンパイルと読み込みに失敗しました: file=" + filePath + L"\r\n詳細: " + detailedMsg);
        } else {
            ERROR_HANDLER->Info(L"ユーザーNgram辞書をインポート: file=" + filePath);
            if (!detailedMsg.empty()) {
                ERROR_HANDLER->Info(detailedMsg);
            }
        }
        return result;
    }

    void ngramSetLogLevel(int logLevel) {
        LOG_INFOH(_T("CALLED: logLevel={}"), logLevel);
        NgramCoreLib::NgramSetLogLevel(logLevel);
    }

    void ngramSaveLog() {
        LOG_INFOH(_T("CALLED"));
        const int ARRAY_SIZE = 1024;
        wchar_t errMsgBuf[ARRAY_SIZE] = { 0 };
        NgramCoreLib::NgramSaveLog(errMsgBuf, ARRAY_SIZE);
    }

    // Ngramの一時的な辞書エントリのために、形態素解析の結果から、未知語や交ぜ書き候補を除いた主要な形態素を抽出する
    String pickMainMorphs(const std::vector<MString>& morphs) {
        MString mainMorphs;
        MString unkMarker = to_mstr(L":未知");
        MString mazeMarker = to_mstr(L"MAZE");
        bool first = true;
        for (const auto& morph : morphs) {
            auto items = utils::split(morph, '\t');
            if (items.size() > 2) {
                const MString& surf = items[0];
                // かな配列だけの場合は、ひらがなのみの形態素や交ぜ書き候補も含める。
                if (surf.size() >= 2 && (SETTINGS->isHiraganaTableOnly || utils::contains_kanji(surf))) {
                    const MString& feat = items[2];
                    if (feat.find(unkMarker) == std::string::npos && (SETTINGS->isHiraganaTableOnly || !utils::endsWith(feat, mazeMarker))) {
                        if (!first) mainMorphs.append(MSTR_VERT_BAR);
                        first = false;
                        mainMorphs.append(surf);
                    }
                }
            }
        }
        _LOG_DEBUGH(_T("RESULT: mainMorphs={}"), to_wstr(mainMorphs));
        return to_wstr(mainMorphs);
    }

    // リアルタイムNgram辞書のパラメータ設定
    void setRealtimeDictParameters(size_t minNgramLen, size_t maxNgramLen, int maxBonusPoint, int bonusPointFactor) {
        NgramCoreLib::SetRealtimeDictParameters(minNgramLen, maxNgramLen, maxBonusPoint, bonusPointFactor);
    }

    // リアルタイムNgram辞書のロード
    int loadRealtimeDict(StringRef ngramFilePath) {
        return NgramCoreLib::LoadRealtimeDict(ngramFilePath.c_str());
    }

    // ユーザーNgram辞書のロード
    int loadUserNgram(StringRef ngramFilePath) {
        return NgramCoreLib::LoadUserNgram(ngramFilePath.c_str());
    }

    // リアルタイムNgramファイルの保存
    // @param ngramFilePath リアルタイムNgramファイルのパス
    // @param rotationNum 過去履歴の保存数 (0 なら保存しない)
    void saveRealtimeDict(StringRef ngramFilePath, int rotationNum) {
        NgramCoreLib::SaveRealtimeDict(ngramFilePath, rotationNum);
    }

    // リアルタイムNgramエントリの更新
    // @param delta 加算する値
    // @return 更新後のエントリの値
    int updateRealtimeEntry(const String& word, int delta) {
        return NgramCoreLib::UpdateRealtimeEntry(word, delta);
    }

    int ngramCalcCost(const MString& str, const std::vector<MString>& tempDictEntries, std::vector<MString>& ngrams, bool needNgrams) {
        if (!initializeSucceeded) return 0;

        _LOG_DEBUGH(_T("ENTER: str={}"), to_wstr(str));
        std::vector<String> results;
        String  errMsg;
        int cost = NgramCoreLib::NgramAnalyze(to_wstr(str), pickMainMorphs(tempDictEntries), results, errMsg, needNgrams);
        if (cost < 0) {
            LOG_WARN(_T("NgramAnalyze FAILED: result={}, errMsg={}"), cost, errMsg);
            return cost;
        }
        //int cost = NgramAnalyze(to_wstr(str).c_str(), wchbuf, BUFSIZE, mazePenalty, allowNonTerminal, false);
        if (needNgrams) {
            for (const auto& s : results) {
                ngrams.push_back(to_mstr(s));
            }
        }
        _LOG_DEBUGH(_T("LEAVE: ngramCost={}, ngrams={}"), cost, to_wstr(utils::join(ngrams, ' ')));
        return cost;
    }
}
