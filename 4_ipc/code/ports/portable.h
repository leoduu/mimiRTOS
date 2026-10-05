#ifndef _MIMI_PORT_H_
#define _MIMI_PORT_H_

#include <stdint.h>
#include "mimi.h"

mimi_inline mimi_bool mimi_is_in_isr(void);
mimi_inline void mimi_enable_irq(void);
mimi_inline void mimi_disable_irq(void);
mimi_inline uint32_t mimi_enter_critical(void);
mimi_inline void mimi_exit_critical(uint32_t level);

void mimi_context_first_switch(void);
void mimi_context_switch(void);

mimi_err mimi_cpu_init(uint32_t ticksPerSec, void (*handler)(void));
void *mimi_stack_init(void *entry, void *param, void *exit, void *stack, uint32_t size);

#endif  // _MIMI_PORT_H_
