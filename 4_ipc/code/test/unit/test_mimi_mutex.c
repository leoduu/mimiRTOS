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

    TEST_ASSERT_EQUAL_UINT8(0, mtx.lock_nest);
    TEST_ASSERT_NULL(mtx.owner);
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
    TEST_ASSERT_EQUAL_UINT8(1, mtx.lock_nest);
    TEST_ASSERT_EQUAL_PTR(&tA, mtx.owner);
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
    TEST_ASSERT_EQUAL_UINT8(1, mtx.lock_nest);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_UINT8(2, mtx.lock_nest);

    mimi_mutex_unlock(&mtx);
}

static void test_mutex_lock_reentrant_count(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_UINT8(3, mtx.lock_nest);
    TEST_ASSERT_EQUAL_PTR(&tA, mtx.owner);
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
    TEST_ASSERT_EQUAL_UINT8(0, mtx.lock_nest);
    TEST_ASSERT_NULL(mtx.owner);
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
    TEST_ASSERT_EQUAL_UINT8(1, mtx.lock_nest);     /* 锁没被拆 */
    TEST_ASSERT_EQUAL_PTR(&tA, mtx.owner);
}

/* 未上锁时释放同样要被拒绝 (thread == NULL 不能当成通配) */
static void test_mutex_unlock_unlocked(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    TEST_ASSERT_EQUAL_INT(MIMI_ERROR, mimi_mutex_unlock(&mtx));
    TEST_ASSERT_EQUAL_UINT8(0, mtx.lock_nest);
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
    TEST_ASSERT_EQUAL_UINT8(0, mtx.lock_nest);
    TEST_ASSERT_EQUAL_INT(0, wakeup_called);
}

/* 解锁把锁交给等待者: 新 owner 必须是被唤醒的那个线程, 不是解锁的线程。
 * 记成解锁者的话, 等待者醒来后会以为自己拿到了锁, 而锁实际挂在别人名下。
 *
 * 同步 mock 能直接测这条: 带超时阻塞的 lock 在 mimi_thread_block() 之后就
 * 返回 curr->error 了, 不会自己把 owner 抢过去, 所以 mtx.owner 还停在 tA。
 */
static void test_mutex_unlock_hands_over_to_waiter(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);   /* tA 持有 */
    test_curr_thread = &tB;
    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);  /* tB 等待 */

    test_curr_thread = &tA;
    wakeup_called = 0;
    mimi_mutex_unlock(&mtx);

    TEST_ASSERT_EQUAL_INT(1, wakeup_called);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_READY, tB.status);
    TEST_ASSERT_EQUAL_UINT8(1, mtx.lock_nest);
    TEST_ASSERT_EQUAL_PTR(&tB, mtx.owner);
}

/* 优先级继承: tB(3) 等在 tA(5) 手里的锁上, 必须把持锁者 tA 提到 3,
 * 否则低优先级的 tA 迟迟得不到运行, 高优先级的 tB 就跟着一起饿。
 */
static void test_mutex_lock_prio_inheritance(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);   /* tA 持有 */
    test_curr_thread = &tB;
    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);  /* tB 等待 */

    TEST_ASSERT_EQUAL_INT(1, prio_raise_cnt);
    TEST_ASSERT_EQUAL_UINT8(3, raised_to);
    TEST_ASSERT_EQUAL_UINT8(3, tA.priority);
}

/* highest_prio 记的是"持锁者被抬到的优先级": 空锁时是 MAX,
 * 一旦有人拿锁就等于持锁者自己的优先级, 之后只会被更高优先级的等待者压低。
 */
static void test_mutex_lock_highest_prio_tracking(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    TEST_ASSERT_EQUAL_UINT8(5, mtx.highest_prio);          /* = tA.priority */

    test_curr_thread = &tB;                                /* tB(3) 更高 */
    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_UINT8(3, mtx.highest_prio);
}

/* 解锁且无等待者: 优先级要还回去, highest_prio 也要回到 MAX */
static void test_mutex_unlock_restores_prio_state(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    test_curr_thread = &tB;
    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_UINT8(3, tA.priority);               /* 已继承 */

    test_curr_thread = &tA;
    mimi_mutex_unlock(&mtx);

    TEST_ASSERT_EQUAL_UINT8(5, tA.priority);               /* 还回 5 */
    TEST_ASSERT_EQUAL_UINT8(1, mtx.lock_nest);             /* 交给了 tB */
}

static void test_mutex_unlock_prio_recover(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);
    mimi_mutex_unlock(&mtx);
    TEST_ASSERT_EQUAL_INT(1, prio_recover_cnt);
}

/* tA 同时持有 m1 和 m2, 两把锁各挂一个等待者:
 *  - m1 上的 tB(3) 先把 tA 抬到 3;
 *  - m2 上的 tC(4) 更低, 不能再把 tA 打回 4 (raise 只抬不降);
 *  - 释放 m2 时 tA 还拿着 m1, 继承必须留着, 等释放 m1 才还给 5。
 */
static void test_mutex_nested_inheritance(void)
{
    mimi_mutex m1, m2;
    mimi_mutex_init(&m1);
    mimi_mutex_init(&m2);

    mimi_mutex_lock(&m1, MIMI_TIMEOUT_NOWAIT);   /* tA(5) 拿 m1 */
    mimi_mutex_lock(&m2, MIMI_TIMEOUT_NOWAIT);   /* 同时拿 m2 */

    test_curr_thread = &tB;                      /* tB(3) 等 m1 */
    mimi_mutex_lock(&m1, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_UINT8(3, tA.priority);     /* 已继承 */

    tC.priority = 4;                             /* 夹在原始 5 和已继承的 3 之间 */
    tC.origin_priority = 4;
    test_curr_thread = &tC;
    mimi_mutex_lock(&m2, MIMI_TIMEOUT_FOREVER);
    TEST_ASSERT_EQUAL_UINT8(3, tA.priority);     /* 不能被打回 4 */

    test_curr_thread = &tA;
    mimi_mutex_unlock(&m2);                      /* 放掉 m2, m1 上还挂着 tB */
    TEST_ASSERT_EQUAL_UINT8(3, tA.priority);     /* 继承必须留着 */

    mimi_mutex_unlock(&m1);                      /* 最后一把 */
    TEST_ASSERT_EQUAL_UINT8(5, tA.priority);     /* 这时才还回去 */
}

/* ====================================================================== */
/*  Group D: timeout                                                       */
/* ====================================================================== */

/* thread_block 失败时必须把错误码往外传。
 * 拿不到等待位却返回 EOK, 调用方会以为自己已经拿到锁。
 */
static void test_mutex_lock_block_failure_propagates(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_NOWAIT);   /* tA 持有 */
    test_curr_thread = &tB;
    tB.status = MIMI_THREAD_SUSPEND;              /* tB 已经挂在别的 IPC 上 */

    TEST_ASSERT_EQUAL_INT(MIMI_ERROR,
        mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER));
    TEST_ASSERT_EQUAL_PTR(&tA, mtx.owner);       /* 锁还在 tA 名下 */
}

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

/* 超时原因同样走 thread->error: 等待者必须带着 ETIMEOUT 醒过来并退出等待队列 */
static void test_mutex_lock_timeout_wakes_with_error(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
    test_curr_thread = &tB;
    mimi_mutex_lock(&mtx, 100);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, tB.timer.status);

    fake_tick = 101;
    mimi_timer_check();

    TEST_ASSERT_EQUAL_INT(MIMI_ETIMEOUT, tB.error);
    TEST_ASSERT_EQUAL_INT(MIMI_THREAD_READY, tB.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&mtx.suspend_list));
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, tB.timer.status);
}

/* Regression: 超时前被交接的等待者, 超时定时器必须归位, 否则下次阻塞起不了定时器 */
static void test_mutex_wakeup_clears_timeout_timer(void)
{
    mimi_mutex mtx;
    mimi_mutex_init(&mtx);

    mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);   /* tA owns it */
    test_curr_thread = &tB;
    mimi_mutex_lock(&mtx, 100);                    /* tB waits with timeout */
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, tB.timer.status);

    /* 超时没到就交接: unlock 会把锁交给 tB 并唤醒它 */
    test_curr_thread = &tA;
    mimi_mutex_unlock(&mtx);

    TEST_ASSERT_EQUAL_PTR(&tB, mtx.owner);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, tB.timer.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list));
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, tB.error);

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
    reset_all(); RUN_TEST(test_mutex_unlock_hands_over_to_waiter);
    reset_all(); RUN_TEST(test_mutex_lock_prio_inheritance);
    reset_all(); RUN_TEST(test_mutex_lock_highest_prio_tracking);
    reset_all(); RUN_TEST(test_mutex_unlock_restores_prio_state);
    reset_all(); RUN_TEST(test_mutex_unlock_prio_recover);
    reset_all(); RUN_TEST(test_mutex_nested_inheritance);

    reset_all(); RUN_TEST(test_mutex_lock_timeout);
    reset_all(); RUN_TEST(test_mutex_lock_timeout_wakes_with_error);
    reset_all(); RUN_TEST(test_mutex_lock_block_failure_propagates);
    reset_all(); RUN_TEST(test_mutex_wakeup_clears_timeout_timer);

    return UNITY_END();
}
