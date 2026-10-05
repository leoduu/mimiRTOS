#ifndef __MIMI_CONSOLE__
#define __MIMI_CONSOLE__

#include <stdint.h>

typedef uint16_t (*console_input_func)(uint8_t *buffer, uint16_t len);
console_input_func mimi_console_input_func(void);

#endif // __MIMI_CONSOLE__