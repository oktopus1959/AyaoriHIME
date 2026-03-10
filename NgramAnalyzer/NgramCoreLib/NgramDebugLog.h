#pragma once

#if 1 || defined(_DEBUG)
#undef _LOG_DEBUGH
#undef LOG_DEBUGH
#undef LOG_DEBUG
#define _LOG_DEBUGH LOG_INFOH
#define LOG_DEBUGH LOG_INFOH
#define LOG_DEBUG LOG_INFO
#define _LOG_DEBUGH_FLAG true
#else
#define _LOG_DEBUGH_FLAG false
#endif
