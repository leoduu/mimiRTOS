#include <stdlib.h>
#include "cm_port.h"
#include "cpuport.h"
#include "stm32l4xx_hal.h"

volatile void *from_thread_sp;
volatile void *to_thread_sp;

void mimi_enable_irq(void)
{
    __enable_irq();
}

void mimi_disable_irq(void)
{
    __disable_irq();
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void mimi_first_switch(void *first)
{
    from_thread_sp = NULL;
    to_thread_sp = first;

    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;        // trigger PendSV
    __set_MSP(*(uint32_t *)SCB->VTOR);
    __ISB();
    __enable_irq();

    mimi_unreachable;
}

void mimi_context_switch(void *from, void *to)
{
    from_thread_sp = from;
    to_thread_sp = to;
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;        // trigger PendSV
    __enable_irq();
}

mimi_err mimi_cpu_init(void)
{
    NVIC_SetPriority(PendSV_IRQn, 0xff);        // lowest
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
    stack_frame->lr = (uint32_t)exit;
    stack_frame->pc = (uint32_t)entry;
    stack_frame->psr = (uint32_t)0x01000000UL;  // set Thumb flag bit

    return (void *)stack_addr;
}
