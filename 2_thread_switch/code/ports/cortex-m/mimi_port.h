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

mimi_inline void mimi_enable_irq(void)
{
    __enable_irq();
}

mimi_inline void mimi_disable_irq(void)
{
    __disable_irq();
}

void mimi_context_first_switch(void);
void mimi_context_switch(void);

mimi_err mimi_cpu_init(void);
void *mimi_stack_init(void *entry, void *param, void *stack, uint32_t size);

#endif // _MIMI_PORT_H_
