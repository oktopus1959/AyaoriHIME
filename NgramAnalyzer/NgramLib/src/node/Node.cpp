#include "Node.h"
#include "Logger.h"

#if 1 || defined(_DEBUG)
#define _LOG_DEBUGH_FLAG true
#undef LOG_DEBUGH
#define LOG_DEBUGH LOG_INFOH
#else
#define _LOG_DEBUGH_FLAG false
#endif

namespace {

    int headHiraganaLen(StringRef str) {
        int len = 0;
        for (const auto& ch : str) {
            if (utils::is_hiragana(ch)) {
                ++len;
            } else {
                break;
            }
        }
        return len;

    }

    int tailHiraganaLen(StringRef str) {
        int len = 0;
        for (auto it = str.rbegin(); it != str.rend(); ++it) {
            if (utils::is_hiragana(*it)) {
                ++len;
            } else {
                break;
            }
        }
        return len;
    }
}

namespace node
{
    DEFINE_CLASS_LOGGER(Node);

    WordType _determineWordType(const RangeStringPtr& str) {
        bool hasHiragana = false;
        bool hasKanji = false;
        bool hasGeta = false;
        size_t _begin = str->begin();
        size_t _end = str->end();
        for (size_t i = _begin; i < _end; ++i) {
            wchar_t ch = str->charAt(i);
            if (ch == GETA_CHAR) {
                hasGeta = true;
            } else if (utils::is_hiragana(ch)) {
                hasHiragana = true;
            } else if (utils::is_kanji(ch)) {
                hasKanji = true;
            }
        }
        if (hasGeta) {
            return hasHiragana || hasKanji ? WordType::MIXED : WordType::GETA;
        } else if (hasHiragana && !hasKanji) {
            return WordType::HIRAGANA;
        } else if (!hasHiragana && hasKanji) {
            return WordType::KANJI;
        } else if (hasHiragana && hasKanji) {
            return WordType::MIXED;
        } else {
            return WordType::UNKNOWN;
        }
    }

    inline WordType determineWordType(RangeStringPtr str, node::NodeType nt) {
        if (nt == node::NodeType::BOS_NODE) return WordType::BOS;
        if (nt == node::NodeType::EOS_NODE) return WordType::EOS;
        if (nt == node::NodeType::NORMAL_NODE) return _determineWordType(str);
        return WordType::UNKNOWN;
    }

    Node::Node(RangeStringPtr sf, node::NodeType nt)
        : _surface(sf),
        _word_type(determineWordType(sf, nt)),
        _stat(nt),
        _next(nullptr),
        _prev(nullptr),
        _rpath(nullptr),
        _lpath(nullptr) {
        StringRef s = _surface->toString();
        LOG_DEBUGH(L"CALLED: ctor: _surface={}, word_type={}", s, getWordTypeStr(_word_type));
        if (!s.empty()) {
            _is_head_hiragana = utils::is_hiragana(s.front());
            _is_tail_hiragana = utils::is_hiragana(s.back());
            _is_short_hiragana = _word_type == WordType::HIRAGANA && s.length() <= 2;
        }
    }

    Node::~Node() {
        LOG_DEBUGH(L"CALLED: dtor: {:p}", (void*)this);
    }

    String Node::toVerbose() const {
        return _surface->toString()
            + _T("[wcost=") + std::to_wstring(wcost())
            + _T(", accum=") + std::to_wstring((int)_accumCost)
            + _T(", accum2=") + std::to_wstring((int)_accumCost2)
            + _T(", wtype=") + getWordTypeStr(_word_type)
            + _T("]");
    }

    // ノードファクトリ
    SharedPtr<Node> Node::Create(RangeStringPtr str, node::NodeType nt) {
        return MakeShared<Node>(str, nt);
    }

} // namespace node
