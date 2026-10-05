#include "mimi_port.h"

#define SCB_BASE            0xE000ED00UL
#define SCB_ICSR            (*(volatile uint32_t *)(SCB_BASE     + 0x004UL))
#define SCB_VTOR            (*(volatile uint32_t *)(SCB_BASE     + 0x008UL))
#define ICSR_PENDSVSET_Msk  (1UL << 28)

void mimi_context_first_switch(void)
{
    SCB_ICSR = ICSR_PENDSVSET_Msk;          // trigger PendSV

    /* Reset the MSP to the initial value specified in the vector table. */
    __set_MSP(*(volatile uint32_t *)SCB_VTOR);
    __ISB();
}

void mimi_context_switch(void)
{
    SCB_ICSR = ICSR_PENDSVSET_Msk;          // trigger PendSV
}

mimi_err mimi_cpu_init(void)
{
    return MIMI_EOK;
}

void *mimi_stack_init(void *entry, void *param, void *stack, uint32_t size)
{
    uint8_t *stack_addr;
    stack_addr = (uint8_t *)stack + size - sizeof(mimi_stack_frame);
    stack_addr = (uint8_t *)mimi_align_down((uint32_t)stack_addr, 8);
    mimi_stack_frame *stack_frame = (mimi_stack_frame *)stack_addr;

    for (int i = 0; i < sizeof(mimi_stack_frame) / sizeof(uint32_t); i++) {
        ((uint32_t *)stack_frame)[i] = 0;
    }
    stack_frame->r0 = (uint32_t)param;
    stack_frame->pc = (uint32_t)entry;
    stack_frame->psr = (uint32_t)0x01000000UL;  // set Thumb flag bit

    return (void *)stack_addr;
}
