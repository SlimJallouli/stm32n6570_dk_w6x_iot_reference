#include "main.h"
#include "logging_levels.h"
/* define LOG_LEVEL here if you want to modify the logging level from the default */
#if defined(LOG_LEVEL)
#undef LOG_LEVEL
#endif

#define LOG_LEVEL    W6X_TRACE_RECORDER_DBG_LEVEL

#include "logging.h"

#define vTracePrintF LogInfo
