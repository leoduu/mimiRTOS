
#include <stdio.h>
#include "thread.h"
#include "export.h"
#include "log.h"
#include "ipc.h"
#include "sched.h"

#define BUFF_SIZE   32
#define STACK_SIZE  1024
#define TEST_CNT    5

mimi_tcb mq_senq11;
mimi_tcb mq_senq22;
mimi_tcb mq_recv11;
uint8_t mq_send11_stack[STACK_SIZE] mimi_aligned(8) = {0};
uint8_t mq_send22_stack[STACK_SIZE] mimi_aligned(8) = {0};
uint8_t mq_recv11_stack[STACK_SIZE] mimi_aligned(8) = {0};

mimi_mqueue mq;
uint32_t mq_buffer[BUFF_SIZE];

void mq_send_entry(void *param)
{
    for (int i = 0; i < TEST_CNT; i++) {
        MIMI_LOG_I("send %d\n", i);
        mimi_mqueue_send(&mq, &i);
        mimi_thread_delay(1000);
    }
}

void mq_recv_entry(void *param)
{
    uint32_t cnt = 0;

    while (1) {
        mimi_mqueue_recv(&mq, &cnt, MIMI_TIMEOUT_FOREVER);
        MIMI_LOG_I("\trecv %d\n", cnt);
    }
}

int msgqueue_test_init(void)
{
    mimi_thread_init(&mq_recv11, "mqRecv", 1, 5,
                     mq_recv11_stack, STACK_SIZE, mq_recv_entry, NULL, NULL);

    mimi_mqueue_init(&mq, sizeof(uint32_t), mq_buffer, BUFF_SIZE);
    return MIMI_EOK;
}
MIMI_INIT_APP_EXPORT(msgqueue_test_init);

int mq_test(void)
{
    mimi_thread_init(&mq_senq11, "mqSend11", 1, 5,
                     mq_send11_stack, STACK_SIZE, mq_send_entry, NULL, NULL);
    mimi_thread_init(&mq_senq22, "mqSend22", 1, 5,
                     mq_send22_stack, STACK_SIZE, mq_send_entry, NULL, NULL);


    return MIMI_EOK;
}
MIMI_CMD_EXPORT(mqtest, mq_test);
