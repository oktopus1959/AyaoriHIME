#pragma once

#include "Logger.h"

#include "FunctionNode.h"

#if 0
#define SIMPLE_DIC_LOG_DEBUGH LOG_INFO
#else
#define SIMPLE_DIC_LOG_DEBUGH(...) {}
#endif

// -------------------------------------------------------------------
// SimpleDicMergerNode - 簡易辞書機能常駐ノード
class SimpleDicMergerNode : public FunctionNode {
    DECLARE_CLASS_LOGGER;
private:
    // 簡易辞書検索により出力された文字列
    MString prevOutString;

    // 上記出力文字列を検索したときのキー文字列
    MString prevKey;

public:
     SimpleDicMergerNode();

     ~SimpleDicMergerNode() override;

    // 当ノードを処理する State インスタンスを作成する
     State* CreateState() override;

    MString getString() const override { return to_mstr(_T("∈")); }

    String getNodeName() const { return _T("StrokeMergerNode"); }

    // 簡易辞書検索により出力された文字列
    inline const MString& GetPrevOutString() const {
        return prevOutString;
    }

    // 簡易辞書検索に使われたキー
    inline const MString& GetPrevKey() const {
        return prevKey;
    }

    inline void SetPrevState(const MString& outStr, const MString& key) {
        SIMPLE_DIC_LOG_DEBUGH(_T("CALLED: outStr={}, key={}"), to_wstr(outStr), to_wstr(key));
        prevOutString = outStr;
        prevKey = key;
    }

    inline void SetPrevKeyState(const MString& key) {
        SIMPLE_DIC_LOG_DEBUGH(_T("CALLED: key={}"), to_wstr(key));
        prevOutString.clear();
        prevKey = key;
    }

    inline void ClearPrevState() {
        SIMPLE_DIC_LOG_DEBUGH(_T("CALLED"));
        prevOutString.clear();
        prevKey.clear();
    }

public:
    void createStrokeTrees(int targetTable = 0);

public:
    // 簡易辞書機能常駐ノードのSingleton
    static std::unique_ptr<SimpleDicMergerNode> Singleton;

    // 簡易辞書機能常駐ノードの生成
    static void CreateSingleton();

    // 簡易辞書機能常駐ノードの初期化
    static void Initialize();

};
#define STROKE_MERGER_NODE (SimpleDicMergerNode::Singleton)
