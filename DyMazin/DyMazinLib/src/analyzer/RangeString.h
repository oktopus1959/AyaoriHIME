#pragma once

#include "string_utils.h"

namespace analyzer
{
    /**
     * 基底文字列に格納された部分文字列を扱うクラス
     */
    class RangeString {
    private:
        String _baseStr;    // 基底文字列
        size_t _begin = 0;   // 部分文字列の先頭位置
        size_t _end = 0;     // 部分文字列の末尾(の次の)位置

        size_t _length = _end - _begin;

        inline size_t _baseLength() const { return _baseStr.size() - 1; } // 末尾の番兵('\0')の分を除く

    public:
        RangeString() : _begin(0), _end(0) {
            _baseStr.push_back('\0');
        }

        /**
         * s を基底文字列とする RangeString を返す。 str には番兵文字('\0')が append される。
         */
        RangeString(StringRef s) : _baseStr(s), _begin(0), _end(s.size()) {
            _baseStr.push_back('\0');
        }

        RangeString(StringRef s, size_t b, size_t e) : _baseStr(s), _begin(b), _end(e) {
            assert(b <= e);
        }

        size_t begin() const { return _begin; }

        size_t end() const { return _end; }

        size_t length() const { return _length; }

        size_t baseLength() const { return _baseLength(); }

        wchar_t charAt(size_t pos) const {
            assert(pos < _baseStr.size());
            return _baseStr[pos];
        }

        void appendTo(String& buf) {
            buf.append(_baseStr, _begin, _length);
        }

        void appendTo(String& buf, size_t from, size_t to) {
            buf.append(_baseStr, from, std::min(to, _baseLength()) - from);
        }

        void appendTo(String& buf, size_t from) {
            appendTo(buf, from, _end);
        }

        std::shared_ptr<RangeString> subString(size_t from, size_t to) const {
            return std::make_shared<RangeString>(_baseStr, from, std::min(to, _baseLength()));
        }

        std::shared_ptr<RangeString> subString(size_t from) const {
            return subString(from, _end);
        }

        String caseForm() const {
            return std::format(L"RangeString({}, {}, {})", _baseStr, _begin, _end);
        }

        String toString() const {
            return _baseStr.substr(_begin, _length);
        }

        String toString(size_t from, size_t to) const {
            return _baseStr.substr(from, std::min(to, _baseLength()) - from);
        }

        String toString(size_t from) const {
            return toString(from, _end);
        }

        bool hasSameBase(const RangeString& rhs) const {
            return _baseStr == rhs._baseStr;
        }

        bool equals(const RangeString& rhs) const {
            //RangeString* p = dynamic_cast<RangeString*>(rhs);
            //if (p) {
            //    return baseStr == p->baseStr && _begin == p->_begin && _end == p->_end;
            //}
            //return false;
            return _baseStr == rhs._baseStr && _begin == rhs._begin && _end == rhs._end;
        }

        bool operator==(const RangeString& rhs) const {
            return equals(rhs);
        }

        int hashCode() const {
            return
                41 * (
                    41 * (
                        41 + (int)_end
                        ) + (int)_begin
                    ) + (int)std::hash<String>()(_baseStr);
        }

        /**
         * str を基底文字列とする RangeString を返す。 str には番兵文字('\0')が append される。
         */
        static std::shared_ptr<RangeString> Create(StringRef str) {
            return std::make_shared<RangeString>(str);
        }
    };

} // namespace analyzer

#define RangeStringPtr std::shared_ptr<analyzer::RangeString>
