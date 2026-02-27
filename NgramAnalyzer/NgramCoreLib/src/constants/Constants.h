#pragma once

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define COPYRIGHT       L"NgramAnalyzer\n"

#define VERSION         L"0.1"          // should be defined in .ini
#define PACKAGE         L"ngram"        // should be defined in .ini
#define DIC_VERSION     0               // should be defined in .ini

#define SYS_DIC_FILE            L"ngram-sys.dic"
#define BOS_KEY                 L"BOS/EOS"

#define NBEST_MAX 512

//#define UNK_WORD_ADDITIONAL_COST 10000  // 未知語長制約による追加コスト
#define UNK_INITIAL_COST        3000    // 未知語に追加する初期コスト
#define ONE_CHAR_COST_FACTOR    5000    // 未知語に与える、2文字目以降、文字数に比例したコストの1文字あたりのファクタ
#define UNK_CONNECT_COST        5000    // 未知語同士の連結の場合の追加コスト

#define NON_TERMINAL_POS    -1          // 非終端の品詞

#define MAX_COST            99999999

#define GETA_CHAR    L'〓'        // ゲタ文字
