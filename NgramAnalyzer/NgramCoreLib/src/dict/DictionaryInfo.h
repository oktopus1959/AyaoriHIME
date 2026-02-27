#pragma once

#include "std_utils.h"
#include "string_utils.h"

namespace dict {
    /**
     * DictionaryInfo structure
     */
    class DictionaryInfo {
    public:
        /**
         * This is a system dictionary.
         */
        static const int SYSTEM_DIC = 1;

        /**
         * This is a user dictionary.
         */
        static const int USER_DIC = 2;

        /**
         * filename of dictionary
         * On Windows, filename is stored in UTF-8 encoding
         */
        String filename;

        /**
         * How many words are registered in this dictionary.
         */
        size_t size = 0;

        /**
         * dictionary type
         * this value should be USER_DIC, SYSTEM_DIC
         */
        int dicType = 0;

        /**
         * version of this dictionary
         */
        int version = 0;

        /**
         * pointer to the next dictionary info.
         */
        //DictionaryInfo* next;

        DictionaryInfo() {}

        DictionaryInfo(StringRef filename, size_t size, int dicType)
            :   filename(filename),
                size(size),
                dicType(dicType)
        {}

        DictionaryInfo(const DictionaryInfo& info)
            :   filename(info.filename),
                size(info.size),
                dicType(info.dicType)
        {}

    };

} // namespace dict

using DictionaryInfoPtr = SharedPtr<dict::DictionaryInfo>;