#ifndef __MIMI_CPU_PORT__
#define __MIMI_CPU_PORT__

#include <stdint.h>
#include "mimi.h"


void mimi_enable_irq(void);
void mimi_disable_irq(void);

void mimi_first_switch(void *first);
void mimi_context_switch(void *from, void *to);

mimi_err mimi_cpu_init();
void *mimi_stack_init(void *entry, void *param, void *stack, uint32_t size);

#endif  // __MIMI_CPU_PORT__
