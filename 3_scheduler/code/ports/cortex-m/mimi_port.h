#ifndef _MIMI_PORT_H_
#define _MIMI_PORT_H_

#include <stdint.h>
#include "mimi.h"
#include "cmsis_gcc.h"

typedef struct {
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;

    // exception
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
} mimi_stack_frame;

mimi_bool mimi_is_in_isr(void);

mimi_inline void mimi_enable_irq(void)
{
    __enable_irq();
}

mimi_inline void mimi_disable_irq(void)
{
    __disable_irq();
}

mimi_inline uint32_t mimi_enter_critical(void)
{
    uint32_t level = __get_PRIMASK();

    __disable_irq();
    return level;
}

mimi_inline void mimi_exit_critical(uint32_t level)
{
    __set_PRIMASK(level);
}

void mimi_context_first_switch(void);
void mimi_context_switch(void);

mimi_err mimi_cpu_init(uint32_t ticksPerSec, void (*handler)(void));
void *mimi_stack_init(void *entry, void *param, void *exit, void *stack, uint32_t size);

#endif // _MIMI_PORT_H_
