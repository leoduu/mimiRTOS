

#include <stdio.h>
#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "thread.h"
#include "export.h"
#include "mimi.h"

#define STACK_SIZE  1024

mimi_tcb led_thread;
uint8_t led_thread_stack[STACK_SIZE] mimi_aligned(8) = {0};

mimi_tcb uart_thread;
uint8_t uart_thread_stack[STACK_SIZE] mimi_aligned(8) = {0};

void led_thread_entry(void *param)
{
    while (1) {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        HAL_Delay(500);
        mimi_thread_yield();
    }
}

void uart_thread_entry(void *param)
{
    int32_t cnt = 0;
    while (1) {
        ++cnt;
        printf("count:%ld\n", cnt);
        HAL_Delay(500);
        mimi_thread_yield();
    }
}

int app_init(void)
{
    mimi_thread_init(&led_thread, led_thread_stack, STACK_SIZE, led_thread_entry, NULL);
    mimi_thread_init(&uart_thread, uart_thread_stack, STACK_SIZE, uart_thread_entry, NULL);

    return MIMI_EOK;
}
MIMI_INIT_APP_EXPORT(app_init);
