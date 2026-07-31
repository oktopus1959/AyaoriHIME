#pragma once

#include "ResidentState.h"

// -------------------------------------------------------------------
// 履歴入力(常駐)機能状態(抽象)クラス
class SimpleDicResidentState : public ResidentState {
protected:
    // 履歴常駐状態の事前チェック
    //void DoHistoryResidentPreCheck() override = 0;
    //int HandleDeckeyPreProc(int deckey) override = 0;

public:
    // 文字列を変換して出力
    virtual void SetTranslatedOutString(const MString& outStr, size_t rewritableLen, bool bBushuComp = true, int numBS = -1) = 0;

    virtual void handleFullEscapeResidentState() = 0;

    virtual void handleEisuDecapitalize() = 0;

    virtual void commitHistory() = 0;

    virtual bool IsHistorySelectableByArrowKey() const = 0;

public:
    // 唯一のインスタンスを指すポインタ (寿命管理は CreateState() を呼び出したところがやる)
    static SimpleDicResidentState* Singleton();
    static void SetSingleton(SimpleDicResidentState* pState);
private:
    static std::unique_ptr<SimpleDicResidentState> _singleton;
};

#define SIMPLE_DIC_RESIDENT_STATE (SimpleDicResidentState::Singleton())
