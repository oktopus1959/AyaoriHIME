#include "path_utils.h"

#include "Reporting/Logger.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <vector>

#if 0
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

    namespace {
        struct TimedBackFile {
            String path;
            std::time_t timestamp;
        };

        // std::filesystem::file_time_type をローカル時刻基準の time_t に寄せる
        bool getFileLastWriteTime(StringRef path, std::time_t& out) {
            try {
                auto fileTime = std::filesystem::last_write_time(path);
                auto systemNow = std::chrono::system_clock::now();
                auto fileNow = decltype(fileTime)::clock::now();
                auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    fileTime - fileNow + systemNow);
                out = std::chrono::system_clock::to_time_t(systemTime);
                return true;
            } catch (...) {
                return false;
            }
        }

        // バックアップファイル名に埋め込む yyyyMMdd-HHmm 文字列を生成する
        String makeBackupTimestampString(std::time_t timestamp) {
            std::tm tm = {};
            if (localtime_s(&tm, &timestamp) != 0) return {};
            return std::format(L"{:04d}{:02d}{:02d}-{:02d}{:02d}",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
        }

        // バックアップファイル名末尾の yyyyMMdd-HHmm を取り出して時刻に戻す
        bool tryParseBackupTimestamp(StringRef name, std::time_t& out) {
            if (name.size() < 14 || name[name.size() - 14] != L'.') return false;
            auto ts = name.substr(name.size() - 13);
            if (ts[8] != L'-') return false;
            if (!std::all_of(ts.begin(), ts.begin() + 8, [](wchar_t ch) { return iswdigit(ch) != 0; })) return false;
            if (!std::all_of(ts.begin() + 9, ts.end(), [](wchar_t ch) { return iswdigit(ch) != 0; })) return false;

            std::tm tm = {};
            tm.tm_year = std::stoi(ts.substr(0, 4)) - 1900;
            tm.tm_mon = std::stoi(ts.substr(4, 2)) - 1;
            tm.tm_mday = std::stoi(ts.substr(6, 2));
            tm.tm_hour = std::stoi(ts.substr(9, 2));    // '-' をはさんでいるので、時刻部分は 9 文字目から
            tm.tm_min = std::stoi(ts.substr(11, 2));
            tm.tm_sec = 0;
            tm.tm_isdst = -1;

            auto parsed = std::mktime(&tm);
            if (parsed == static_cast<std::time_t>(-1)) return false;
            out = parsed;
            return true;
        }
    }

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

    // ファイルを back ディレクトリに移動
    // - ファイル名を {path}.yyyyymmdd-hhmm の形式にして保存する
    // - バックアップは、3時間前、6時間前、12時間前、24時間前、1週間前、1ヶ月前のファイルを残す
    // - ただし、直近3時間以内のバックアップは最新 genNum 件と最古の 1 件を残す
    // - bCopy が true の場合はコピー、false の場合は移動する
    bool moveFileToBackDirWithRotationEx(StringRef path, int genNum, bool bCopy) {
        // 退避元ファイルの更新日時をバックアップ名に使う
        if (!utils::isFileExistent(path)) return false;
        std::time_t lastWriteTime = 0;
        if (!getFileLastWriteTime(path, lastWriteTime)) return false;

        // back ディレクトリを用意し、更新日時付きのファイル名で退避する
        auto backDirPath = utils::joinPath(utils::getParentDirPath(path), L"back");
        if (!utils::makeDirectory(backDirPath)) return false;

        auto fileName = utils::getFileName(path);
        auto timestampStr = makeBackupTimestampString(lastWriteTime);
        if (timestampStr.empty()) return false;
        auto backupPath = utils::joinPath(backDirPath, fileName + L"." + timestampStr);
        if (bCopy) {
            utils::copyFile(path, backupPath);
        } else {
            utils::moveFile(path, backupPath);
        }

        if (!utils::isFileExistent(backupPath)) return false;

        // 同一ファイルのバックアップ群だけを集め、時刻の新しい順に並べる
        std::vector<TimedBackFile> files;
        try {
            auto prefix = fileName + L".";
            for (const auto& entry : std::filesystem::directory_iterator(backDirPath)) {
                if (!entry.is_regular_file()) continue;
                auto name = entry.path().filename().wstring();
                if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0) continue;

                std::time_t ts = 0;
                if (tryParseBackupTimestamp(name, ts)) {
                    files.push_back({ entry.path().wstring(), ts });
                }
            }
        } catch (...) {
            return true;
        }

        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
            return a.timestamp > b.timestamp;
        });

        auto now = std::time(nullptr);
        std::vector<String> keepPaths;
        keepPaths.reserve((genNum > 0 ? genNum : 0) + 16);

        // 保持対象は重複登録しない
        auto addKeepPath = [&](StringRef pathToKeep) {
            if (std::find(keepPaths.begin(), keepPaths.end(), pathToKeep) == keepPaths.end()) {
                keepPaths.push_back(pathToKeep);
            }
        };

        // 指定した経過時間帯に入るバックアップだけを抽出する
        auto collectBandFiles = [&](double lowerBoundSec, double upperBoundSec) {
            std::vector<const TimedBackFile*> bandFiles;
            for (const auto& file : files) {
                auto ageSec = std::difftime(now, file.timestamp);
                if (ageSec < 0) ageSec = 0;
                if (ageSec >= lowerBoundSec && (upperBoundSec < 0 || ageSec < upperBoundSec)) {
                    bandFiles.push_back(&file);
                }
            }
            return bandFiles;
        };

        // 直近3時間は最新 genNum 件に加えて、その帯域で最古の 1 件も残す
        auto recentFiles = collectBandFiles(0, 3 * 3600.0);
        for (int i = 0; i < genNum && i < static_cast<int>(recentFiles.size()); ++i) {
            addKeepPath(recentFiles[i]->path);
        }
        if (!recentFiles.empty()) {
            addKeepPath(recentFiles.back()->path);
        }

        // 各帯域では最新の 1 件と最古の 1 件を残す
        constexpr std::array<double, 8> hourBounds = { 3, 6, 12, 24, 24 * 7, 24 * 30 };
        double prevBoundSec = 3 * 3600.0;
        for (size_t i = 1; i < hourBounds.size(); ++i) {
            const double upperBoundSec = hourBounds[i] * 3600.0;
            auto bandFiles = collectBandFiles(prevBoundSec, upperBoundSec);
            if (!bandFiles.empty()) {
                addKeepPath(bandFiles.front()->path);
                addKeepPath(bandFiles.back()->path);
            }
            prevBoundSec = upperBoundSec;
        }

        // 1ヶ月より古い領域も、最新と最古を 1 件ずつ残す
        auto oldFiles = collectBandFiles(24 * 30 * 3600.0, -1);
        if (!oldFiles.empty()) {
            addKeepPath(oldFiles.front()->path);
            addKeepPath(oldFiles.back()->path);
        }

        // 保持対象に入らなかったバックアップを削除する
        for (const auto& file : files) {
            if (std::find(keepPaths.begin(), keepPaths.end(), file.path) == keepPaths.end()) {
                utils::removeFileIfExists(file.path);
            }
        }
        return true;
    }

    // 新旧ファイルのサイズを比較して、新サイズ+1024 > 旧サイズ または 新サイズ > 旧サイズ * 0.7 の場合のみ、
    // 旧ファイルを back ディレクトリに移動してから新ファイルを差し替える
    bool compareAndMoveFileToBackDirWithRotation(StringRef pathTmp, StringRef pathCurrent, int genNum, bool bCopy) {
        LOG_INFOH(_T("CALLED: pathTmp={}, pathCurrent={}, genNum={}, bCopy={}"), pathTmp, pathCurrent, genNum, bCopy);
        uintmax_t tmpSize = utils::getFileSize(pathTmp);
        uintmax_t oldSize = utils::getFileSize(pathCurrent);
        if (tmpSize > 0 && (tmpSize + 1024 > oldSize || tmpSize * 10 > oldSize * 7)) {
            if (genNum > 0) {
                // 旧ファイルのバックアップ
                if (!utils::moveFileToBackDirWithRotationEx(pathCurrent, genNum, bCopy)) {
                    // 旧ファイルのバックアップに失敗
                    LOG_WARN(_T("Failed to move old file to back dir: tmpSize={}, oldSize={}"), tmpSize, oldSize);
                    return false;
                }
            }
            // 本ファイル差し替え
            utils::moveFile(pathTmp, pathCurrent);
            LOG_INFOH(_T("File replaced: tmpSize={}, oldSize={}"), tmpSize, oldSize);
            return true;
        } else {
            LOG_WARN(_T("New file is NOT larger than old file: tmpSize={}, oldSize={}"), tmpSize, oldSize);
            //// 差し替え不要なので、tmpファイルを削除
            //utils::removeFileIfExists(pathTmp);
        }
        return false;
    }

} // namespace utils
