#include "path_utils.h"

#include "Reporting/Logger.h"

#if 1
#undef LOG_WARN
#undef LOG_INFOH
#undef LOG_INFO
#undef LOG_DEBUGH
#undef LOG_DEBUG
#define LOG_WARN LOG_WARNH
#define LOG_INFOH LOG_WARNH
#define LOG_INFO LOG_WARNH
#define LOG_DEBUGH LOG_WARNH
#define LOG_DEBUG LOG_WARNH
#endif
namespace utils {
    DEFINE_NAMESPACE_LOGGER(utils);

    // ファイルを back ディレクトリに移動(backファイルのローテートもやる)
    bool moveFileToBackDirWithRotation(StringRef path, int genNum, bool bCopy) {
        // path と同じ階層に back ディレクトリを作成
        auto backDirPath = utils::joinPath(utils::getParentDirPath(path), L"back");
        if (!utils::makeDirectory(backDirPath)) return false;

        // backファイルのローテーション
        auto backFilePath = utils::joinPath(backDirPath, utils::getFileName(path)) + L".";
        while (genNum > 1) {
            utils::moveFile(sx::catAny(backFilePath, genNum - 1), sx::catAny(backFilePath, genNum));
            --genNum;
        }
        // path を back/filename.1 に move or copy
        if (bCopy)
            utils::copyFile(path, sx::catAny(backFilePath, 1));
        else
            utils::moveFile(path, sx::catAny(backFilePath, 1));
        return true;
    }

    bool compareAndMoveFileToBackDirWithRotation(StringRef pathTmp, StringRef pathCurrent, int genNum, bool bCopy) {
        LOG_INFOH(_T("CALLED: pathTmp={}, pathCurrent={}, genNum={}, bCopy={}"), pathTmp, pathCurrent, genNum, bCopy);
        uintmax_t tmpSize = utils::getFileSize(pathTmp);
        uintmax_t oldSize = utils::getFileSize(pathCurrent);
        if (tmpSize > 0 && tmpSize + 1024 > oldSize) {
            // 旧ファイルのバックアップ
            if (utils::moveFileToBackDirWithRotation(pathCurrent, genNum, bCopy)) {
                // 旧ファイルのバックアップ成功後に本ファイル差し替え
                utils::moveFile(pathTmp, pathCurrent);
                LOG_INFOH(_T("File replaced: tmpSize={}, oldSize={}"), tmpSize, oldSize);
                return true;
            } else {
                LOG_WARN(_T("Failed to move old file to back dir: tmpSize={}, oldSize={}"), tmpSize, oldSize);
            }
        } else {
            LOG_WARN(_T("New file is NOT larger than old file: tmpSize={}, oldSize={}"), tmpSize, oldSize);
            //// 差し替え不要なので、tmpファイルを削除
            //utils::removeFileIfExists(pathTmp);
        }
        return false;
    }

} // namespace utils
