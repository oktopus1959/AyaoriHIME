#pragma once

#include "std_utils.h"
#include "string_utils.h"
#include "misc_utils.h"
#include "exception.h"
#include "constants/Constants.h"
#include "Logger.h"

namespace util {
    /**
     * command line option parser
     *
     * each opts' element is a array of: NAME_SPEC[:ARG_TYPE], DEFAULT_VALUE, DESCRIPTION
     * NAME_SPEC is: SHORT_NAME[/LONG_NAME] or LONG_NAME[/SHORT_NAME]
     * examples: "a/abc:INT", "foo", "a or abc option"
     *           "a", null, "only short name option is available"
     *           "xyz:STR", "default", "long name and type specified"
     *           "foo:*", null, "long name and some arg may be supplied"
     */
    class OptHandler {
        DECLARE_CLASS_LOGGER;

    public:
        static SharedPtr<OptHandler> CreateOptHandler(StringRef progname, const Vector<Vector<String>>& optArray);
        static SharedPtr<OptHandler> CreateDefaultHandler(StringRef progname = L"");
        static SharedPtr<OptHandler> CreateOptHandler(size_t argc, const wchar_t** argv, const wchar_t* logFilePath);

        // rcfile や dicrc などの設定ファイルを読み込む
        virtual void loadDictionaryResource() = 0;

        virtual String version() const = 0;
        virtual String help() const = 0;

        virtual String what() const = 0;

        virtual String dump() const = 0;

    public:
        virtual const std::vector<String>& restArgs() const = 0;

        virtual int getInt(StringRef key, int defval = 0) const = 0;

        virtual const String getValue(StringRef key) const = 0;

        virtual const String getString(StringRef key, StringRef defVal = L"") const = 0;

        virtual bool getBoolean(StringRef key) const = 0;

        virtual float getFloat(StringRef key, float defval = 0.0) const = 0;

        virtual double getDouble(StringRef key, double defval = 0.0) const = 0;

        virtual bool isDefined(StringRef key) const = 0;

        virtual void set(StringRef key, StringRef value) = 0;

        virtual void set(StringRef key, int value) = 0;

        // load configuration file
        virtual int loadConfig(StringRef rcfile) = 0;

        virtual bool parseArgs(const Vector<String>& args) = 0;

        // argv may not contain program name
        virtual bool parseArgs(size_t argc, const wchar_t** argv) = 0;

        virtual bool help_or_version() = 0;
    };

} // namespace util

using OptHandlerRef = const util::OptHandler&;
using OptHandlerPtr = std::shared_ptr<util::OptHandler>;
