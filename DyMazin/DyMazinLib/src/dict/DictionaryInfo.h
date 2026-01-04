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
         * This is a unknown word dictionary.
         */
        static const int UNKNOWN_DIC = 3;

        /**
         * filename of dictionary
         * On Windows, filename is stored in UTF-8 encoding
         */
        String filename;

        /**
         * character set of the dictionary. e.g., "SHIFT-JIS", "UTF-8"
         */
        //String charset;

        /**
         * How many words are registered in this dictionary.
         */
        size_t size = 0;

        /**
         * dictionary type
         * this value should be USER_DIC, SYSTEM_DIC, or UNKNOWN_DIC.
         */
        int dicType = 0;

        /**
         * left attributes size
         */
        size_t lsize = 0;

        /**
         * right attributes size
         */
        size_t rsize = 0;

        /**
         * version of this dictionary
         */
        int version = 0;

        /**
         * pointer to the next dictionary info.
         */
        //DictionaryInfo* next;

        DictionaryInfo() {}

        DictionaryInfo(StringRef filename, size_t size, int dicType, size_t lsize, size_t rsize, int version/*, DictionaryInfo* next*/)
            :   filename(filename),
                //info.charset,
                size(size),
                dicType(dicType),
                lsize(lsize),
                rsize(rsize),
                version(version)
                //next(next)
        {}

        DictionaryInfo(const DictionaryInfo& info /*, DictionaryInfo* next*/)
            :   filename(info.filename),
                //info.charset,
                size(info.size),
                dicType(info.dicType),
                lsize(info.lsize),
                rsize(info.rsize),
                version(info.version)
                //next(next)
        {}

    };

} // namespace dict

using DictionaryInfoPtr = SharedPtr<dict::DictionaryInfo>;