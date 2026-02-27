#pragma once

#include "string_utils.h"

namespace analyzer {
    class TemporaryDict {
        std::set<wchar_t> firstChars;
        std::set<String> entries;
        size_t maxEntryLen = 0;

        void addEntry(const String& entry) {
            if (!entry.empty()) {
                firstChars.insert(entry[0]);
                entries.insert(entry);
                if (entry.length() > maxEntryLen) maxEntryLen = entry.length();
            }
        }

        void clear() {
            firstChars.clear();
            entries.clear();
            maxEntryLen = 0;
        }

    public:
        TemporaryDict() = default;

        void resetEntries(StringRef entries) {
            clear();
            for (const auto& morph : utils::split(entries, L'|')) {
                addEntry(morph);
            }
        }

        bool hasEntry(const String& entry) const {
            return entries.find(entry) != entries.end();
        }

        bool hasFirstChar(wchar_t ch) const {
            return firstChars.find(ch) != firstChars.end();
        }

        size_t getMaxEntryLen() const {
            return maxEntryLen;
        }
    };
}
