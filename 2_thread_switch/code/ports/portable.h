#ifndef _MIMI_PORT_H_
#define _MIMI_PORT_H_

#include <stdint.h>
#include "mimi.h"

mimi_inline void mimi_enable_irq(void);
mimi_inline void mimi_disable_irq(void);

void mimi_context_first_switch(void);
void mimi_context_switch(void);

mimi_err mimi_cpu_init(void);
void *mimi_stack_init(void *entry, void *param, void *stack, uint32_t size);

#endif  // _MIMI_PORT_H_
