

#include <stdio.h>
#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "thread.h"
#include "export.h"
#include "log.h"

int led_on(void)
{
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    MIMI_LOG_I("LED ON!\n");
    return MIMI_EOK;
}
MIMI_CMD_EXPORT(ledon, led_on);

int led_off(void)
{
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    MIMI_LOG_I("LED OFF!\n");
    return MIMI_EOK;
}
MIMI_CMD_EXPORT(ledoff, led_off);
