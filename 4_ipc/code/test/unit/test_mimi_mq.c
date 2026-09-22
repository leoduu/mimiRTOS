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
uint8_t mq_static_buf[MQ_CAPACITY * 16];

/* ====================================================================== */
/*  Group A: init  (2 tests)                                              */
/* ====================================================================== */

static void test_mq_init_basic(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    TEST_ASSERT_EQUAL_UINT16(16, mq.msg_size);
    TEST_ASSERT_EQUAL_UINT16(MQ_CAPACITY, mq.capacity);
    TEST_ASSERT_EQUAL_UINT16(0, mq.cnt);
    TEST_ASSERT_EQUAL_UINT16(0, mq.prod);
    TEST_ASSERT_EQUAL_UINT16(0, mq.cons);
    TEST_ASSERT_TRUE(mimi_list_empty(&mq.recv_suspend_list));
}

static void test_mq_init_null(void)
{
    TEST_EXPECT_ASSERT(mimi_mqueue_send(NULL, mq_static_buf));
    TEST_EXPECT_ASSERT(mimi_mqueue_recv(NULL, mq_static_buf, MIMI_TIMEOUT_NOWAIT));
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
    TEST_EXPECT_ASSERT(mimi_mqueue_init(&mq, 16, NULL,
                                        sizeof(mq_static_buf)));
    TEST_EXPECT_ASSERT(mimi_mqueue_init(&mq, 16, mq_static_buf, 8));
    TEST_EXPECT_ASSERT(mimi_mqueue_init(NULL, 16, mq_static_buf,
                                        sizeof(mq_static_buf)));
}

/* 合法参数下 capacity 按整条消息数取整, 余数丢弃 */
static void test_mq_init_truncates_partial_msg(void)
{
    mimi_mqueue mq;
    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_mqueue_init(&mq, 16, mq_static_buf, 16 * 3 + 5));
    TEST_ASSERT_EQUAL_UINT16(3, mq.capacity);
}

/* ====================================================================== */
/*  Group B: send / recv basic  (6 tests)                                 */
/* ====================================================================== */

static void test_mq_send_one(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    uint8_t data[16] = {0xAA};
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_mqueue_send(&mq, data));

    TEST_ASSERT_EQUAL_UINT16(1, mq.cnt);
    TEST_ASSERT_EQUAL_UINT16(1, mq.prod);
}

static void test_mq_send_recv_data_match(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    uint8_t tx[16] = "hello";
    uint8_t rx[16] = {0};

    mimi_mqueue_send(&mq, tx);
    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT));

    TEST_ASSERT_EQUAL_STRING("hello", (char *)rx);
}

static void test_mq_send_null_buf(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));
    TEST_EXPECT_ASSERT(mimi_mqueue_send(&mq, NULL));
}

static void test_mq_recv_empty_nowait(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    uint8_t rx[16];
    TEST_ASSERT_EQUAL_INT(MIMI_ERESOURCE,
        mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT));
}

static void test_mq_send_until_full(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    uint8_t data[16] = {0};
    for (int i = 0; i < MQ_CAPACITY; i++) {
        TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_mqueue_send(&mq, data));
    }
    TEST_ASSERT_EQUAL_UINT16(MQ_CAPACITY, mq.cnt);

    TEST_ASSERT_EQUAL_INT(MIMI_ERESOURCE, mimi_mqueue_send(&mq, data));
}

static void test_mq_send_recv_wrap(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    uint8_t tx[16] = "wrap", rx[16] = {0};
    for (int i = 0; i < MQ_CAPACITY; i++) {
        mimi_mqueue_send(&mq, tx);
        mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT);
    }
    mimi_mqueue_send(&mq, tx);
    TEST_ASSERT_EQUAL_UINT16(1, mq.cnt);
    TEST_ASSERT_EQUAL_UINT16(0, mq.cons);
}

/* ====================================================================== */
/*  Group C: blocking & wakeup  (4 tests)                                 */
/* ====================================================================== */

static void test_mq_recv_empty_blocking(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    uint8_t rx[16];
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_FOREVER);

    TEST_ASSERT_EQUAL_INT(1, sleep_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_SUSPEND, tA.status);
}

static void test_mq_recv_wakeup_on_send(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    uint8_t rx[16];
    mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_INT(1, sleep_called);

    test_curr_thread = &tB;
    uint8_t tx[16] = "wake";
    mimi_mqueue_send(&mq, tx);

    TEST_ASSERT_EQUAL_INT(1, wakeup_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_READY, tA.status);
}

static void test_mq_recv_timeout(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    uint8_t rx[16];
    mimi_mqueue_recv(&mq, rx, 100);

    TEST_ASSERT_EQUAL_INT(1, sleep_called);
    fake_tick = 101;
    mimi_timer_check();
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_TIMEROUT, tA.timer.status);
}

/* Regression: see test_sem_wakeup_clears_timeout_timer — a receiver woken by
 * a send must not keep a RUNNING timeout timer behind.
 */
static void test_mq_wakeup_clears_timeout_timer(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    uint8_t rx[16];
    mimi_mqueue_recv(&mq, rx, 100);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, tA.timer.status);

    test_curr_thread = &tB;
    uint8_t tx[16] = "wake";
    mimi_mqueue_send(&mq, tx);              /* wake tA, timeout not reached */

    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, tA.timer.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list));

    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_timer_start(&tA.timer, 50, MIMI_TIMER_ONCE));
}

static void test_mq_multiple_roundtrip(void)
{
    mimi_mqueue mq;
    mimi_mqueue_init(&mq, 16, mq_static_buf, sizeof(mq_static_buf));

    uint8_t tx[16] = "test", rx[16] = {0};
    for (int i = 0; i < MQ_CAPACITY; i++) {
        mimi_mqueue_send(&mq, tx);
    }
    for (int i = 0; i < MQ_CAPACITY; i++) {
        TEST_ASSERT_EQUAL_INT(MIMI_EOK,
            mimi_mqueue_recv(&mq, rx, MIMI_TIMEOUT_NOWAIT));
    }
    TEST_ASSERT_EQUAL_UINT16(0, mq.cnt);
}

/* ====================================================================== */
/*  Group D: edge cases  (2 tests)                                        */
/* ====================================================================== */

static void test_mq_recv_null_mq(void)
{
    TEST_EXPECT_ASSERT(mimi_mqueue_recv(NULL, mq_static_buf, MIMI_TIMEOUT_NOWAIT));
}

static void test_mq_send_null_mq(void)
{
    TEST_EXPECT_ASSERT(mimi_mqueue_send(NULL, mq_static_buf));
}

/* ====================================================================== */
/*  Test runner                                                            */
/* ====================================================================== */

int run_mq_tests(void)
{
    UNITY_BEGIN();

    reset_all(); RUN_TEST(test_mq_init_basic);
    reset_all(); RUN_TEST(test_mq_init_null);
    reset_all(); RUN_TEST(test_mq_init_reject_bad_params);
    reset_all(); RUN_TEST(test_mq_init_truncates_partial_msg);

    reset_all(); RUN_TEST(test_mq_send_one);
    reset_all(); RUN_TEST(test_mq_send_recv_data_match);
    reset_all(); RUN_TEST(test_mq_send_null_mq);
    reset_all(); RUN_TEST(test_mq_send_null_buf);
    reset_all(); RUN_TEST(test_mq_recv_null_mq);
    reset_all(); RUN_TEST(test_mq_recv_empty_nowait);
    reset_all(); RUN_TEST(test_mq_send_until_full);
    reset_all(); RUN_TEST(test_mq_send_recv_wrap);

    reset_all(); RUN_TEST(test_mq_recv_empty_blocking);
    reset_all(); RUN_TEST(test_mq_recv_wakeup_on_send);
    reset_all(); RUN_TEST(test_mq_recv_timeout);
    reset_all(); RUN_TEST(test_mq_wakeup_clears_timeout_timer);
    reset_all(); RUN_TEST(test_mq_multiple_roundtrip);

    return UNITY_END();
}
