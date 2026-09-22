#include <string.h>
#include <unity.h>

#include "mimi.h"
#include "list.h"
#include "ipc.h"
#include "timer.h"
#include "test_mocks.h"

/* thread stubs (tA/tB/tC, reset_all, curr_thread) in test_mocks.c */

static void reset_all(void) { test_ipc_reset_all(); }

/* ====================================================================== */
/*  Group A: init                                                          */
/* ====================================================================== */

static void test_mutex_init_basic(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    TEST_ASSERT_EQUAL_UINT8(0, mtx.lock_cnt);
    TEST_ASSERT_NULL(mtx.thread);
    TEST_ASSERT_EQUAL_UINT8(THREAD_PRIORITY_MAX, mtx.highest_prio);
    TEST_ASSERT_TRUE(mimi_list_empty(&mtx.suspend_list));
}

/* ====================================================================== */
/*  Group B: lock / unlock state machine                                   */
/* ====================================================================== */

static void test_mutex_lock_free(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT));
    TEST_ASSERT_EQUAL_UINT8(1, mtx.lock_cnt);
    TEST_ASSERT_EQUAL_PTR(&tA, mtx.thread);
}

static void test_mutex_lock_null(void)
{
    TEST_EXPECT_ASSERT(mimi_mutex_lock(NULL, MIMI_TIMEOUT_NOWAIT));
}

static void test_mutex_lock_reentrant(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_UINT8(1, mtx.lock_cnt);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_UINT8(2, mtx.lock_cnt);

    mimi_mutex_unlock(&mtx);
}

static void test_mutex_lock_reentrant_count(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_UINT8(3, mtx.lock_cnt);
    TEST_ASSERT_EQUAL_PTR(&tA, mtx.thread);
}

static void test_mutex_lock_contended_nowait(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    test_curr_thread = &tB;

    TEST_ASSERT_EQUAL_INT(MIMI_ERESOURCE,
        mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT));
}

static void test_mutex_unlock_basic(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    mimi_mutex_unlock(&mtx);
    TEST_ASSERT_EQUAL_UINT8(0, mtx.lock_cnt);
    TEST_ASSERT_NULL(mtx.thread);
}

static void test_mutex_unlock_null(void)
{
    TEST_EXPECT_ASSERT(mimi_mutex_unlock(NULL));
}

/* 空指针入参现在由断言拦截 */
static void test_mutex_init_null(void)
{
    TEST_EXPECT_ASSERT(mimi_mutex_init(NULL));
}

/* 非持有者释放必须被拒绝: 否则任何线程都能把别人持有的锁拆掉 */
static void test_mutex_unlock_not_owner(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);   /* tA 持有 */
    test_curr_thread = &tB;

    TEST_ASSERT_EQUAL_INT(MIMI_ERROR, mimi_mutex_unlock(&mtx));
    TEST_ASSERT_EQUAL_UINT8(1, mtx.lock_cnt);     /* 锁没被拆 */
    TEST_ASSERT_EQUAL_PTR(&tA, mtx.thread);
}

/* 未上锁时释放同样要被拒绝 (thread == NULL 不能当成通配) */
static void test_mutex_unlock_unlocked(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    TEST_ASSERT_EQUAL_INT(MIMI_ERROR, mimi_mutex_unlock(&mtx));
    TEST_ASSERT_EQUAL_UINT8(0, mtx.lock_cnt);
}

static void test_mutex_lock_contended_forever(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    test_curr_thread = &tB;

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_INT(1, sleep_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_SUSPEND, tB.status);
    TEST_ASSERT_EQUAL_PTR(&mtx.suspend_list, sleep_list);
}

/* ====================================================================== */
/*  Group C: priority                                                      */
/* ====================================================================== */

static void test_mutex_unlock_no_waiters(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    wakeup_called = 0;
    mimi_mutex_unlock(&mtx);
    TEST_ASSERT_EQUAL_UINT8(0, mtx.lock_cnt);
    TEST_ASSERT_EQUAL_INT(0, wakeup_called);
}

static void test_mutex_lock_prio_raise(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    test_curr_thread = &tB;
    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_INT(0, prio_raise_cnt);
}

static void test_mutex_lock_highest_prio_tracking(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_UINT8(THREAD_PRIORITY_MAX, mtx.highest_prio);

    test_curr_thread = &tB;
    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_LESS_THAN_UINT8(THREAD_PRIORITY_MAX, mtx.highest_prio);
}

static void test_mutex_unlock_prio_recover(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    mimi_mutex_unlock(&mtx);
    TEST_ASSERT_EQUAL_INT(1, prio_recover_cnt);
}

/* ====================================================================== */
/*  Group D: timeout                                                       */
/* ====================================================================== */

static void test_mutex_lock_timeout(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    test_curr_thread = &tB;
    mimi_mutex_lock(&mtx, 100);

    TEST_ASSERT_EQUAL_INT(1, sleep_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_SUSPEND, tB.status);
}

static void test_mutex_lock_timeout_timer_fires(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    test_curr_thread = &tB;
    mimi_mutex_lock(&mtx, 100);

    fake_tick = 101;
    mimi_timer_check();
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_TIMEROUT, tB.timer.status);
}

/* Regression: see test_sem_wakeup_clears_timeout_timer — the waiter's sleep
 * timer must be detached when the mutex is handed over before the timeout.
 */
static void test_mutex_wakeup_clears_timeout_timer(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);   /* tA owns it */
    test_curr_thread = &tB;
    mimi_mutex_lock(&mtx, 100);                    /* tB waits with timeout */
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, tB.timer.status);

    /* The mock scheduler is synchronous: mimi_mutex_lock() returned with tB
     * already recorded as the owner, so mimi_mutex_unlock() from tA would bail
     * out with MIMI_ERROR before reaching the wake-up. Drive the hand-over the
     * way the unlock path does it.
     */
    mimi_thread_wakeup_from_ipc(&tB);

    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, tB.timer.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list));

    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_timer_start(&tB.timer, 50, MIMI_TIMER_ONCE));
}

/* ====================================================================== */
/*  Test runner                                                            */
/* ====================================================================== */

int run_mutex_tests(void)
{
    UNITY_BEGIN();

    reset_all(); RUN_TEST(test_mutex_init_basic);

    reset_all(); RUN_TEST(test_mutex_lock_free);
    reset_all(); RUN_TEST(test_mutex_lock_null);
    reset_all(); RUN_TEST(test_mutex_lock_reentrant);
    reset_all(); RUN_TEST(test_mutex_lock_reentrant_count);
    reset_all(); RUN_TEST(test_mutex_lock_contended_nowait);
    reset_all(); RUN_TEST(test_mutex_unlock_basic);
    reset_all(); RUN_TEST(test_mutex_unlock_null);
    reset_all(); RUN_TEST(test_mutex_init_null);
    reset_all(); RUN_TEST(test_mutex_unlock_not_owner);
    reset_all(); RUN_TEST(test_mutex_unlock_unlocked);
    reset_all(); RUN_TEST(test_mutex_lock_contended_forever);

    reset_all(); RUN_TEST(test_mutex_unlock_no_waiters);
    reset_all(); RUN_TEST(test_mutex_lock_prio_raise);
    reset_all(); RUN_TEST(test_mutex_lock_highest_prio_tracking);
    reset_all(); RUN_TEST(test_mutex_unlock_prio_recover);

    reset_all(); RUN_TEST(test_mutex_lock_timeout);
    reset_all(); RUN_TEST(test_mutex_lock_timeout_timer_fires);
    reset_all(); RUN_TEST(test_mutex_wakeup_clears_timeout_timer);

    return UNITY_END();
}
