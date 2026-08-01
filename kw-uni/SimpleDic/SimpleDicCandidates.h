#pragma once

//#include "Settings.h"
//#include "State.h"

#include "SimpleDic/SimpleDictionary.h"

// -------------------------------------------------------------------
// 簡易辞書検索候補リストのクラス
class SimpleDicCandidates {
public:
    // デストラクタ
    virtual ~SimpleDicCandidates() { }

    // 簡易辞書検索キー設定をクリアする
    virtual void ClearKeyInfo() = 0;

    // 指定のキーで始まる候補を取得する
    virtual const std::vector<SimpleDicResult>& GetCandidates(const MString& key,
                                                         bool allowSingleAsciiMap = false) = 0;

    virtual const std::vector<MString> GetCandWords(const MString& key,
                                                    bool allowSingleAsciiMap = false) = 0;

    // 取得済みの候補列を返す
    //virtual const std::vector<SimpleDicResult>& GetCandidates() const = 0;

    virtual const std::vector<MString> GetCandWords() const = 0;

    virtual const MString& GetCurrentKey() const = 0;

    // 次の候補を選択する
    virtual const SimpleDicResult GetNext() const = 0;

    // 前の候補を選択する
    virtual const SimpleDicResult GetPrev() const = 0;

    // 選択された単語を取得する
    virtual const MString& GetSelectedWord() const = 0;

    // 選択されている位置を返す -- 未選択状態なら -1を返す
    virtual int GetSelectPos() const = 0;

    // 選択位置を初期化(未選択状態)する
    virtual const SimpleDicResult ClearSelectPos() = 0;

    // 候補が選択されていれば、それを使用順リストの先頭にpushする -- selectPos は未選択状態に戻る
    virtual void DelayedPushFrontSelectedWord() = 0;

public:
    static std::unique_ptr<SimpleDicCandidates> Singleton;

    static void CreateSingleton();
};

#define SIMPLE_DIC_CAND (SimpleDicCandidates::Singleton)
