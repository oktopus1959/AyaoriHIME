#include "OptHandler.h"
#include "file_utils.h"
#include "regex_utils.h"
#include "misc_utils.h"
#include "string_utils.h"
#include "path_utils.h"
#include "Logger.h"
#include "reporting/ErrorHandler.h"

using sx = utils::StringUtil;

template<class T>
using vx = utils::VectorUtil<T>;

namespace {
    Vector<Vector<String>> defaultOptions = {
        Vector<String>({L"r/rcfile:FILE",                  L"", L"use FILE as resource file"}),
        Vector<String>({L"d/dicdir:DIR",                  L".", L"set DIR as dic dir (default \".\")"}),
        Vector<String>({L"o/outdir:DIR",                  L".", L"set DIR as output dir (default \".\")"}),
        Vector<String>({L"u/userdic:FILE",                 L"", L"use user dictionary"}),
        Vector<String>({L"D/dictionary-info",              L"", L"show dictionary information and exit"}),
        Vector<String>({L"N/nbest:INT",                   L"1", L"output N best results (default 1)"}),
        Vector<String>({L"a/assign-user-dictionary-costs", L"", L"only assign costs/ids to user dictionary"}),
        Vector<String>({L"s/build-sysdic",                 L"", L"build system dictionary"}),
        Vector<String>({L"t/theta:FLOAT",              L"0.75", L"set temparature parameter theta (default 0.75)"}),
        Vector<String>({L"c/cost-factor:INT",           L"700", L"set cost factor (default 700)"}),
        Vector<String>({L"?/non-terminal-cost:INT",        L"10000", L"set non terminal word cost (default 10000)"}),
        Vector<String>({L"f/output:FILE",                  L"", L"set the output file name"}),
        Vector<String>({L"L/log-level:STR",                L"", L"set log level (default none)"}),
        Vector<String>({L"V/verbose:*",                    L"", L"show debug/verbose message."}),
        Vector<String>({L"v/version",                      L"", L"show the version and exit."}),
        Vector<String>({L"h/help",                         L"", L"show this help and exit."})
    };

    class OptsDef {
        DECLARE_CLASS_LOGGER;
    public:
        String shortName;
        String longName;
        String argDesc;
        String value;
        String description;
        bool isSpecified = false;

        OptsDef(StringRef sName, StringRef lName, StringRef aDesc, StringRef sVal, StringRef desc, bool isSpec)
            : shortName(sName), longName(lName), argDesc(aDesc), value(sVal), description(desc), isSpecified(isSpec)
        { }

        OptsDef(const std::vector<String>& opts)
            : OptsDef(
                opts.size() > 0 ? opts[0] : _T(""),
                opts.size() > 1 ? opts[1] : _T(""),
                opts.size() > 2 ? opts[2] : _T(""))
        { }

#define SELECT_NOT_EMPTY(x, y) x.empty() ? y : x

        OptsDef(StringRef keydef, StringRef defval, StringRef desc) {
            if (!keydef.empty()) {
                auto items = utils::reScan(keydef, LR"((([^/: ])(/([^: ]{2,}))?|([^/: ]{2,})(/([^: ]))?)(:([^ ]+)?)?)");
                if (items.size() >= 9) {
                    shortName = SELECT_NOT_EMPTY(items[1], items[6]);
                    longName = SELECT_NOT_EMPTY(items[3], items[4]);
                    argDesc = SELECT_NOT_EMPTY(items[8], items[7].empty() ? _T("") : _T("STR"));  // ':' の後が省略時は STR
                    value = defval;
                    description = desc;
                } else {
                    THROW_RTE(L"invalid option definition: {}", keydef);
                }
            } else {
                shortName = _T("");
                longName = _T("");
                argDesc = _T("");
                value = defval;
                description = desc;
            }
        }

#undef SELECT_NOT_EMPTY
    };
    DEFINE_CLASS_LOGGER(OptsDef);

    using OptsDefPtr = SharedPtr<OptsDef>;
#define MakeOptsDefPtr MakeShared<OptsDef>
}

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
    class OptHandlerImpl : public OptHandler {
        DECLARE_CLASS_LOGGER;

    private:
        String progName;
        std::vector<OptsDefPtr> optValues;
        std::map<String, OptsDefPtr> optMap;
        //std::map<String, String> optMap;

        OptsDefPtr nullOptsDefPtr;

    public:
        OptHandlerImpl() {
            LOG_INFOH(L"CALLED: ctor");
        }

        OptHandlerImpl(StringRef progname, const Vector<Vector<String>>& optArray)
            : progName(progname)
        {
            LOG_INFOH(L"ENTER: ctor: progname={}");

            for (const auto& opt : optArray) {
                optValues.push_back(MakeOptsDefPtr(opt));
            }
            for (const auto& opt : optValues) {
                if (sx::notEmpty(opt->shortName)) {
                    optMap[opt->shortName] = opt;
                }
            }
            for (const auto& opt : optValues) {
                if (sx::notEmpty(opt->longName)) {
                    optMap[opt->longName] = opt;
                }
            }

            LOG_INFOH(L"LEAVE: ctor");
        }

#if 0
        // rcfile や dicrc の読み込み
        void loadDictionaryResource() override {
            LOG_INFOH(L"ENTER");
            auto rcfile = getString(L"rcfile");
            if (rcfile.empty()) {
                LOG_INFOH(L"rcfile not defined");
            } else {
                // load rcfile
                loadConfig(getString(L"rcfile"));
                if (ERROR_HANDLER->HasError()) {
                    LOG_ERROR(L"LEAVE: rcfile not found: {}", rcfile);
                    return;
                }
            }

            String dicdir = getString(L"dicdir");
            if (dicdir.empty()) THROW_RTE(L"dicdir not defined");

            // 
            loadConfig(utils::joinPath(dicdir, DICRC));
            LOG_INFOH(L"LEAVE");
        }
#endif

    public:
        String version() const override {
            return utils::wconcatAny(PACKAGE, L" of ", VERSION);
        }

        String help() const override {
            return makeHelpMessage();
            //return _T("");
        }

        String what() const override {
            return _T("");
        };

        String dump() const override {
            String result;
            for (const auto& [key, defPtr] : optMap) {
                result.append(key).append(L"=").append(defPtr->value).append(L"\n");
            }
            result.append(L"restArgs=").append(utils::join(_restArgs, L" "));
            return result;
        }

    private:
        std::vector<String> _restArgs;

        const OptsDefPtr& _find(StringRef key) const {
            auto iter = optMap.find(key);
            return iter == optMap.end() ? nullOptsDefPtr : iter->second;
        }

    public:
        const std::vector<String>& restArgs() const override {
            return _restArgs;
        }

        const String get(StringRef key, StringRef defVal = L"") const {
            const auto& ptr = _find(key);
            return ptr ? ptr->value : defVal;
        }

        const String getValue(StringRef key) const override {
            return get(key);
        }

        int getInt(StringRef key, int defval = 0) const override {
            return sx::parseInt(getValue(key), defval);
        }

        const String getString(StringRef key, StringRef defVal = L"") const override {
            return get(key, defVal);
        }

        bool getBoolean(StringRef key) const override {
            return getValue(key) == L"true";
        }

        float getFloat(StringRef key, float defval = 0.0) const override {
            return sx::parseFloat(getValue(key), defval);
        }

        double getDouble(StringRef key, double defval = 0.0) const override {
            return sx::parseDouble(getValue(key), defval);
        }

        bool isDefined(StringRef key) const override {
            //return utils::MapUtil::containsKey(optMap, key);
            return sx::notEmpty(getValue(key));
        }

        void set(StringRef key, StringRef value) override {
            LOG_INFOH(L"CALLED: key={}, value={}", key, value);
            auto p = _find(key);
            if (p) {
                if (!p->isSpecified) p->value = value;
            } else {
                OptsDefPtr od = MakeShared<OptsDef>(key, value, L"");
                optValues.push_back(od);
                optMap[key] = od;
            }
        }

        void set(StringRef key, int value) override {
            set(key, sx::toStr(value));
        }

#if 0
        // load configuration file
        int loadConfig(StringRef confFile) override {
            LOG_INFOH(L"ENTER: confFile={}", confFile);
            if (confFile.empty()) {
                return ERROR_HANDLER->Info(L"Config File not specified");
            }

            //val path = make_path(confFile);
            if (!PathUtil::exists(confFile)) {
                //ERROR_NOT_FOUND(confFile);
                LOG_ERROR(L"LEAVE: Config File not found: {}", confFile);
                return ERROR_HANDLER->Error(std::format(L"Config File not found: {}", confFile));
            } else {
                RegexUtil re(LR"!(^\s*([^;#][^\s=]*)\s*=\s*([^\s]*))!");
                for (const auto& line : FileUtil::readAllLines(confFile)) {
                    auto items = re.scan(line);
                    if (items.size() == 2) {
                        LOG_INFO(L"line={}", line);
                        set(items[0], items[1]);
                    }
                }
                LOG_INFOH(L"LEAVE");
                return 0;
            }
        }
#endif

        /**
         * parse argument list
         */
        bool parseArgs(const Vector<String>& args) override {
            if (args.empty()) {
                return true;    // this is not error
            }

            return _parseArgs(args);
        }

        bool parseArgs(size_t argc, const wchar_t** argv) override {
            if (argc == 0) {
                return true;    // this is not error
            }

            Vector<String> args;
            for (size_t i = 0; i < argc; ++i) {
                args.push_back(argv[i]);
            }
            return _parseArgs(args);
        }

        /**
         * help or version
         */
        bool help_or_version() override {
            if (isDefined(L"help")) {
                std::wcout << help() << std::endl;
                return true;
            }
            if (isDefined(L"version")) {
                std::wcout << version() << std::endl;
                return true;
            }
            return false;
        }

        // Initialize
    private:
        String makeHelpMessage() const {
            auto help = sx::catAny(COPYRIGHT, L"\nUsage: ", progName, L" [options] files\n");

            size_t maxPadLen = 0;
            for (const auto& opt : optValues) {
                size_t len =
                    (opt->longName.empty() ? 0 : opt->longName.size() + 2) +
                    (opt->argDesc.empty() ? 0 : opt->argDesc.size() + 1);
                if (len > maxPadLen) maxPadLen = len;
            }

            // help
            for (const auto& opt : optValues) {
                size_t padLen = maxPadLen;
                if (sx::notEmpty(opt->shortName)) {
                    help.append(L" -").append(opt->shortName);
                    help.append(sx::notEmpty(opt->longName) ? L", " : L"  ");
                } else {
                    help.append(L"     ");
                }
                if (sx::notEmpty(opt->longName)) {
                    help.append(L"--").append(opt->longName);
                    padLen -= opt->longName.length() + 2;
                }
                if (sx::notEmpty(opt->argDesc)) {
                    help.append(L"=" + opt->argDesc);
                    padLen -= opt->argDesc.length() + 1;
                }
                help.append(padLen, ' ');
                if (sx::notEmpty(opt->description)) {
                    help.append(L" ").append(opt->description);
                }
                help.append(L"\n");
            }

            help.append(L"\n");

            return help;
        }

#if 0
        /**
         * print configuration
         */
        void printConfig() {
            println("Configurations are:");
            for (auto od : optValues) {
                val value = (if (od.value == null) null else "'%s'".format(od.value));
                if (od.shortName != null && od.longName != null)
                    printf("%s/%s = %s\n", od.shortName, od.longName, value);
                else if (od.longName != null)
                    printf("%s = %s\n", od.longName, value);
                else if (od.shortName != null)
                    printf("%s = %s\n", od.shortName, value);
            }
        }
#endif

        std::wregex reLong = std::wregex(LR"(^--([^=]+)(=(.+))?)");
        std::wregex reShort = std::wregex(LR"(^-(.)(.+)?)");

#define ERROR_UNKNOWN(x)   LOG_ERROR(L"`{}`: unrecognized option", x); result = false
#define ERROR_NO_ARG(x)    LOG_ERROR(L"`{}`: doesn't allow an argument", x); result = false
#define ERROR_REQ_ARG(x)   LOG_ERROR(L"`{}`: requires an argument", x); result = false
//#define ERROR_NOT_FOUND(x) LOG_ERROR(L"`%s`: not found", x.c_str()); result = false

        /**
         * Parse arguments
         * @return error string (null if no error)
         */
        bool _parseArgs(const Vector<String>& args) {
            LOG_INFOH(L"ENTER: {}", utils::join(args, L" | "));
            bool result = true;
            size_t i = 0;
            while (i < args.size()) {
                const auto& arg = args[i++];
                // long option
                auto items = RegexUtil::reScan(arg, reLong);
                if (items.size() == 3) {
                    //case reLong(o,_,v) = > 
                    // println("long: o=" + o + ", v=" + v)
                    const String& o = items[0];
                    const String& v = items[2];
                    LOG_INFO(L"long: o={}, v={}", o, v);
                    auto optdef = utils::safe_get(optMap, o);
                    if (optdef) {
                        if (sx::notEmpty(optdef->argDesc)) {
                            if (sx::notEmpty(v)) {
                                optdef->value = v;
                                optdef->isSpecified = true;
                            } else if (optdef->argDesc == L"*") {
                                optdef->value = L"";
                                optdef->isSpecified = true;
                            } else if (i < args.size()) {
                                optdef->value = args[i];
                                optdef->isSpecified = true;
                                ++i;
                            } else {
                                ERROR_REQ_ARG(arg);
                            }
                        } else {
                            if (sx::notEmpty(v)) {
                                ERROR_NO_ARG(arg);
                            } else {
                                optdef->value = L"true";
                                optdef->isSpecified = true;
                            }
                        }
                    } else {
                        ERROR_UNKNOWN(arg);
                    }
                } else {
                    // short option
                    items = RegexUtil::reScan(arg, reShort);
                    if (items.size() == 2) {
                        const String& o = items[0];
                        const String& r = items[1];
                        LOG_INFO(L"short: o={}, rest={}", o, r);
                        auto optdef = utils::safe_get(optMap, o);
                        if (optdef) {
                            if (sx::notEmpty(optdef->argDesc)) {
                                if (sx::notEmpty(r)) {
                                    optdef->value = r;
                                    optdef->isSpecified = true;
                                } else if (optdef->argDesc == L"*") {
                                    optdef->value = L"";
                                    optdef->isSpecified = true;
                                } else if (i < args.size()) {
                                    optdef->value = args[i];
                                    optdef->isSpecified = true;
                                    ++i;
                                } else {
                                    ERROR_REQ_ARG(arg);
                                }
                            } else {
                                if (sx::notEmpty(r)) {
                                    // combined short opts
                                    for (auto x : r) {
                                        auto so = std::format(L"-{} (in {})", x, arg);
                                        auto od = utils::safe_get(optMap, String(1, x));
                                        if (!od) {
                                            ERROR_UNKNOWN(so);
                                        } else if (sx::notEmpty(od->argDesc)) {
                                            ERROR_REQ_ARG(so);
                                        } else {
                                            od->value = L"true";
                                            od->isSpecified = true;
                                        }
                                    }
                                } else {
                                    optdef->value = L"true";
                                    optdef->isSpecified = true;
                                }
                            }
                        } else {
                            ERROR_UNKNOWN(arg);
                        }
                    } else {
                        _restArgs.push_back(arg);
                    }
                }
            }

            LOG_INFOH(L"LEAVE: result={}", result);
            return result;
        }
    };
    DEFINE_CLASS_LOGGER(OptHandlerImpl);

    DEFINE_CLASS_LOGGER(OptHandler);

    SharedPtr<OptHandler> OptHandler::CreateOptHandler(StringRef progname, const Vector<Vector<String>>& optArray) {
        LOG_INFOH(L"CALLED: progname={}", progname);
        return MakeShared<OptHandlerImpl>(progname, optArray);
    }

    SharedPtr<OptHandler> OptHandler::CreateDefaultHandler(StringRef progname) {
        LOG_INFOH(L"CALLED: progname={}", progname);
        return CreateOptHandler(progname.empty() ? L"default" : progname, defaultOptions);
    }

    void setLogLevel(StringRef logLevel) {
        if (logLevel == L"error") Reporting::Logger::SetLogLevel(Reporting::Logger::LogLevelError);
        else if (logLevel == L"warnh") Reporting::Logger::SetLogLevel(Reporting::Logger::LogLevelWarnH);
        else if (logLevel == L"warn") Reporting::Logger::SetLogLevel(Reporting::Logger::LogLevelWarn);
        else if (logLevel == L"infoh") Reporting::Logger::SetLogLevel(Reporting::Logger::LogLevelInfoH);
        else if (logLevel == L"info") Reporting::Logger::SetLogLevel(Reporting::Logger::LogLevelInfo);
        else if (logLevel == L"debugh") Reporting::Logger::SetLogLevel(Reporting::Logger::LogLevelDebugH);
        else if (logLevel == L"debug") Reporting::Logger::SetLogLevel(Reporting::Logger::LogLevelDebug);
    }

    SharedPtr<OptHandler> OptHandler::CreateOptHandler(size_t argc, const wchar_t** argv, const wchar_t* logFilePath) {
        String progname;
        Vector<String> args;
        if (argc > 0) {
            progname = *argv++;
            --argc;
        }

        for (size_t i = 0; i < argc; ++i) {
            args.push_back(argv[i]);
            if (args.size() >= 2 && (args[args.size()-2] == L"-L" || args[args.size()-2] == L"--log-level")) {
                setLogLevel(args.back());
            }
        }

        if (Reporting::Logger::LogFilename().empty() && logFilePath) {
            //Reporting::Logger::LogFilename = _T("NgramLib.log");
            Reporting::Logger::SetLogFilename(logFilePath);
            LOG_INFOH(L"\n==== START NgramLib: progname={}, args={} ====\n", progname, utils::join(args, L" | "));
        }

        auto opts = util::OptHandler::CreateDefaultHandler(progname);
        if (!opts->parseArgs(args)) {
            //println(opts->what);
            std::cerr << "To show help, specify -h or --help" << std::endl;
            return 0;
        }
        if (opts->help_or_version()) {
            return 0;
        }

        return opts;
    }

} // namespace util

