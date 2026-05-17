#pragma once

#include "Logger.h"

#if 0 || defined(_DEBUG)
#undef LOG_DEBUGH
#undef LOG_DEBUG
#undef _LOG_DEBUGH
#undef _DEBUG_SENT

#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFO
#define _LOG_DEBUGH LOG_INFOH
#define _DEBUG_SENT(x) x

#define _LOG_DEBUG_ENABLED true
#define _LOG_TEMPH LOG_WARNH
#define _LOG_TEMPW LOG_WARN
#define _LOG_DETAIL if (SETTINGS->multiStreamDetailLog && IS_LOG_INFO_ENABLED) LOG_INFO_QUEUE
#define _LOG_DETAILH if (SETTINGS->multiStreamDetailLog && IS_LOG_INFOH_ENABLED) LOG_INFO_QUEUE
#define _LOG_DETAILW if (SETTINGS->multiStreamDetailLog && IS_LOG_WARN_ENABLED) LOG_INFO_QUEUE
#else
#define _LOG_DEBUG_ENABLED false
#define _LOG_TEMPH LOG_INFOH
#define _LOG_TEMPW LOG_DEBUG
#define _LOG_DETAIL LOG_DEBUG
#define _LOG_DETAILH LOG_DEBUG
#define _LOG_DETAILW LOG_DEBUG
#endif

#define LOG_SAVE_DICT LOG_INFOH
