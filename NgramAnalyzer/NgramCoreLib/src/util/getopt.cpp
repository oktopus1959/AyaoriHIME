#include "getopt.h"

#include "string_utils.h"

namespace utils {
//----------------------------------------------------------------------
// コンストラクタ
inline GetOpt::GetOpt( int argc, const char* const* argv,
                       const char *shortopts, const char *longopts )
    : a_argv( argv, argv+argc ) 
{
    if (shortopts != NULL)
        a_shortOpts = shortopts;

    parseLongOpts( longopts );

    parseOpt();
}

//----------------------------------------------------------------------
// 指定されたオプションを得る
inline bool GetOpt::getopt( const char *opt, std::string& value ) const
{
    OptionValueMap::const_iterator pair = a_foundOpts.find( opt );
    if (pair == a_foundOpts.end()) {
        // not found
        return false;
    } else {
        // found
        value = pair->second;
        return true;
    }
}

//----------------------------------------------------------------------
/** longopts を解析する.
 */
inline void GetOpt::parseLongOpts( const char* longopts )
{
    // longopts を分解する
    if (longopts != NULL) {
        std::vector<std::string> lopts = utils::split( longopts, ',' );

        for (size_t i = 0; i < lopts.size(); ++i) {
            std::vector<std::string> opt_val = utils::split( lopts[i], ':' );
            if (opt_val.size() == 1) {
                // 引数をとらないオプションの場合は、"\0" にしておく
                opt_val.push_back( std::string(1, '\0') );
            }
            // opt_val should be a pair of option and value
            a_longOpts[ opt_val[0] ] = opt_val[1];
        }
    }
}

//----------------------------------------------------------------------
/** コマンドライン引数の解析.
 */
inline void GetOpt::parseOpt()
{
    for ( std::vector<std::string>::iterator iter = a_argv.begin();
          iter != a_argv.end();
          ++iter )
    {
        const std::string& args = *iter;

	if ( args[0] != '-' || args == "-" ) {
            // オプションではないか、ただの "-" は実引数とみなす
	    a_args.push_back( args );
	} else if ( args == "--" ) {
            // "--" 以降は、実引数とみなす
            ++iter;
            std::copy( iter, a_argv.end(), std::back_inserter( a_args ) );
            break;
        } else if (args[1] == '-') {
            // "--" で始まる長い名前のオプション、もしくは、引数を取るオプション
            OptionValueMap::iterator pair = a_longOpts.find( args.substr(2) );
            if (pair == a_longOpts.end()) {
                // 仕様外のオプションがあった
                if (a_illegal.empty())
                    a_illegal = args;
            } else {
                // オプションが見つかった
                if (pair->second.size() == 1 && pair->second[0] == '\0') {
                    // 引数をとらないオプション
                    a_foundOpts[ pair->first ] = std::string();
                } else {
                    // 次のパラメタが、オプションの引数になる
                    if ( (iter+1) != a_argv.end() && (*(iter+1))[0] != '-' ) {
                        ++iter;
                        a_foundOpts[ pair->first ] = *iter;
                    } else {
                        a_foundOpts[ pair->first ] = pair->second;
                    }
                }
            }
        } else {
            // "-" で始まる一文字のオプション
            OptionValueMap::iterator pair = a_longOpts.find( args.substr(1,1) );
            if (pair != a_longOpts.end()) {
                // 引数をとる一文字オプション
                if (args.length() > 2) {
                    // "-oFoo" のように、オプションと引数が連接している
                    a_foundOpts[ pair->first ] = args.substr(2);
                } else {
                    // 次のパラメタが、オプションの引数になる
                    if ( (iter+1) != a_argv.end() && (*(iter+1))[0] != '-' ) {
                        ++iter;
                        a_foundOpts[ pair->first ] = *iter;
                    } else {
                        a_foundOpts[ pair->first ] = pair->second;
                    }
                }
            } else {
                // 引数をとらない、純粋な一文字オプション
                // "-abc" のように、引数をとらない一文字オプションは、
                // 連結して記述することができる
                for (size_t j = 1; j < args.length(); ++j) {
                    size_t n = a_shortOpts.find( args[j] );
                    if (n < a_shortOpts.length()) {
                        // single opt found
                        a_foundOpts[ args.substr(j,1) ] = std::string();
                    } else {
                        // illegal opt
                        if (a_illegal.empty())
                            a_illegal = args.substr(j, 1);
                    }
                }
            }
	}
    }
}

} // namespace utils

