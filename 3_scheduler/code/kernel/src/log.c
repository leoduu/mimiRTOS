
#include "log.h"
#include <stdarg.h>
#include <stdio.h>

void mimi_log_printf(char level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[%c] ", level);
    vprintf(fmt, args);
    va_end(args);
}
