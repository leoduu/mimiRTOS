#include <stdlib.h>
#include "mimi_port.h"

#define SCB_BASE           0xE000ED00UL
#define SYSTICK_BASE       0xE000E010UL

#define SCB_ICSR           (*(volatile uint32_t *)(SCB_BASE     + 0x004UL))
#define SCB_VTOR           (*(volatile uint32_t *)(SCB_BASE     + 0x008UL))
#define SYSTICK_CTRL       (*(volatile uint32_t *)(SYSTICK_BASE + 0x000UL))
#define SYSTICK_LOAD       (*(volatile uint32_t *)(SYSTICK_BASE + 0x004UL))
#define SYSTICK_VAL        (*(volatile uint32_t *)(SYSTICK_BASE + 0x008UL))

#define ICSR_VECTACTIVE_Msk    (0x1FFUL)
#define ICSR_PENDSTCLR_Msk     (1UL << 25)
#define ICSR_PENDSVSET_Msk     (1UL << 28)

#define SYSTICK_ENABLE_Msk     (1UL << 0)
#define SYSTICK_TICKINT_Msk    (1UL << 1)
#define SYSTICK_CLKSOURCE_Msk  (1UL << 2)
#define SYSTICK_RELOAD_Msk     (0xFFFFFFUL)

static void (*mimi_SysTick_Handler)(void) = NULL;

mimi_bool mimi_is_in_isr(void)
{
    return ((SCB_ICSR & ICSR_VECTACTIVE_Msk) != 0);
}

void mimi_context_first_switch(void)
{
    SYSTICK_VAL = 0;
    SYSTICK_CTRL |= SYSTICK_ENABLE_Msk;     // enable SysTick interrupt

    SCB_ICSR = ICSR_PENDSTCLR_Msk;          // clear pending SysTick
    SCB_ICSR = ICSR_PENDSVSET_Msk;          // trigger PendSV

    /* Reset the MSP to the initial value specified in the vector table. */
    __set_MSP(*(volatile uint32_t *)SCB_VTOR);
    __ISB();
}

void mimi_context_switch(void)
{
    SCB_ICSR = ICSR_PENDSVSET_Msk;          // trigger PendSV
}

mimi_err cortexm_systick_init(uint32_t ticksPerSec, void (*handler)(void))
{
    mimi_SysTick_Handler = handler;

    extern uint32_t SystemCoreClock;
    uint32_t ticks = SystemCoreClock / ticksPerSec;
    if ((ticks - 1UL) > SYSTICK_RELOAD_Msk) {
        return MIMI_ERROR;
    }

    SYSTICK_LOAD = (uint32_t)(ticks - 1UL);
    SYSTICK_VAL  = 0UL;
    SYSTICK_CTRL = SYSTICK_CLKSOURCE_Msk | SYSTICK_TICKINT_Msk;
    return MIMI_EOK;
}

mimi_err mimi_cpu_init(uint32_t ticksPerSec, void (*handler)(void))
{
    return cortexm_systick_init(ticksPerSec, handler);
}

void *mimi_stack_init(void *entry, void *param, void *exit, void *stack, uint32_t size)
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

/* -------------------------------------------------------------------------- */
/*  interrupt handler                                                         */
/* -------------------------------------------------------------------------- */
void SysTick_Handler(void)
{
    mimi_SysTick_Handler();
}
