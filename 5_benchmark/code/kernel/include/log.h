#ifndef __MIMI_LOG__
#define __MIMI_LOG__

#include <stdint.h>
#include "mimi.h"
#include "conf.h"

/* -------------------------------------------------------------------------- */
/*  log                                                                       */
/* -------------------------------------------------------------------------- */
void mimi_log_printf(char level, const char *fmt, ...);
void mimi_log_kprintf(const char *fmt, ...);
void mimi_log_print_raw(const char *fmt, ...);
void mimi_log_init(void);

typedef void (*log_output_func)(const uint8_t *buffer, uint16_t len);
log_output_func mimi_log_output_func(void);

#define MIMI_LOG(fmt, ...)    mimi_log_print_raw(fmt, ##__VA_ARGS__)

#if (MIMI_LOG_LEVEL >= MIMI_LOG_LEVEL_VERBOSE)
#define MIMI_LOG_V(fmt, ...)    mimi_log_printf('V', fmt, ##__VA_ARGS__)
#else
#define MIMI_LOG_V(...)
#endif

#if (MIMI_LOG_LEVEL >= MIMI_LOG_LEVEL_DEBUG)
#define MIMI_LOG_D(fmt, ...)    mimi_log_printf('D', fmt, ##__VA_ARGS__)
#else
#define MIMI_LOG_D(...)
#endif

#if (MIMI_LOG_LEVEL >= MIMI_LOG_LEVEL_INFO)
#define MIMI_LOG_I(fmt, ...)    mimi_log_printf('I', fmt, ##__VA_ARGS__)
#else
#define MIMI_LOG_I(...)
#endif

#if (MIMI_LOG_LEVEL >= MIMI_LOG_LEVEL_WARNING)
#define MIMI_LOG_W(fmt, ...)    mimi_log_printf('W', fmt, ##__VA_ARGS__)
#else
#define MIMI_LOG_W(...)
#endif

#if (MIMI_LOG_LEVEL >= MIMI_LOG_LEVEL_ERROR)
#define MIMI_LOG_E(fmt, ...)    mimi_log_printf('E', fmt, ##__VA_ARGS__)
#else
#define MIMI_LOG_E(...)
#endif

#endif // __MIMI_LOG__
