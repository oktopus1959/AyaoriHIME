#pragma once

#include "string_utils.h"

namespace analyzer {
    class TemporaryDict {
        std::set<wchar_t> firstChars;
        std::set<String> entries;

        void addEntry(const String& entry) {
            if (!entry.empty()) {
                firstChars.insert(entry[0]);
                entries.insert(entry);
            }
        }

        void clear() {
            firstChars.clear();
            entries.clear();
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
    };
}
