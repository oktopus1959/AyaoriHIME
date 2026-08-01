#pragma once

#include "file_utils.h"
#include "misc_utils.h"
#include "Logger.h"

// -------------------------------------------------------------------
// 検索された履歴候補のクラス
struct SimpleDicResult {
    MString OrigKey;        // 履歴検索の基となったキー (ex.「プログ」)
    MString Key;            // 当履歴候補のキー (ex.「ログ」)
    MString Word;           // 当履歴候補 (ex.「ログファイル」)
    bool WildKey = false;   // ワイルドカードを含むキーか
    size_t KeyLen() const { return Key.size(); }
};

// 検索された履歴候補リストのクラス
class SImpleDicResultList {
    std::vector<SimpleDicResult> dicResults;
    MString origKey;
    bool isWildKey = false;

    SimpleDicResult emptyResult;

private:
    // 履歴リストのサイズが10個以下なら、先頭から10個分の要素と比較する
    bool findSameEntry(const MString& word) {
        if (dicResults.size() < 10) {
            for (size_t i = 0; i < dicResults.size(); ++i) {
                if (dicResults[i].Word == word) return true;
            }
        }
        return false;
    }

public:
    void ClearKeyInfo() {
        dicResults.clear();
        origKey.clear();
        isWildKey = false;
    }

    // ClearKeyInfo() の直後のみ、基キーのセットをする
    void SetKeyInfoIfFirst(const MString& key, bool bWild = false) {
        if (origKey.empty()) {
            origKey = key;
            isWildKey = bWild;
        }
    }

    const MString& GetOrigKey() const {
        return origKey;
    }

    const std::vector<SimpleDicResult>& GetResults() const {
        return dicResults;
    }

    void PushEntry(const MString& key, const MString& word) {
        auto result = SimpleDicResult{ origKey, key, utils::replace(word, MSTR_VERT_BAR_2, MSTR_VERT_BAR), isWildKey };
        if (!findSameEntry(word)) {
            dicResults.push_back(result);
        }
    }

    const MString& GetNthWord(size_t n) const {
        return GetNthResult(n).Word;

    }

    const SimpleDicResult& GetNthResult(size_t n) const {
        return n < dicResults.size() ? dicResults[n] : emptyResult;
    }

    size_t Size() const { return dicResults.size(); }

    bool Empty() const { return Size() == 0; }

    void Append(const SImpleDicResultList& list) {
        utils::append(dicResults, list.dicResults);
    }

    // 最短語を少なくとも先頭から2番目に移動する
    void MoveShortestResultAt2nd() {
        size_t shortestIdx = 0;
        size_t shortestLen = size_t(-1);
        for (size_t i = 0; i < Size(); ++i) {
            if (dicResults[i].Word.size() < shortestLen) {
                shortestIdx = i;
                shortestLen = dicResults[i].Word.size();
            }
        }
        if (shortestIdx > 1) {
            auto elem = dicResults[shortestIdx];
            dicResults.erase(dicResults.begin() + shortestIdx);
            dicResults.insert(dicResults.begin() + 1, elem);
        }
    }

    // 同じ履歴変換キーを探す
    const SimpleDicResult& findSameResultMapKey(const MString& key) {
        for (const auto& hr : dicResults) {
            if (key == hr.Key && hr.Word.size() > key.size() && hr.Word[key.size()] == VERT_BAR) return hr;
        }
        return emptyResult;
    }
};

// -------------------------------------------------------------------
// 履歴入力辞書クラス
class SimpleDictionary{
    DECLARE_CLASS_LOGGER;

public:
    // 仮想デストラクタ
    virtual ~SimpleDictionary() {
        LOG_INFOH(_T("dtor"));
    }

    // 作成された履歴入力辞書インスタンスにアクセスするための Singleton
    static std::unique_ptr<SimpleDictionary> Singleton;

    // 履歴入力辞書インスタンスを生成する
    static int CreateSimpleDictionary(const String&, const String&);

    // ユーザー定義のローマ字辞書を読み込む
    static int ReloadSimpleDictionary();

    // 辞書ファイルへの内容の書き出し
    static void WriteSimpleDictionary();

    // 辞書ファイルへの内容の書き出し
    static void WriteSimpleDictionary(const String&);

    // 辞書ファイルの読み込み(読み込み専用辞書)
    virtual void ReadDicFileAsReadOnlyFirstPreferred(const std::vector<String>& lines) = 0;

    virtual void ReadDicFileAsReadOnlyLastPreferred(const std::vector<String>& lines) = 0;

    // 指定の見出し文字に対する変換候補のセットを取得する
    virtual const SImpleDicResultList& GetCandidates(const MString& key, MString&,
                                                bool allowSingleAsciiMap = false) = 0;

    // 単語の使用
    virtual void UseWord(const MString& word) = 0;

    // 使用辞書の読み込み
    virtual void ReadUsedFile(const std::vector<String>& lines) = 0;

    // 使用辞書内容の保存
    virtual void WriteUsedFile(utils::OfstreamWriter& writer) = 0;

    virtual bool IsUsedDicDirty() const = 0;

};

#define SIMPLE_DIC (SimpleDictionary::Singleton)
