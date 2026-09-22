#ifndef __MIMI_CPU_PORT__
#define __MIMI_CPU_PORT__

#include <stdint.h>
#include "mimi.h"

mimi_bool mimi_is_in_isr(void);
void mimi_enable_irq(void);
void mimi_disable_irq(void);

uint32_t mimi_enter_critical(void);
void mimi_exit_critical(uint32_t level);


void mimi_context_switch_to(void *to);
void mimi_context_switch(void *from, void *to);

mimi_err mimi_cpu_init(uint32_t ticksPerSec, void (*handler)(void));
void *mimi_stack_init(void *entry, void *param, void *exit, void *stack, uint32_t size);

#endif  // __MIMI_CPU_PORT__
