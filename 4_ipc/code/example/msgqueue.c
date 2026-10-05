
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
        mimi_mqueue_send(&mq, &i, MIMI_MQ_FULL_DROP);
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


/* -------------------------------------------------------------------------- */
/*  队列满时的两种策略                                                        */
/* -------------------------------------------------------------------------- */
/* 队列容量 4, 生产者在消费者接收之前连发 8 条, 必然满。
 *
 *   FULL_DROP      : 满了就丢弃, 队列里留下的是最早的 0 1 2 3。
 *
 *   FULL_OVERWRITE : 满了就顶掉最旧的一条, 队列里留下的是最新的 4 5 6 7。
 *
 */

#define MQ_FULL_CAP     4
#define MQ_FULL_CNT     8

mimi_tcb mq_full_prod;
mimi_tcb mq_full_cons;

uint8_t mq_full_prod_stack[STACK_SIZE] mimi_aligned(8) = {0};
uint8_t mq_full_cons_stack[STACK_SIZE] mimi_aligned(8) = {0};

mimi_mqueue mq_full;
uint32_t mq_full_buffer[MQ_FULL_CAP];
mimi_mqueue_flag mq_full_flag = MIMI_MQ_FULL_DROP;

static void mq_full_prod_entry(void *param)
{
    for (uint32_t i = 0; i < MQ_FULL_CNT; i++) {
        mimi_err ret = mimi_mqueue_send(&mq_full, &i, mq_full_flag);
        if (ret == MIMI_ERESOURCE) {
            MIMI_LOG_I("send %d -> full, DROPPED\n", (int)i);
        } else {
            MIMI_LOG_I("send %d\n", (int)i);
        }
    }
    MIMI_LOG_I("prod done, cnt %d\n", (int)mq_full.cnt);
}

static void mq_full_cons_entry(void *param)
{
    uint32_t val = 0;

    mimi_thread_delay(500);             /* 等生产者把队列灌满 */

    while (mimi_mqueue_recv(&mq_full, &val, MIMI_TIMEOUT_NOWAIT) == MIMI_EOK) {
        MIMI_LOG_I("\trecv %d\n", (int)val);
    }
    MIMI_LOG_I("cons done, cnt %d\n", (int)mq_full.cnt);
}

static int mq_full_test(mimi_mqueue_flag flag, const char *policy)
{
    mq_full_flag = flag;
    mimi_mqueue_init(&mq_full, sizeof(uint32_t),
                     mq_full_buffer, sizeof(mq_full_buffer));

    MIMI_LOG_I("== %s: cap %d, send %d ==\n", policy, MQ_FULL_CAP, MQ_FULL_CNT);

    mimi_thread_init(&mq_full_prod, "mqFullProd", 8, 5,
                     mq_full_prod_stack, STACK_SIZE, mq_full_prod_entry, NULL, NULL);
    mimi_thread_init(&mq_full_cons, "mqFullCons", 12, 5,
                     mq_full_cons_stack, STACK_SIZE, mq_full_cons_entry, NULL, NULL);

    return MIMI_EOK;
}

int mq_drop_test(void)
{
    return mq_full_test(MIMI_MQ_FULL_DROP, "FULL_DROP");
}
MIMI_CMD_EXPORT(mqdrop, mq_drop_test);

int mq_overwrite_test(void)
{
    return mq_full_test(MIMI_MQ_FULL_OVERWRITE, "FULL_OVERWRITE");
}
MIMI_CMD_EXPORT(mqoverwrite, mq_overwrite_test);
