#include "std_utils.h"
#include "string_utils.h"
using sx = utils::StringUtil;
#include "path_utils.h"
#include "Logger.h"
using Reporting::Logger;
#include "exception.h"
#include "my_utils.h"

#include "constants/Constants.h"
#include "analyzer/CharProperty.h"
#include "analyzer/Connector.h"
#include "analyzer/FeatureIndex.h"
#include "dict/DictionaryInfo.h"
#include "dict/Dictionary.h"
#include "DictionaryBuilder.h"

namespace compiler {
    DEFINE_CLASS_LOGGER(DictionaryBuilder);

    /**
     * make dictionary index
     */
    int DictionaryBuilder::make_dict_index(const Vector<String>& args) {
        auto opts = util::OptHandler::CreateDefaultHandler(L"make_dict_index");
        if (!opts->parseArgs(args)) {
            //println(opts->what);
            std::cout << "To show help, specify -h or --help" << std::endl;
            return EXIT_FAILURE;
        }
        if (opts->help_or_version()) {
            return EXIT_SUCCESS;
        }

        return make_dict_index(opts);
    }

    /**
     * make dictionary index
     */
    int DictionaryBuilder::make_dict_index(OptHandlerPtr opts) {

        auto dicdir = opts->getString(L"dicdir");
        auto userdic = opts->getString(L"userdic");

        LOG_INFOH(L"ENTER: dicdir={}, userdic={}", dicdir, userdic);

        opts->loadDictionaryResource();

        auto dicrc = utils::join_path(dicdir, DICRC);
        int result = opts->loadConfig(dicrc);
        if (result != 0) THROW_RTE(L"no such file or directory: {}", dicrc);

        if (userdic.empty()) {
            compile_sys_dics(opts, dicdir);
        } else {
            compile_user_dics(opts, userdic);
        }

        LOG_INFOH(L"LEAVE: DONE");
        return EXIT_SUCCESS;
    }

    /**
     *  compile system dictionaries
     */
    void DictionaryBuilder::compile_sys_dics(OptHandlerPtr opts, StringRef dicdir) {
        LOG_INFOH(L"ENTER: dicdir={}", dicdir);

        auto outdir = opts->getString(L"outdir");
        auto opt_unknown = opts->getBoolean(L"build-unknown");
        auto opt_matrix = opts->getBoolean(L"build-matrix");
        auto opt_charcategory = opts->getBoolean(L"build-charcategory");
        auto opt_sysdic = opts->getBoolean(L"build-sysdic");
        auto opt_model = opts->getBoolean(L"build-model");
        // auto opt_assign_user_dictionary_costs = opts->getBoolean("assign-user-dictionary-costs")

        // システム辞書のソース(csv)を格納するディレクトリ(dicdir)から csv ファイルを集める
        auto dics = util::enum_csv_dictionaries(dicdir);

        if (!opt_unknown && !opt_matrix && !opt_charcategory && !opt_sysdic && !opt_model) {
            opt_unknown = true;
            opt_matrix = true;
            opt_charcategory = true;
            opt_sysdic = true;
            opt_model = true;
        }

        if (opt_charcategory || opt_unknown) {
            // Char Category
            analyzer::CharProperty::compile(utils::join_path(dicdir, CHAR_PROPERTY_DEF_FILE),
                utils::join_path(dicdir, UNK_DEF_FILE),
                utils::join_path(outdir, CHAR_PROPERTY_FILE));
        }

        if (opt_unknown) {
            // Unknown dic
            Vector<String> unkDefFile = { utils::join_path(dicdir, UNK_DEF_FILE) };
            opts->set(L"type", dict::DictionaryInfo::UNKNOWN_DIC);
            dict::Dictionary::compile(opts, unkDefFile, utils::join_path(outdir, UNK_DIC_FILE));
        }

        if (opt_model) {
            // Model file
            auto modeldeffile = utils::join_path(dicdir, MODEL_DEF_FILE);
            if (utils::exists_path(modeldeffile)) {
                analyzer::FeatureIndex::compile(opts, modeldeffile, utils::join_path(outdir, MODEL_FILE));
            } else {
                std::wcout << modeldeffile << L" is not found. skipped." << std::endl;
            }
        }

        if (opt_sysdic) {
            // System dic
            CHECK_OR_THROW(!dics.empty(), L"no dictionaries are specified");
            opts->set(L"type", dict::DictionaryInfo::SYSTEM_DIC);
            dict::Dictionary::compile(opts, dics, utils::join_path(outdir, SYS_DIC_FILE));
        }

        if (opt_matrix) {
            // Connection Matrix
            analyzer::Connector::compile(
                utils::join_path(dicdir, MATRIX_DEF_FILE),
                utils::join_path(dicdir, MATRIX_EOS_PENALTY_FILE),
                utils::join_path(outdir, MATRIX_FILE));
        }

        LOG_INFOH(L"LEAVE");
    }

    // コマンドライン引数で指定したユーザー辞書ソースファイルをコンパイルする
    // @param opts コマンドライン引数の解析結果
    // @param userdic 作成されるユーザー辞書バイナリーファイル
    Vector<String> DictionaryBuilder::compile_user_dics(OptHandlerPtr opts, StringRef userdic) {
        LOG_INFOH(L"ENTER: userdic={}", userdic);

        // auto userdic = opts->getString("userdic")
        auto restargs = opts->restArgs();
        CHECK_OR_THROW(!restargs.empty(), L"no dictionaries are specified");

        opts->set(L"type", dict::DictionaryInfo::USER_DIC);
        if (opts->isDefined(L"assign-user-dictionary-costs")) {
            //dict::Dictionary::assignUserDictionaryCosts(opts, restargs, userdic);
        } else {
            dict::Dictionary::compile(opts, restargs, userdic);
        }

        LOG_INFOH(L"LEAVE");
        return restargs;
    }

    /**
     * make dictionary index
     */
    int DictionaryBuilder::make_user_dict(OptHandlerPtr opts, const Vector<String>& dic_lines, StringRef outputPath) {

        auto dicdir = opts->getString(L"dicdir");

        LOG_INFOH(L"ENTER: dicdir={}, userdic={}", dicdir, outputPath);

        opts->loadDictionaryResource();

        auto dicrc = utils::join_path(dicdir, DICRC);
        if (opts->loadConfig(dicrc) != 0) {
            LOG_ERROR(L"no such file or directory: {}", dicrc);
            return EXIT_FAILURE;
        }

        opts->set(L"type", dict::DictionaryInfo::USER_DIC);
        dict::Dictionary::compileUserDict(opts, dic_lines, outputPath);

        LOG_INFOH(L"LEAVE: DONE");
        return EXIT_SUCCESS;
    }

}