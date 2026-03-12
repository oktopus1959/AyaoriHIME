#pragma once

#if 1 || defined(_DEBUG)
#undef LOG_DEBUGH
#undef LOG_DEBUG
#undef _LOG_DEBUGH
#undef _DEBUG_SENT
#undef LOG_SAVE_DICT

#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFO
#define _LOG_DEBUGH LOG_INFOH
#define _DEBUG_SENT(x) x
#define LOG_SAVE_DICT LOG_INFOH
#define _LOG_DEBUG_ENABLED true
#else
#define _LOG_DEBUG_ENABLED false
#endif
