#include <stdlib.h>
#include "cm_port.h"
#include "cpuport.h"
#include "stm32l4xx_hal.h"

volatile uint32_t *from_thread_sp;
volatile uint32_t *to_thread_sp;
static void (*mimi_SysTick_Handler)(void) = NULL;

mimi_bool mimi_is_in_isr(void)
{
    return ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0);
}

void mimi_enable_irq(void)
{
    __enable_irq();
}

void mimi_disable_irq(void)
{
    __disable_irq();
}

uint32_t mimi_enter_critical(void)
{
    uint32_t level = __get_PRIMASK();
    __disable_irq();
    return level;
}

void mimi_exit_critical(uint32_t level)
{
    __set_PRIMASK(level);
}

void mimi_context_switch_to(void *to)
{
    from_thread_sp = NULL;
    to_thread_sp = to;

    /* clear systick before trigger PendSV, otherwise SysTick_Handler
     * will be executed immediately When enable interrupts,
     * But the OS hasn't started yet. */
    SysTick->VAL = 0;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;   // enable SysTick interrupt
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;        // clear SYSTick mask
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

mimi_err cortexm_systick_init(uint32_t ticksPerSec, void (*handler)(void))
{
    mimi_SysTick_Handler = handler;
    uint32_t ticks = SystemCoreClock / ticksPerSec;
    if ((ticks - 1UL) > SysTick_LOAD_RELOAD_Msk) {
        return MIMI_ERROR;                              // Reload value impossible
    }

    SysTick->LOAD  = (uint32_t)(ticks - 1UL);           // set reload register
    SysTick->VAL   = 0UL;                               // Load the SysTick Counter Value
    SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk |
                     SysTick_CTRL_TICKINT_Msk;
    return MIMI_EOK;
}

void cortexm_interrupt_init(void)
{
    NVIC_SetPriority(SysTick_IRQn, 0xff);
    NVIC_SetPriority(PendSV_IRQn, 0xff);    // lowest
}

mimi_err mimi_cpu_init(uint32_t ticksPerSec, void (*handler)(void))
{
    cortexm_interrupt_init();
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
