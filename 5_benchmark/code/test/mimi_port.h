#ifndef _MIMI_PORT_H_
#define _MIMI_PORT_H_

#include <stdint.h>
#include "mimi.h"

/* native host port for [env:unit]: no real interrupts, stubs in test_mocks.c */
void mimi_enable_irq(void);
void mimi_disable_irq(void);
uint32_t mimi_enter_critical(void);
void mimi_exit_critical(uint32_t level);

#endif
