#pragma once

#include "string_utils.h"

namespace utils {
    // 正規化されたパスを返す
    // 何かエラーになったら元のパスを返す
    inline String canonicalizePath(const std::filesystem::path& path) {
        try {
            return std::filesystem::weakly_canonical(path).wstring();
        }
        catch (...) {
            return path;
        }
    }

    // 正規化されたパスを返す
    // 何かエラーになったら元のパスを返す
    inline String canonicalizePath(StringRef path) {
        return canonicalizePath(std::filesystem::path(path));
    }

    // path の親ディレクトリを返す
    inline String getParentDirPath(StringRef path) {
        return std::filesystem::path(path).parent_path().wstring();
    }

    // path のファイル名の部分を返す
    inline String getFileName(StringRef path) {
        auto p = std::filesystem::path(path);
        if (p.has_filename()) {
            return p.filename().wstring();
        } else {
            return path;
        }
    }

    /** ディレクトリを作成する.
     * 複数階層に渡ってディレクトリを作成する.
     * @param path ディレクトリパス (PathStringType 文字列)
     * @retval true 成功
     * @retval false 失敗
     */
    inline bool makeDirectory(StringRef path) {
        try {
            if (std::filesystem::is_directory(path)) return true;
            return std::filesystem::create_directories(path);
        } catch (...) {
            return false;
        }
    }

    // 2つの path を結合する。後者が絶対パスなら、後者を返す
    inline String joinPath(StringRef path1, StringRef path2) {
        auto p1 = std::filesystem::path(path1);
        auto p2 = std::filesystem::path(path2);
        if (p2.is_absolute()) {
            return p2.wstring();
        } else {
            try {
                return canonicalizePath(p1.append(path2));
            } catch (...) {
                return path1 + std::filesystem::path::preferred_separator + path2;
            }
        }

    }

    // 2つの path を結合する。後者が絶対パスなら、後者を返す
    inline String join_path(StringRef path1, StringRef path2) {
        return joinPath(path1, path2);
    }

    // path のデリミタの正規化
    inline String canonicalizePathDelimiter(StringRef path) {
        return utils::replace_all(path, '/', '\\');
    }

    // カレントディレクトリの取得
    inline String getCurrentDirName() {
        TCHAR dirName[MAX_PATH + 1];
        GetCurrentDirectory(_countof(dirName), dirName);
        return dirName;
    }

    // ファイルが存在するか
    inline bool isFileExistent(StringRef path) noexcept {
        std::error_code ec;
        bool result = std::filesystem::exists(path, ec);
        return !ec && result;
    }

    inline bool exists_path(StringRef path) {
        return isFileExistent(path);
    }

    // ファイルの削除
    inline bool removeFile(StringRef path) {
        std::error_code ec;
        bool result = std::filesystem::remove(path, ec);
        return !ec && result;
    }

    // ファイルが存在すれば削除する
    inline bool removeFileIfExists(StringRef path) {
        if (isFileExistent(path)) return removeFile(path);
        return false;
    }

    // ファイルの move
    inline void moveFile(StringRef src, StringRef dest) {
        if (isFileExistent(src) && !dest.empty()) {
            if (isFileExistent(dest)) removeFile(dest);
            std::error_code ec;
            std::filesystem::rename(src.c_str(), dest.c_str(), ec);
        }
    }

    // ファイルの copy
    inline void copyFile(StringRef src, StringRef dest) {
        CopyFile(src.c_str(), dest.c_str(), FALSE);
    }

    // ファイルのサイズを返す。ファイルが存在しないかエラーの場合は
    inline uintmax_t getFileSize(StringRef path) {
        if (!isFileExistent(path)) return 0;
        std::error_code ec;
        auto result = std::filesystem::file_size(path, ec);
        return ec ? 0 : result;
    }

    using sx = utils::StringUtil;

    // ファイルを back ディレクトリに移動(backファイルのローテートもやる)
    bool moveFileToBackDirWithRotation(StringRef path, int genNum = 3, bool bCopy = false);

    // tmpファイルとcurrentファイルのサイズを比較し、tmpのサイズの方が大きかったら、current を back ディレクトリに移動し、tmp を current にする
    bool compareAndMoveFileToBackDirWithRotation(StringRef pathTmp, StringRef pathCurrent, int genNum = 3, bool bCopy = false);

    // ファイルを back ディレクトリにコピー(backファイルのローテートもやる)
    inline bool copyFileToBackDirWithRotation(StringRef path, int genNum = 3) {
        return moveFileToBackDirWithRotation(path, genNum, true);
    }
} // namespace utils

// Path の拡張メソッドクラス
struct PathUtil {
    static inline bool exists(StringRef path) {
        return utils::isFileExistent(path);
    }

};

