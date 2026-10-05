#ifndef __MIMI_BOARD__
#define __MIMI_BOARD__

#include "log.h"
#include "conf.h"

void mimi_board_init(void);

log_output_func mimi_board_get_log_output_func(void);

#if MIMI_CONSOLE
console_input_func mimi_board_get_console_input_func(void);
#endif

#endif // __MIMI_BOARD__
