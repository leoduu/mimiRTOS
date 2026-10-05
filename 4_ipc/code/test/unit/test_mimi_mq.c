#include <string.h>
#include <unity.h>

#include "mimi.h"
#include "list.h"
#include "ipc.h"
#include "timer.h"
#include "test_mocks.h"

/* thread stubs (tA, curr_thread) in test_mocks.c */

static void reset_all(void) { test_ipc_reset_all(); }

/* mqueue uses a static buffer for init-based testing */
#define MQ_CAPACITY  8
#define MQ_MSG_SIZE  16
uint8_t mq_static_buf[MQ_CAPACITY * MQ_MSG_SIZE];

/* 常规发送统一用这个宏: 队列不满时三个 flag 行为一致,
 * 满了之后 FULL_DROP 会返回 ERESOURCE(见 test_mq_send_full_drop)。
 */
#define SEND_NORMAL  MIMI_MQ_FULL_DROP

static void mq_init(mimi_mqueue *mq)
{
    mimi_mqueue_init(mq, MQ_MSG_SIZE, mq_static_buf, sizeof(mq_static_buf));
}

/* 写一条定长消息, 不足的部分补 0 */
static void mkmsg(uint8_t *buf, const char *s)
{
    memset(buf, 0, MQ_MSG_SIZE);
    strncpy((char *)buf, s, MQ_MSG_SIZE - 1);
}

/* ====================================================================== */
/*  Group A: init                                                         */
/* ====================================================================== */

static void test_mq_init_basic(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    /* msg_size 存的是 word 数（mq_copy_words 按 word 拷贝），不是字节数 */
    TEST_ASSERT_EQUAL_UINT16(MQ_MSG_SIZE / 4, mq.msg_size);
    TEST_ASSERT_EQUAL_UINT16(MQ_CAPACITY, mq.capacity);
    TEST_ASSERT_EQUAL_UINT16(0, mq.cnt);
    TEST_ASSERT_EQUAL_PTR(mq.buffer, mq.prod);
    TEST_ASSERT_EQUAL_PTR(mq.buffer, mq.cons);
    TEST_ASSERT_EQUAL_PTR(mq.buffer + (MQ_CAPACITY * (MQ_MSG_SIZE / 4)), mq.end);
    TEST_ASSERT_EQUAL_PTR(mq_static_buf, mq.buffer);
    TEST_ASSERT_NULL(tA.mq_buffer);   /* 零拷贝收件地址记在 TCB 上, 不在 mq 上 */
    TEST_ASSERT_TRUE(mimi_list_empty(&mq.recv_suspend_list));
}

/* init 是唯一会做除法的入口, 四类非法入参都必须触发断言:
 * msg_size == 0          -> buffer_size / msg_size 除零（修复前进程直接崩）
 * buffer == NULL         -> 后续 memcpy 空指针
 * buffer_size < msg_size -> capacity 0, send/recv 的 % capacity 同样除零
 * mq == NULL             -> 空指针
 */
static void test_mq_init_reject_bad_params(void)
{
    mimi_mqueue mq;

    TEST_EXPECT_ASSERT(mimi_mqueue_init(&mq, 0, mq_static_buf,
                                        sizeof(mq_static_buf)));
    TEST_EXPECT_ASSERT(mimi_mqueue_init(&mq, MQ_MSG_SIZE, NULL,
                                        sizeof(mq_static_buf)));
    TEST_EXPECT_ASSERT(mimi_mqueue_init(&mq, MQ_MSG_SIZE, mq_static_buf, 8));
    TEST_EXPECT_ASSERT(mimi_mqueue_init(NULL, MQ_MSG_SIZE, mq_static_buf,
                                        sizeof(mq_static_buf)));
}

/* 合法参数下 capacity 按整条消息数取整, 余数丢弃 */
static void test_mq_init_truncates_partial_msg(void)
{
    mimi_mqueue mq;
    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_mqueue_init(&mq, MQ_MSG_SIZE, mq_static_buf,
                         MQ_MSG_SIZE * 3 + 5));
    TEST_ASSERT_EQUAL_UINT16(3, mq.capacity);
}

/* ====================================================================== */
/*  Group B: send / recv                                                  */
/* ====================================================================== */

static void test_mq_send_one(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t data[MQ_MSG_SIZE] = {0xAA};
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_mqueue_send(&mq, data, SEND_NORMAL));

    TEST_ASSERT_EQUAL_UINT16(1, mq.cnt);
    /* 生产者指针必须前进一整条消息（MQ_MSG_SIZE/4 个 word），不是 1 个 word */
    TEST_ASSERT_EQUAL_PTR(mq.buffer + (MQ_MSG_SIZE / 4), mq.prod);
}

static void test_mq_send_recv_data_match(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t tx[MQ_MSG_SIZE];
    uint8_t rx[MQ_MSG_SIZE] = {0};
    mkmsg(tx, "hello");

    mimi_mqueue_send(&mq, tx, SEND_NORMAL);
    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT));

    TEST_ASSERT_EQUAL_STRING("hello", (char *)rx);
}

/* 两条消息必须落在两个不同的槽位, 不能互相覆盖 */
static void test_mq_send_two_keeps_both(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t a[MQ_MSG_SIZE], b[MQ_MSG_SIZE], rx[MQ_MSG_SIZE] = {0};
    mkmsg(a, "AAA");
    mkmsg(b, "BBB");

    mimi_mqueue_send(&mq, a, SEND_NORMAL);
    mimi_mqueue_send(&mq, b, SEND_NORMAL);
    TEST_ASSERT_EQUAL_UINT16(2, mq.cnt);

    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_STRING("AAA", (char *)rx);
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_STRING("BBB", (char *)rx);
}

static void test_mq_send_null_buf(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    TEST_EXPECT_ASSERT(mimi_mqueue_send(&mq, NULL, SEND_NORMAL));
}

static void test_mq_recv_empty_nowait(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t rx[MQ_MSG_SIZE];
    TEST_ASSERT_EQUAL_INT(MIMI_ERESOURCE,
        mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT));
}

static void test_mq_send_recv_wrap(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t tx[MQ_MSG_SIZE], rx[MQ_MSG_SIZE] = {0};
    mkmsg(tx, "wrap");
    for (int i = 0; i < MQ_CAPACITY; i++) {
        mimi_mqueue_send(&mq, tx, SEND_NORMAL);
        mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    }
    mimi_mqueue_send(&mq, tx, SEND_NORMAL);
    TEST_ASSERT_EQUAL_UINT16(1, mq.cnt);
    /* 走了 MQ_CAPACITY 整条后 prod/cons 都应已回绕到 buffer 起点 */
    TEST_ASSERT_EQUAL_PTR(mq.buffer, mq.cons);
}

/* 队列满时 FULL_DROP: 拒绝入队并返回 ERESOURCE, 队列内容一个字节都不动 */
static void test_mq_send_full_drop(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t tx[MQ_MSG_SIZE], rx[MQ_MSG_SIZE] = {0};
    mkmsg(tx, "old");
    for (int i = 0; i < MQ_CAPACITY; i++) {
        mimi_mqueue_send(&mq, tx, MIMI_MQ_FULL_DROP);
    }
    TEST_ASSERT_EQUAL_UINT16(MQ_CAPACITY, mq.cnt);

    uint8_t late[MQ_MSG_SIZE];
    mkmsg(late, "new");
    TEST_ASSERT_EQUAL_INT(MIMI_ERESOURCE,
        mimi_mqueue_send(&mq, late, MIMI_MQ_FULL_DROP));

    /* 数量不变, 队首仍是原来那条 */
    TEST_ASSERT_EQUAL_UINT16(MQ_CAPACITY, mq.cnt);
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_STRING("old", (char *)rx);
}

/* 填满队列后 OVERWRITE: 丢掉最旧的一条, 新消息排到队尾 */
static void test_mq_send_full_overwrite(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t tx[MQ_MSG_SIZE], rx[MQ_MSG_SIZE] = {0};
    char name[8];
    for (int i = 0; i < MQ_CAPACITY; i++) {
        snprintf(name, sizeof(name), "m%d", i);
        mkmsg(tx, name);
        mimi_mqueue_send(&mq, tx, SEND_NORMAL);
    }

    uint8_t urgent[MQ_MSG_SIZE];
    mkmsg(urgent, "NEW");
    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_mqueue_send(&mq, urgent, MIMI_MQ_FULL_OVERWRITE));

    /* 覆盖: 总数不能超过容量, 最旧的 m0 被顶掉 */
    TEST_ASSERT_EQUAL_UINT16(MQ_CAPACITY, mq.cnt);
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_STRING("m1", (char *)rx);

    /* 一路读到队尾, 最后一条是刚写进去的 */
    for (int i = 2; i < MQ_CAPACITY; i++) {
        mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    }
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_STRING("NEW", (char *)rx);
    TEST_ASSERT_EQUAL_UINT16(0, mq.cnt);
}

/* URGENT: 插到队首, 但不许吃掉已经在排队的消息 */
static void test_mq_send_urgent_jumps_queue(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t a[MQ_MSG_SIZE], b[MQ_MSG_SIZE], u[MQ_MSG_SIZE], rx[MQ_MSG_SIZE] = {0};
    mkmsg(a, "AAA");
    mkmsg(b, "BBB");
    mkmsg(u, "URG");

    mimi_mqueue_send(&mq, a, SEND_NORMAL);
    mimi_mqueue_send(&mq, b, SEND_NORMAL);
    mimi_mqueue_send(&mq, u, MIMI_MQ_URGENT);

    TEST_ASSERT_EQUAL_UINT16(3, mq.cnt);
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_STRING("URG", (char *)rx);
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_STRING("AAA", (char *)rx);
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_STRING("BBB", (char *)rx);
}

/* 队列满时 URGENT: 顶掉最旧那条, 总数不能超过容量 */
static void test_mq_send_urgent_when_full(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t tx[MQ_MSG_SIZE], rx[MQ_MSG_SIZE] = {0};
    char name[8];
    for (int i = 0; i < MQ_CAPACITY; i++) {
        snprintf(name, sizeof(name), "m%d", i);
        mkmsg(tx, name);
        mimi_mqueue_send(&mq, tx, MIMI_MQ_FULL_DROP);
    }
    TEST_ASSERT_EQUAL_UINT16(MQ_CAPACITY, mq.cnt);

    uint8_t urgent[MQ_MSG_SIZE];
    mkmsg(urgent, "URG");
    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_mqueue_send(&mq, urgent, MIMI_MQ_URGENT));

    TEST_ASSERT_EQUAL_UINT16(MQ_CAPACITY, mq.cnt);
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_STRING("URG", (char *)rx);   /* 紧急消息插到最前 */
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_STRING("m1", (char *)rx);    /* 最旧的 m0 被顶掉 */
}

/* ====================================================================== */
/*  Group C: blocking & wakeup                                            */
/* ====================================================================== */

static void test_mq_recv_empty_blocking(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t rx[MQ_MSG_SIZE];
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_FOREVER);

    TEST_ASSERT_EQUAL_INT(1, sleep_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_SUSPEND, tA.status);
    TEST_ASSERT_EQUAL_PTR(&mq.recv_suspend_list, sleep_list);
}

/* 有接收者在等时, send 不走环形缓冲, 而是零拷贝直投: 数据写进接收者 TCB 上
 * 登记的 mq_buffer。这条路径必须验证 (a) 数据真的落地 (b) 指针用完被清掉,
 * 否则"唤醒了但没收到"和"下次 send 写到已消失的栈上"都测不出来。
 */
static void test_mq_recv_wakeup_on_send(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t rx[MQ_MSG_SIZE] = {0};
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_INT(1, sleep_called);
    TEST_ASSERT_EQUAL_PTR(rx, tA.mq_buffer);   /* 阻塞前先登记好收件地址 */

    test_curr_thread = &tB;
    uint8_t tx[MQ_MSG_SIZE];
    mkmsg(tx, "wake");
    mimi_mqueue_send(&mq, tx, MIMI_MQ_FULL_DROP);

    TEST_ASSERT_EQUAL_INT(1, wakeup_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_READY, tA.status);
    TEST_ASSERT_EQUAL_STRING("wake", (char *)rx);
    TEST_ASSERT_NULL(tA.mq_buffer);            /* 用完必须清, 不能留悬垂指针 */
    /* 直投不进环形缓冲 */
    TEST_ASSERT_EQUAL_UINT16(0, mq.cnt);
    TEST_ASSERT_TRUE(mimi_list_empty(&mq.recv_suspend_list));
}

/* 接收超时: 和 sem/mutex 一样, 原因记在 thread->error 上。
 *
 * 注意: mimi_mqueue_recv 里 "唤醒后清 curr->mq_buffer" 那几行是在
 * mimi_schedule() 之后才执行的, 同步 mock 没有真正的上下文切换, recv 早就
 * 返回了, 这段尾巴永远跑不到。所以这里只验能观测到的部分; mq_buffer 的
 * 清理由 test_mq_recv_wakeup_on_send 那条直投路径覆盖。
 */
static void test_mq_recv_timeout(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t rx[MQ_MSG_SIZE];
    mimi_mqueue_recv(&mq, rx, 100);
    TEST_ASSERT_EQUAL_INT(1, sleep_called);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, tA.timer.status);

    fake_tick = 101;
    mimi_timer_check();

    TEST_ASSERT_EQUAL_INT(MIMI_ETIMEOUT, tA.error);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_READY, tA.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&mq.recv_suspend_list));
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, tA.timer.status);
}

/* Regression: see test_sem_wakeup_clears_timeout_timer — a receiver woken by
 * a send must not keep a RUNNING timeout timer behind.
 */
static void test_mq_wakeup_clears_timeout_timer(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t rx[MQ_MSG_SIZE];
    mimi_mqueue_recv(&mq, rx, 100);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, tA.timer.status);

    test_curr_thread = &tB;
    uint8_t tx[MQ_MSG_SIZE];
    mkmsg(tx, "wake");
    mimi_mqueue_send(&mq, tx, SEND_NORMAL);     /* wake tA, timeout not reached */

    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, tA.timer.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list));

    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_timer_start(&tA.timer, 50, MIMI_TIMER_ONCE));
}

static void test_mq_multiple_roundtrip(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    uint8_t tx[MQ_MSG_SIZE], rx[MQ_MSG_SIZE] = {0};
    mkmsg(tx, "test");
    for (int i = 0; i < MQ_CAPACITY; i++) {
        mimi_mqueue_send(&mq, tx, SEND_NORMAL);
    }
    for (int i = 0; i < MQ_CAPACITY; i++) {
        TEST_ASSERT_EQUAL_INT(MIMI_EOK,
            mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT));
        TEST_ASSERT_EQUAL_STRING("test", (char *)rx);
    }
    TEST_ASSERT_EQUAL_UINT16(0, mq.cnt);
}

/* ====================================================================== */
/*  Group D: NULL guards                                                  */
/* ====================================================================== */

static void test_mq_send_null_mq(void)
{
    TEST_EXPECT_ASSERT(mimi_mqueue_send(NULL, mq_static_buf, SEND_NORMAL));
}

static void test_mq_recv_null_mq(void)
{
    TEST_EXPECT_ASSERT(
        mimi_mqueue_recv(NULL, mq_static_buf, MIMI_TIMEOUT_NOWAIT));
}

static void test_mq_recv_null_buf(void)
{
    mimi_mqueue mq;
    mq_init(&mq);

    TEST_EXPECT_ASSERT(mimi_mqueue_recv(&mq, NULL, MIMI_TIMEOUT_NOWAIT));
}

/* ====================================================================== */
/*  Test runner                                                           */
/* ====================================================================== */

int run_mq_tests(void)
{
    UNITY_BEGIN();

    reset_all(); RUN_TEST(test_mq_init_basic);
    reset_all(); RUN_TEST(test_mq_init_reject_bad_params);
    reset_all(); RUN_TEST(test_mq_init_truncates_partial_msg);

    reset_all(); RUN_TEST(test_mq_send_one);
    reset_all(); RUN_TEST(test_mq_send_recv_data_match);
    reset_all(); RUN_TEST(test_mq_send_two_keeps_both);
    reset_all(); RUN_TEST(test_mq_send_null_buf);
    reset_all(); RUN_TEST(test_mq_recv_empty_nowait);
    reset_all(); RUN_TEST(test_mq_send_recv_wrap);
    reset_all(); RUN_TEST(test_mq_send_full_drop);
    reset_all(); RUN_TEST(test_mq_send_full_overwrite);
    reset_all(); RUN_TEST(test_mq_send_urgent_jumps_queue);
    reset_all(); RUN_TEST(test_mq_send_urgent_when_full);

    reset_all(); RUN_TEST(test_mq_recv_empty_blocking);
    reset_all(); RUN_TEST(test_mq_recv_wakeup_on_send);
    reset_all(); RUN_TEST(test_mq_recv_timeout);
    reset_all(); RUN_TEST(test_mq_wakeup_clears_timeout_timer);
    reset_all(); RUN_TEST(test_mq_multiple_roundtrip);

    reset_all(); RUN_TEST(test_mq_send_null_mq);
    reset_all(); RUN_TEST(test_mq_recv_null_mq);
    reset_all(); RUN_TEST(test_mq_recv_null_buf);

    return UNITY_END();
}
