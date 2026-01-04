#include "LearnerPath.h"
#include "LearnerNode.h"

namespace node
{
    DEFINE_CLASS_LOGGER(LearnerPath);

    LearnerPath::LearnerPath() {
        LOG_INFOH(L"CALLED: ctor");
    }

    LearnerPath::~LearnerPath() {
        LOG_INFOH(L"CALLED: dtor");
    }

    bool LearnerPath::isEmpty() const {
        return (!rnode->rpath && rnode->stat() != NodeType::EOS_NODE)
            || (!lnode->lpath && lnode->stat() != NodeType::BOS_NODE);
    }

    SharedPtr<LearnerPath> LearnerPath::Create() {
        return MakeShared<LearnerPath>();
    }

    ///**
    // * LearnerPath ファクトリトレイト
    // */
    //trait LearnerPathFactory extends PathFactoryT[LearnerPath]

    ///**
    // * LearnerPath ファクトリオブジェクト
    // */
    //object LearnerPathFactory extends LearnerPathFactory{
    //  override def newPath() = new LearnerPath()
    //}

} // namespace node