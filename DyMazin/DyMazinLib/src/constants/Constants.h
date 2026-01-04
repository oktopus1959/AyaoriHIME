#pragma once

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define COPYRIGHT       L"DyMazin: Morphological Analyzer\n"

#define VERSION         L"0.1"       // should be defined in .ini
#define PACKAGE         L"dymazin"   // should be defined in .ini
#define DIC_VERSION     102             // should be defined in .ini

#define DEFAULT_CONF            L"etc/dymazinrc"
#define SYS_DIC_FILE            L"sys.dic"
#define UNK_DEF_FILE            L"unk.def"
#define UNK_DIC_FILE            L"unk.dic"
#define MATRIX_DEF_FILE         L"matrix.def"
#define MATRIX_EOS_PENALTY_FILE L"matrix-eos-penalty.def"
#define MATRIX_FILE             L"matrix.bin"
#define CHAR_PROPERTY_DEF_FILE  L"char.def"
#define CHAR_PROPERTY_FILE      L"char.bin"
#define FEATURE_FILE            L"feature.def"
#define REWRITE_FILE            L"rewrite.def"
#define LEFT_ID_FILE            L"left-id.def"
#define RIGHT_ID_FILE           L"right-id.def"
#define POS_ID_FILE             L"pos-id.def"
#define MODEL_DEF_FILE          L"model.def"
#define MODEL_FILE              L"model.bin"
#define DICRC                   L"dicrc"
#define BOS_KEY                 L"BOS/EOS"

#define DEFAULT_MAX_GROUPING_SIZE 24

#define CHAR_PROPERTY_DEF_DEFAULT L"DEFAULT 1 0 0\nSPACE   0 1 0\n0x0020 SPACE\n"
#define UNK_DEF_DEFAULT           L"DEFAULT,0,0,0,*\nSPACE,0,0,0,*\n"
#define MATRIX_DEF_DEFAULT        L"1 1\n0 0 0\n"

#define NBEST_MAX 512

//#define UNK_WORD_ADDITIONAL_COST 10000  // 未知語長制約による追加コスト
#define UNK_INITIAL_COST        3000    // 未知語に追加する初期コスト
#define ONE_CHAR_COST_FACTOR    5000    // 未知語に与える、2文字目以降、文字数に比例したコストの1文字あたりのファクタ
#define UNK_CONNECT_COST        5000    // 未知語同士の連結の場合の追加コスト

#define NON_TERMINAL_POS    -1          // 非終端の品詞

#define MAX_COST            99999999
