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
static volatile uint16_t rx_len;
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

static void interrupt_init(void)
{
    NVIC_SetPriority(SysTick_IRQn, 0x0E);
    NVIC_SetPriority(PendSV_IRQn, 0x0F);    // lowest
}

int mimi_board_init(void)
{
    main();
    interrupt_init();

    mimi_sem_init(&dma_tx_sem, 1, 0);
    mimi_sem_init(&rx_sem, 1, 0);

    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, sizeof(rx_buf)) != HAL_OK) {
        MIMI_LOG_E("uart rx start failed\r\n");
    }

    MIMI_LOG_I("nucleo-l452 load\r\n");
    return MIMI_EOK;
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

log_output_func mimi_log_output_func(void)
{
    return log_output;
}

/* ------------------------------------------------------------------ */
/*  Assert output (polled, no scheduler / DMA / HAL state)            */
/* ------------------------------------------------------------------ */
void mimi_assert_output(const char *s, int len)
{
    USART_TypeDef *uart = huart2.Instance;
    uint32_t spin;

    if (uart == NULL) {
        return;
    }

    /* Stop the log DMA from competing for TDR while we poll. */
    uart->CR3 &= ~USART_CR3_DMAT;

    for (size_t i = 0; i < len; i++) {
        spin = 100000U;
        while (!(uart->ISR & USART_ISR_TXE) && (--spin != 0U)) {
        }
        if (spin == 0U) {
            return;
        }
        uart->TDR = (uint8_t)s[i];
    }

    spin = 100000U;
    while (!(uart->ISR & USART_ISR_TC) && (--spin != 0U)) {
    }
}

/* -------------------------------------------------------------------------- */
/*  console input (DMA)                                                       */
/* -------------------------------------------------------------------------- */
#if MIMI_CONSOLE
#include "console.h"

size_t console_input(uint8_t *buffer, size_t max_len)
{
    mimi_sem_take(&rx_sem, MIMI_TIMEOUT_FOREVER);

    /* The RX DMA runs in circular mode: stop it before touching rx_buf,
       otherwise it can overwrite bytes while we copy them.  A failure here
       means reception was already stopped, which is safe for the copy. */
    if (HAL_UART_AbortReceive(&huart2) != HAL_OK) {
        MIMI_LOG_W("console rx stop failed\r\n");
    }

    size_t len = ((size_t)rx_len < max_len) ? (size_t)rx_len : max_len;
    memcpy(buffer, rx_buf, len);
    rx_len = 0;

    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, sizeof(rx_buf)) != HAL_OK) {
        MIMI_LOG_E("console rx rearm failed\r\n");
    }

    return len;
}

console_input_func mimi_console_input_func(void)
{
    return console_input;
}
#endif // MIMI_CONSOLE
