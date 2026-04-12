#pragma once

#include "string_utils.h"
#include "Logger.h"

namespace analyzer {
    class TemporaryDict {
        DECLARE_CLASS_LOGGER;

        std::set<wchar_t> firstChars;
        std::set<String> entries;
        size_t maxEntryLen = 0;

        void addEntry(const String& entry);

        void clear();

    public:
        TemporaryDict() = default;

        // 一時的なユーザー辞書を作成する
        void resetEntries(StringRef entries);

        inline bool hasEntry(const String& entry) const {
            return entries.find(entry) != entries.end();
        }

        inline bool hasFirstChar(wchar_t ch) const {
            return firstChars.find(ch) != firstChars.end();
        }

        inline size_t getMaxEntryLen() const {
            return maxEntryLen;
        }
    };
}
