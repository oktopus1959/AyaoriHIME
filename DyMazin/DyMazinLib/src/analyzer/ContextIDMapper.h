#pragma once

#include "std_utils.h"
#include "string_utils.h"
#include "Logger.h"

namespace analyzer {

    using ContextMap = Map<String, int>;
    using RevContextMap = Map<int, String>;

    class ContextIDMapper {
        DECLARE_CLASS_LOGGER;
    private:
        String left_bos;    // 左文脈BOS
        String right_bos;   // 右文脈BOS

        ContextMap left;
        ContextMap right;

    public:
        static bool open_map(StringRef filename, ContextMap& cmap);
        static void addContext(ContextMap& cmap, StringRef key);
        static bool build(ContextMap& cmap, StringRef bos);
        static bool save(StringRef filename, const ContextMap& cmap);

        static RevContextMap reverseMap(const ContextMap& cmap);
        static short findId(const ContextMap& cmap, StringRef s, const wchar_t* errMsg);

    public:
        void clear() {
            left.clear();
            right.clear();
        }

        void addContexts(StringRef l, StringRef r) {
            addContext(left, l);
            addContext(right, r);
        }

        bool save(StringRef lfile, StringRef rfile) {
            return save(lfile, left) && save(rfile, right);
        }

        bool build() {
            return build(left, left_bos) && build(right, right_bos);
        }

        bool open(StringRef lfile, StringRef rfile) {
            return open_map(lfile, left) && open_map(rfile, right);
        }

        short lid(StringRef l) {
            return findId(left, l, L"cannot find LEFT-ID for ");
        }

        short rid(StringRef r) {
            return findId(right, r, L"cannot find RIGHT-ID for ");
        }

        size_t left_size() {
            return left.size();
        }

        size_t right_size() {
            return right.size();
        }

        RevContextMap left_ids() {
            return reverseMap(left);
        }

        RevContextMap right_ids() {
            return reverseMap(right);
        }

        bool is_valid(int lid, int rid) {
            return lid >= 0 && (size_t)lid < left_size() && rid >= 0 && (size_t)rid < right_size();
        }
    };

} // namespace analyzer