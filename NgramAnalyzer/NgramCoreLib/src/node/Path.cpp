#include "string_utils.h"
#include "Path.h"
#include "Node.h"

namespace node
{
    DEFINE_CLASS_LOGGER(Path);

    Path::Path(int id) : _rnode(nullptr), _rnext(nullptr), _lnode(nullptr), _lnext(nullptr), _id(id) {
        LOG_DEBUGH(L"CALLED: ctor");
    }

    Path::~Path() {
        LOG_DEBUGH(L"CALLED: dtor");
    }

    String Path::debugString() const {
        const Node* rn = rnode();
        const Node* ln = lnode();
        return std::format(L"{}:<{}>-[{}]-<{}>:{}|{}", _id, ln ? ln->toVerbose() : L"null", cost(), rn ? rn->toVerbose() : L"null", _lnext ? _lnext->_id : 0, _rnext ? _rnext->_id : 0);
    }

    int Path::_serial = 0;

    void Path::Reset() {
        _serial = 0;
    }

    // Path ファクトリ
    SharedPtr<Path> Path::Create() {
        return MakeShared<Path>(++_serial);
    }

} // namespace node
