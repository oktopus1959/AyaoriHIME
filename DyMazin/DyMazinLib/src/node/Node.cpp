#include "Node.h"

namespace node
{
    DEFINE_CLASS_LOGGER(Node);

    Node::Node(RangeStringPtr sf) : _surface(sf), _next(nullptr), _prev(nullptr), _rpath(nullptr), _lpath(nullptr) {
        LOG_DEBUGH(L"CALLED: ctor: {:p}", (void*)this);
    }

    Node::~Node() {
        LOG_DEBUGH(L"CALLED: dtor: {:p}", (void*)this);
    }

    String Node::toVerbose() const {
        return _surface->toString() + _T("[") + std::to_wstring(wcost()) + _T("]: ") + feature() + _T(" [") + getNodeTypeStr(_stat) + _T("]");
    }

    // ノードファクトリ
    SharedPtr<Node> Node::Create(RangeStringPtr str) {
        return MakeShared<Node>(str);
    }

} // namespace node
