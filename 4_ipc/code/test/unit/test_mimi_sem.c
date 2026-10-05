#include <string.h>
#include <unity.h>

#include "mimi.h"
#include "list.h"
#include "ipc.h"
#include "timer.h"
#include "test_mocks.h"

/* thread stubs (tA/tB/tC, reset_all, curr_thread) in test_mocks.c */

/* ---------------------------------------------------------------------- */
/*  Helpers                                                               */
/* ---------------------------------------------------------------------- */

static void reset_all(void)
{
    test_ipc_reset_all();
}

/* ====================================================================== */
/*  Group A: init  (2 tests)                                              */
/* ====================================================================== */

static void test_sem_init_basic(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 3);

    TEST_ASSERT_EQUAL_UINT32(5, sem.max_cnt);
    TEST_ASSERT_EQUAL_UINT32(3, sem.cnt);
    TEST_ASSERT_TRUE(mimi_list_empty(&sem.suspend_list));
}

static void test_sem_init_zero(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 0);

    TEST_ASSERT_EQUAL_UINT32(0, sem.cnt);
}

/* ====================================================================== */
/*  Group B: basic counting  (5 tests)                                    */
/* ====================================================================== */

static void test_sem_take_positive(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 3);

    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_sem_take(&sem, MIMI_TIMEOUT_NOWAIT));
    TEST_ASSERT_EQUAL_UINT32(2, sem.cnt);
}

static void test_sem_take_to_zero(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 3);

    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_sem_take(&sem, MIMI_TIMEOUT_NOWAIT));
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_sem_take(&sem, MIMI_TIMEOUT_NOWAIT));
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_sem_take(&sem, MIMI_TIMEOUT_NOWAIT));
    TEST_ASSERT_EQUAL_UINT32(0, sem.cnt);
}

static void test_sem_release_increments(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 2);

    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_sem_release(&sem));
    TEST_ASSERT_EQUAL_UINT32(3, sem.cnt);
}

static void test_sem_take_release_roundtrip(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 3);

    mimi_sem_take(&sem, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_UINT32(2, sem.cnt);

    mimi_sem_release(&sem);
    TEST_ASSERT_EQUAL_UINT32(3, sem.cnt);
}

static void test_sem_release_at_max(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 2, 2);

    mimi_sem_release(&sem);
    TEST_ASSERT_EQUAL_UINT32(2, sem.cnt);
}

/* ====================================================================== */
/*  Group C: blocking & timeout  (5 tests)                                */
/* ====================================================================== */

static void test_sem_take_nowait_when_zero(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 0);

    TEST_ASSERT_EQUAL_INT(MIMI_ERESOURCE,
        mimi_sem_take(&sem, MIMI_TIMEOUT_NOWAIT));
    TEST_ASSERT_EQUAL_UINT32(0, sem.cnt);
    TEST_ASSERT_EQUAL_INT(0, sleep_called);
}

/* 唤醒原因不再看 timer.status, 而是超时回调写进 thread->error。
 * 到期后线程必须同时满足: error == ETIMEOUT、回到 READY、从等待队列摘掉、
 * 定时器归位。少任何一条, IPC 就会要么永久睡眠要么误判超时。
 */
static void test_sem_take_timeout_wakes_with_error(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 0);

    mimi_sem_take(&sem, 100);
    TEST_ASSERT_EQUAL_INT(1, sleep_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_SUSPEND, tA.status);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, tA.timer.status);

    fake_tick = 101;
    mimi_timer_check();

    TEST_ASSERT_EQUAL_INT(MIMI_ETIMEOUT, tA.error);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_READY, tA.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&sem.suspend_list));
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, tA.timer.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list));
}

/* Regression: a thread woken before its timeout must have the sleep timer
 * detached, otherwise the timer stays RUNNING and the next blocking call
 * cannot arm a new timeout (mimi_timer_start() rejects RUNNING timers).
 */
static void test_sem_wakeup_clears_timeout_timer(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 0);

    mimi_sem_take(&sem, 100);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, tA.timer.status);

    test_curr_thread = &tB;
    mimi_sem_release(&sem);                 /* wake tA, timeout not reached */

    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, tA.timer.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list));

    /* 被 release 唤醒不算失败: error 必须是 EOK, 不能留着上一次的 ETIMEOUT */
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, tA.error);

    /* the timer can be armed again */
    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_timer_start(&tA.timer, 50, MIMI_TIMER_ONCE));
}

static void test_sem_take_forever_sets_up_block(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 0);

    mimi_sem_take(&sem, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_INT(1, sleep_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_SUSPEND, tA.status);
    TEST_ASSERT_EQUAL_PTR(&sem.suspend_list, sleep_list);
}

static void test_sem_release_wakes_waiter(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 0);

    mimi_sem_take(&sem, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_INT(1, sleep_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_SUSPEND, tA.status);

    test_curr_thread = &tB;
    mimi_sem_release(&sem);

    TEST_ASSERT_EQUAL_INT(1, wakeup_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_READY, tA.status);
    /* cnt not checked: sync mock model cannot properly simulate
     * the blocking resume path where release increments count. */
}

static void test_sem_release_no_waiters(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 3);

    wakeup_called = 0;
    mimi_sem_release(&sem);

    TEST_ASSERT_EQUAL_UINT32(4, sem.cnt);
    TEST_ASSERT_EQUAL_INT(0, wakeup_called);
}

/* ====================================================================== */
/*  Group D: edge cases  (4 tests)                                        */
/* ====================================================================== */

static void test_sem_take_null(void)
{
    TEST_EXPECT_ASSERT(mimi_sem_take(NULL, MIMI_TIMEOUT_NOWAIT));
}

/* 空指针入参现在由断言拦截 */
static void test_sem_null_guards(void)
{
    TEST_EXPECT_ASSERT(mimi_sem_init(NULL, 1, 0));
    TEST_EXPECT_ASSERT(mimi_sem_release(NULL));
}

/* 回归: 超时过一次之后再阻塞, error 必须复位。
 * 以前唤醒原因看 timer.status, 到期后没人清, 第二次 FOREVER 阻塞会被误判成
 * 又超时了一次(线程其实是被正常唤醒的)。
 */
static void test_sem_error_reset_on_next_block(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 5, 0);

    mimi_sem_take(&sem, 100);
    fake_tick = 101;
    mimi_timer_check();
    TEST_ASSERT_EQUAL_INT(MIMI_ETIMEOUT, tA.error);

    /* 再等一次: 这次不带超时, error 必须回到 EOK */
    mimi_sem_take(&sem, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, tA.error);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_SUSPEND, tA.status);
}

/* 等待队列按优先级排序: tA(5) 先等, 但 tB(3) 优先级更高, release 要先给 tB。
 * 如果退化成 FIFO, 高优先级线程会被排在后面的低优先级线程挡住。
 */
static void test_sem_waiters_ordered_by_priority(void)
{
    mimi_sem sem;
    mimi_sem_init(&sem, 1, 0);

    mimi_sem_take(&sem, MIMI_TIMEOUT_FOREVER);      /* tA(5) 先等 */
    test_curr_thread = &tB;
    mimi_sem_take(&sem, MIMI_TIMEOUT_FOREVER);      /* tB(3) 后等, 但优先级高 */

    test_curr_thread = &tC;
    wakeup_called = 0;

    mimi_sem_release(&sem);
    TEST_ASSERT_EQUAL_INT(1, wakeup_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_READY, tB.status);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_SUSPEND, tA.status);
}

/* 二值信号量(max_cnt = 1): 拿空之后再拿必须失败 */
static void test_sem_binary_sem(void)
{
    mimi_sem bin;
    mimi_sem_init(&bin, 1, 1);

    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_sem_take(&bin, MIMI_TIMEOUT_NOWAIT));
    TEST_ASSERT_EQUAL_UINT32(0, bin.cnt);

    TEST_ASSERT_EQUAL_INT(MIMI_ERESOURCE,
        mimi_sem_take(&bin, MIMI_TIMEOUT_NOWAIT));
    TEST_ASSERT_EQUAL_UINT32(0, bin.cnt);
}

/* ====================================================================== */
/*  Test runner                                                            */
/* ====================================================================== */

int run_sem_tests(void)
{
    UNITY_BEGIN();

    reset_all(); RUN_TEST(test_sem_init_basic);
    reset_all(); RUN_TEST(test_sem_init_zero);

    reset_all(); RUN_TEST(test_sem_take_positive);
    reset_all(); RUN_TEST(test_sem_take_to_zero);
    reset_all(); RUN_TEST(test_sem_release_increments);
    reset_all(); RUN_TEST(test_sem_take_release_roundtrip);
    reset_all(); RUN_TEST(test_sem_release_at_max);

    reset_all(); RUN_TEST(test_sem_take_nowait_when_zero);
    reset_all(); RUN_TEST(test_sem_take_timeout_wakes_with_error);
    reset_all(); RUN_TEST(test_sem_error_reset_on_next_block);
    reset_all(); RUN_TEST(test_sem_wakeup_clears_timeout_timer);
    reset_all(); RUN_TEST(test_sem_take_forever_sets_up_block);
    reset_all(); RUN_TEST(test_sem_release_wakes_waiter);
    reset_all(); RUN_TEST(test_sem_release_no_waiters);

    reset_all(); RUN_TEST(test_sem_take_null);
    reset_all(); RUN_TEST(test_sem_null_guards);
    reset_all(); RUN_TEST(test_sem_waiters_ordered_by_priority);
    reset_all(); RUN_TEST(test_sem_binary_sem);

    return UNITY_END();
}
