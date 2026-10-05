#include "board.h"
#include <stdio.h>
#include "stm32l4xx_hal.h"
#include "export.h"

/* ------------------------------------------------------------------ */
/*  Board init                                                          */
/* ------------------------------------------------------------------ */
int main(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
}

int mimi_board_init(void)
{
    main();
    NVIC_SetPriority(PendSV_IRQn, 0x0F);    // lowest

    printf("Hello, nucleo-l452!\n");
    return MIMI_EOK;
}
