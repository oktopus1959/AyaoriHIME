#pragma once

#include "Logger.h"

#include "FunctionNode.h"
//#include "HistoryResidentState.h"

#if 0
#define HIST_LOG_DEBUGH LOG_INFO
#else
#define HIST_LOG_DEBUGH(...) {}
#endif

// -------------------------------------------------------------------
// HistoryNode - 履歴入力機能ノード
class HistoryNode : public FunctionNode {
    DECLARE_CLASS_LOGGER;
 public:
     HistoryNode();

     ~HistoryNode() override;

    // 当ノードを処理する State インスタンスを作成する
     State* CreateState();

    MString getString() const override { return to_mstr(_T("◆")); }

    String getNodeName() const { return _T("HistoryNode"); }

public:
    static HistoryNode* Singleton(bool bRenew = false);
private:
    static HistoryNode* _singleton;
};
#define HISTORY_NODE (HistoryNode::Singleton())

// -------------------------------------------------------------------
// HistoryNodeBuilder - 履歴入力機能ノードビルダ
#include "FunctionNodeBuilder.h"

class HistoryNodeBuilder : public FunctionNodeBuilder {
    DECLARE_CLASS_LOGGER;
public:
    Node* CreateNode() override;
};

