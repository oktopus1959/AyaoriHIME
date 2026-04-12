#include "TemporaryDict.h"

namespace analyzer {

    DEFINE_CLASS_LOGGER(TemporaryDict);

    /**
     * 一時的なユーザー辞書を作成する

     * @param wakatiEntries 形態素解析によって分かち書きされた形態素列 ("|" 区切り)
     */
    void TemporaryDict::resetEntries(StringRef wakatiEntries) {
        clear();
        for (const auto& morph : utils::split(wakatiEntries, L'|')) {
            addEntry(morph);
        }
    }

    void TemporaryDict::addEntry(const String& entry) {
        if (!entry.empty()) {
            firstChars.insert(entry[0]);
            entries.insert(entry);
            if (entry.length() > maxEntryLen) maxEntryLen = entry.length();
        }
    }

    void TemporaryDict::clear() {
        firstChars.clear();
        entries.clear();
        maxEntryLen = 0;
    }

} // namespace analyzer
