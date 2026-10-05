#include "board.h"
#include "main.h"
#include "usart.h"
#include "ipc.h"
#include "log.h"
#include "export.h"
#include <string.h>

#define DMA_RX_RECVIVE_BUFFER 256

extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;

static mimi_sem dma_tx_sem;

/* ------------------------------------------------------------------ */
/*  DMA ISR / callback                                                  */
/* ------------------------------------------------------------------ */

void DMA1_Channel7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_tx);
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        huart->gState = HAL_UART_STATE_READY;
        mimi_sem_release(&dma_tx_sem);
    }
}

static uint8_t  rx_buf[DMA_RX_RECVIVE_BUFFER];
static uint16_t rx_len;
static mimi_sem rx_sem;

void DMA1_Channel6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_rx);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
    if (huart->Instance == USART2) {
        rx_len = size;
        mimi_sem_release(&rx_sem);
    }
}

/* ------------------------------------------------------------------ */
/*  Board init                                                          */
/* ------------------------------------------------------------------ */
int main(void);

void mimi_board_init(void)
{
    main();

    mimi_sem_init(&dma_tx_sem, 1, 0);
    mimi_sem_init(&rx_sem, 1, 0);

    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, sizeof(rx_buf));

    MIMI_LOG_I("nucleo-l452 load\r\n");
}

/* ------------------------------------------------------------------ */
/*  Log output (DMA)                                                  */
/* ------------------------------------------------------------------ */

void log_output(const uint8_t *buffer, size_t len)
{
    HAL_StatusTypeDef ret = 0;
    ret = HAL_UART_Transmit_DMA(&huart2, (uint8_t *)buffer, (uint16_t)len);
     if (ret == HAL_OK) {
        mimi_sem_take(&dma_tx_sem, MIMI_TIMEOUT_FOREVER);
    }
}

log_output_func mimi_board_get_log_output_func(void)
{
    return log_output;
}

/* -------------------------------------------------------------------------- */
/*  console input (DMA)                                                       */
/* -------------------------------------------------------------------------- */
#if MIMI_CONSOLE

size_t console_input(uint8_t *buffer, size_t max_len)
{
    mimi_sem_take(&rx_sem, MIMI_TIMEOUT_FOREVER);
    size_t len = (rx_len < max_len) ? rx_len : max_len;
    memcpy(buffer, rx_buf, len);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, sizeof(rx_buf));
    return len;
}

console_input_func mimi_board_get_console_input_func(void)
{
    return console_input;
}
#endif // MIMI_CONSOLE
