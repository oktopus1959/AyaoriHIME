#pragma once

#include "OptHandler.h"
#include "Logger.h"

namespace compiler {

    /**
     * Dictionary compiler class
     */
    struct DictionaryBuilder {
        DECLARE_CLASS_LOGGER;

        /**
         * make dictionary index
         */
        static int make_dict_index(const Vector<String>& args);

        /**
         * make dictionary index
         */
        static int make_dict_index(OptHandlerPtr opts);

        /**
         *  compile system dictionaries
         */
        static void compile_sys_dics(OptHandlerPtr opts, StringRef dicdir);

        /**
         *  compile specified user dictionaries
         */
        static Vector<String> compile_user_dics(OptHandlerPtr opts, StringRef userdic);

        /**
         * make user dictionary index
         */
        static int make_user_dict(OptHandlerPtr opts, const Vector<String>& dic_lines, StringRef outputPath);

    };
}  // namespace compiler
