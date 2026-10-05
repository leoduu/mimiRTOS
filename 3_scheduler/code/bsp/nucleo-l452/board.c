#include "board.h"
#include "export.h"
#include "log.h"
#include "stm32l4xx_hal.h"

/* ------------------------------------------------------------------ */
/*  Board init                                                          */
/* ------------------------------------------------------------------ */
int main(void);

static void interrupt_init(void)
{
    NVIC_SetPriority(SysTick_IRQn, 0x0E);
    NVIC_SetPriority(PendSV_IRQn, 0x0F);    // lowest
}

int mimi_board_init(void)
{
    main();
    interrupt_init();

    MIMI_LOG_I("nucleo-l452 load\r\n");
    return MIMI_EOK;
}
