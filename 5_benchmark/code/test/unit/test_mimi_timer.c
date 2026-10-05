#include <unity.h>

#include "mimi.h"
#include "list.h"
#include "timer.h"
#include "test_mocks.h"

extern mimi_list mimi_timer_list;

/* ---------------------------------------------------------------------- */
/*  Helper: timer callback tracking                                       */
/* ---------------------------------------------------------------------- */

static int fire_count;

static void fire_handler(mimi_timer *t)
{
    (void)t;
    fire_count++;
}

/* ====================================================================== */
/*  timer_init                                                            */
/* ====================================================================== */

static void test_timer_init_basic(void)
{
    mimi_timer t;
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_timer_init(&t, fire_handler));

    TEST_ASSERT_EQUAL_UINT32(0, t.timeout_tick);
    TEST_ASSERT_EQUAL_UINT32(0, t.timeout);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, t.status);
    TEST_ASSERT_EQUAL_PTR(fire_handler, t.handler);
}

/* ====================================================================== */
/*  timer_start                                                           */
/* ====================================================================== */

static void test_timer_start_once(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);

    TEST_ASSERT_EQUAL_UINT32(100, t.timeout_tick);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, t.status);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_ONCE, t.flag);
    /* must be in the timer list */
    TEST_ASSERT_EQUAL_PTR(&t.node, mimi_list_head(&mimi_timer_list));
}

static void test_timer_start_periodic(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_PERIODIC);

    TEST_ASSERT_EQUAL_UINT32(100, t.timeout_tick);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_PERIODIC, t.flag);
}

/* NULL handler 现在被 init 拒绝: 提前返回且不初始化结构体,
 * 所以不能再拿未初始化的定时器去 start(会把栈上的垃圾 node 塞进链表)。
 */
static void test_timer_init_reject_null_handler(void)
{
    mimi_timer t;
    TEST_EXPECT_ASSERT(mimi_timer_init(&t, NULL));
}

static void test_timer_start_twice(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);

    /* 重复 start 一个已在运行的定时器：返回错误，超时不更新 */
    TEST_ASSERT_EQUAL_INT(MIMI_ERROR,
        mimi_timer_start(&t, 200, MIMI_TIMER_ONCE));

    TEST_ASSERT_EQUAL_UINT32(100, t.timeout_tick);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, t.status);
}

static void test_timer_start_running_rejected(void)
{
    mimi_timer t1, t2;
    mimi_timer_init(&t1, fire_handler);
    mimi_timer_init(&t2, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t1, 100, MIMI_TIMER_ONCE);
    mimi_timer_start(&t2, 300, MIMI_TIMER_ONCE);

    /* 重复 start 运行中的 t1：返回错误，链表顺序不变 */
    TEST_ASSERT_EQUAL_INT(MIMI_ERROR,
        mimi_timer_start(&t1, 500, MIMI_TIMER_ONCE));

    mimi_timer *head = container_of_timer(mimi_list_head(&mimi_timer_list));
    TEST_ASSERT_EQUAL_UINT32(100, head->timeout_tick);  /* t1 still head */

    mimi_timer *next = container_of_timer(head->node.next);
    TEST_ASSERT_EQUAL_UINT32(300, next->timeout_tick);  /* t2 follows */
}

/* ====================================================================== */
/*  timer_stop                                                            */
/* ====================================================================== */

static void test_timer_stop_running(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 50;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);
    fake_tick = 80;
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_timer_stop(&t));

    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, t.status);
    TEST_ASSERT_EQUAL_UINT32(80, t.stop_tick);
    /* must be removed from list */
    TEST_ASSERT_NULL(mimi_list_head(&mimi_timer_list));
}

static void test_timer_stop_already_stopped(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    TEST_ASSERT_EQUAL_INT(MIMI_ERROR, mimi_timer_stop(&t));
}

/* 一次性定时器到期后 status 变 STOP 并摘出链表, 不再有 TIMEROUT 这个中间态。
 * 唤醒原因改由 thread->error 记录(见 test_mocks.c 的超时回调)。
 */
static void test_timer_stop_after_timeout(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);
    fake_tick = 100;
    mimi_timer_check();            /* status → MIMI_TIMER_STOP */

    TEST_ASSERT_EQUAL_INT(MIMI_ERROR, mimi_timer_stop(&t));
}

/* ====================================================================== */
/*  timer_resume                                                          */
/* ====================================================================== */

static void test_timer_resume_no_elapsed(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);
    fake_tick = 30;
    mimi_timer_stop(&t);

    /* resume immediately at same tick — no time elapsed since stop */
    mimi_timer_resume(&t);

    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, t.status);
    /* timeout_tick stays 100 — 0 elapsed ticks added */
    TEST_ASSERT_EQUAL_UINT32(100, t.timeout_tick);
}

static void test_timer_resume_with_elapsed(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);
    fake_tick = 30;
    mimi_timer_stop(&t);

    /* 70 ticks passed while stopped */
    fake_tick = 100;
    mimi_timer_resume(&t);

    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, t.status);
    /* 100 + (100 - 30) elapsed = 170 */
    TEST_ASSERT_EQUAL_UINT32(170, t.timeout_tick);
}

static void test_timer_resume_running(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);

    TEST_ASSERT_EQUAL_INT(MIMI_ERROR, mimi_timer_resume(&t));
}

/* 到期过的一次性定时器 status 是 STOP, 和"手动 stop 过"共用同一个状态,
 * 所以 mimi_timer_resume() 会把它重新挂起来 —— 这是设计选择, 不是缺陷:
 * 一次性/周期只由 flag 区分, status 只表示"在不在链表上"。
 */
static void test_timer_resume_after_timeout(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);
    fake_tick = 100;
    mimi_timer_check();

    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, t.status);
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_timer_resume(&t));
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, t.status);
}

/* ====================================================================== */
/*  timer_join                                                            */
/* ====================================================================== */

static void test_timer_join_empty_list(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);
    t.timeout_tick = 100;

    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_timer_join(&t));

    TEST_ASSERT_EQUAL_PTR(&t.node, mimi_list_head(&mimi_timer_list));
    TEST_ASSERT_EQUAL_PTR(&t.node, t.node.next);     /* circular */
    TEST_ASSERT_EQUAL_PTR(&t.node, t.node.prev);
}

static void test_timer_join_single_existing_head(void)
{
    mimi_timer t1, t2;
    mimi_timer_init(&t1, fire_handler);
    mimi_timer_init(&t2, fire_handler);

    /* t1=200 existing, t2=100 earlier → t2 is head */
    t1.timeout_tick = 200;
    mimi_timer_join(&t1);
    t2.timeout_tick = 100;
    mimi_timer_join(&t2);

    TEST_ASSERT_EQUAL_PTR(&t2.node, mimi_list_head(&mimi_timer_list));   /* earliest */
    TEST_ASSERT_EQUAL_PTR(&t1.node, t2.node.next);
}

static void test_timer_join_single_existing_tail(void)
{
    mimi_timer t1, t2;
    mimi_timer_init(&t1, fire_handler);
    mimi_timer_init(&t2, fire_handler);

    t1.timeout_tick = 100;
    mimi_timer_join(&t1);
    t2.timeout_tick = 200;
    mimi_timer_join(&t2);

    TEST_ASSERT_EQUAL_PTR(&t1.node, mimi_list_head(&mimi_timer_list));   /* earliest */
    TEST_ASSERT_EQUAL_PTR(&t2.node, t1.node.next);
}

static void test_timer_join_as_earliest(void)
{
    mimi_timer t[4];
    for (int i = 0; i < 4; i++)
        mimi_timer_init(&t[i], fire_handler);

    t[0].timeout_tick = 100; mimi_timer_join(&t[0]);
    t[1].timeout_tick = 200; mimi_timer_join(&t[1]);
    t[2].timeout_tick = 300; mimi_timer_join(&t[2]);
    t[3].timeout_tick = 50;  mimi_timer_join(&t[3]);  /* earliest */

    TEST_ASSERT_EQUAL_PTR(&t[3].node, mimi_list_head(&mimi_timer_list));
}

static void test_timer_join_as_latest(void)
{
    mimi_timer t[4];
    for (int i = 0; i < 4; i++)
        mimi_timer_init(&t[i], fire_handler);

    t[0].timeout_tick = 100; mimi_timer_join(&t[0]);
    t[1].timeout_tick = 200; mimi_timer_join(&t[1]);
    t[2].timeout_tick = 300; mimi_timer_join(&t[2]);
    t[3].timeout_tick = 500; mimi_timer_join(&t[3]);  /* latest */

    /* walk to the tail */
    mimi_node *n = mimi_list_head(&mimi_timer_list);
    while (n->next != mimi_list_head(&mimi_timer_list))
        n = n->next;
    TEST_ASSERT_EQUAL_PTR(&t[3].node, n);
}

static void test_timer_join_in_middle(void)
{
    mimi_timer t[4];
    for (int i = 0; i < 4; i++)
        mimi_timer_init(&t[i], fire_handler);

    t[0].timeout_tick = 100; mimi_timer_join(&t[0]);
    t[1].timeout_tick = 300; mimi_timer_join(&t[1]);
    t[2].timeout_tick = 200; mimi_timer_join(&t[2]);  /* middle */

    /* order should be: 100 → 200 → 300 */
    mimi_node *h = mimi_list_head(&mimi_timer_list);
    mimi_timer *tm = container_of_timer(h);
    TEST_ASSERT_EQUAL_UINT32(100, tm->timeout_tick);
    tm = container_of_timer(h->next);
    TEST_ASSERT_EQUAL_UINT32(200, tm->timeout_tick);
    tm = container_of_timer(h->next->next);
    TEST_ASSERT_EQUAL_UINT32(300, tm->timeout_tick);
}

static void test_timer_join_same_timeout(void)
{
    mimi_timer t1, t2;
    mimi_timer_init(&t1, fire_handler);
    mimi_timer_init(&t2, fire_handler);

    t1.timeout_tick = 100;
    mimi_timer_join(&t1);
    t2.timeout_tick = 100;
    mimi_timer_join(&t2);

    /* t1 remains head, t2 follows */
    TEST_ASSERT_EQUAL_PTR(&t1.node, mimi_list_head(&mimi_timer_list));
    TEST_ASSERT_EQUAL_PTR(&t2.node, t1.node.next);
    /* both at same tick */
    mimi_timer *tm = container_of_timer(mimi_list_head(&mimi_timer_list));
    TEST_ASSERT_EQUAL_UINT32(100, tm->timeout_tick);
}

/* ====================================================================== */
/*  timer_detach                                                          */
/* ====================================================================== */

/* detach 是线程被提前唤醒时清理超时定时器的入口: 必须真的从链表摘掉,
 * 否则链表里留着一个已经不再被引用的栈上定时器。
 */
static void test_timer_detach_running(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);
    TEST_ASSERT_FALSE(mimi_list_empty(&mimi_timer_list));

    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_timer_detach(&t));
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, t.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list));
    TEST_ASSERT_TRUE(mimi_node_isolated(&t.node));

    /* 摘掉之后定时器还能重新挂上 */
    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_timer_start(&t, 50, MIMI_TIMER_ONCE));
}

/* 对已经停止的定时器 detach 是幂等的: 不能改状态, 更不能动链表 */
static void test_timer_detach_stopped(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_timer_detach(&t));
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, t.status);
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list));
}

/* ====================================================================== */
/*  timer_check                                                           */
/* ====================================================================== */

static void test_timer_check_empty(void)
{
    /* should not crash */
    mimi_timer_check();
}

static void test_timer_check_not_expired(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);

    fire_count = 0;
    fake_tick = 50;
    mimi_timer_check();

    TEST_ASSERT_EQUAL_INT(0, fire_count);           /* not fired */
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, t.status);  /* still running */
}

static void test_timer_check_exact_expired(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);

    fire_count = 0;
    fake_tick = 100;
    mimi_timer_check();

    TEST_ASSERT_EQUAL_INT(1, fire_count);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, t.status);
    /* 一次性定时器到期后必须自己摘出链表 */
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list));
}

/* 一次性定时器只能响一次: 到期摘链后, 后续 check 不能再触发 */
static void test_timer_check_once_no_refire(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);

    fire_count = 0;
    fake_tick = 100;
    mimi_timer_check();
    TEST_ASSERT_EQUAL_INT(1, fire_count);

    fake_tick = 300;
    mimi_timer_check();
    TEST_ASSERT_EQUAL_INT(1, fire_count);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, t.status);
}

static void test_timer_check_past_expired(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_ONCE);

    fire_count = 0;
    fake_tick = 150;              /* past deadline */
    mimi_timer_check();

    TEST_ASSERT_EQUAL_INT(1, fire_count);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_STOP, t.status);
}

static void test_timer_check_multi_partial(void)
{
    mimi_timer t1, t2;
    mimi_timer_init(&t1, fire_handler);
    mimi_timer_init(&t2, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t1, 100, MIMI_TIMER_ONCE);
    mimi_timer_start(&t2, 300, MIMI_TIMER_ONCE);

    fire_count = 0;
    fake_tick = 200;
    mimi_timer_check();

    TEST_ASSERT_EQUAL_INT(1, fire_count);    /* only t1 */
    TEST_ASSERT_FALSE(mimi_node_isolated(&t2.node)); /* t2 still on list */
}

static void test_timer_check_multi_all(void)
{
    mimi_timer t1, t2, t3;
    mimi_timer_init(&t1, fire_handler);
    mimi_timer_init(&t2, fire_handler);
    mimi_timer_init(&t3, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t1, 100, MIMI_TIMER_ONCE);
    mimi_timer_start(&t2, 200, MIMI_TIMER_ONCE);
    mimi_timer_start(&t3, 300, MIMI_TIMER_ONCE);

    fire_count = 0;
    fake_tick = 400;
    mimi_timer_check();

    TEST_ASSERT_EQUAL_INT(3, fire_count);    /* all three */
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list)); /* all popped */
}

static void test_timer_check_periodic_refire(void)
{
    mimi_timer t;
    mimi_timer_init(&t, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&t, 100, MIMI_TIMER_PERIODIC);

    fire_count = 0;
    fake_tick = 100;
    mimi_timer_check();
    TEST_ASSERT_EQUAL_INT(1, fire_count);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, t.status);  /* re-joined */

    fake_tick = 200;
    mimi_timer_check();
    TEST_ASSERT_EQUAL_INT(2, fire_count);
    TEST_ASSERT_EQUAL_INT(MIMI_TIMER_RUNNING, t.status);
}

static void test_timer_check_periodic_keeps_order(void)
{
    mimi_timer tp, to;
    mimi_timer_init(&tp, fire_handler);
    mimi_timer_init(&to, fire_handler);

    fake_tick = 0;
    mimi_timer_start(&tp, 100, MIMI_TIMER_PERIODIC);   /* refires every 100 */
    mimi_timer_start(&to, 250, MIMI_TIMER_ONCE);       /* fires once */

    /* after 100: tp fires, re-joins at 200 — should still be before to(250) */
    fire_count = 0;
    fake_tick = 100;
    mimi_timer_check();
    TEST_ASSERT_EQUAL_INT(1, fire_count);            /* tp fired */

    /* tp re-joined at 200, to is at 250 — tp should be head */
    mimi_timer *head = container_of_timer(mimi_list_head(&mimi_timer_list));
    TEST_ASSERT_EQUAL_UINT32(200, head->timeout_tick);
}

/* ====================================================================== */
/*  NULL guards                                                           */
/* ====================================================================== */

static void test_timer_null_guards(void)
{
    mimi_timer t;

    /* 空指针入参现在由断言拦截 */
    TEST_EXPECT_ASSERT(mimi_timer_init(NULL, fire_handler));
    TEST_EXPECT_ASSERT(mimi_timer_start(NULL, 100, MIMI_TIMER_ONCE));
    TEST_EXPECT_ASSERT(mimi_timer_stop(NULL));
    TEST_EXPECT_ASSERT(mimi_timer_resume(NULL));
    TEST_EXPECT_ASSERT(mimi_timer_join(NULL));
    TEST_EXPECT_ASSERT(mimi_timer_detach(NULL));

    /* 上面每次都被断言拦下, 定时器链表不该被塞进任何东西 */
    TEST_ASSERT_TRUE(mimi_list_empty(&mimi_timer_list));

    /* 正常路径不受影响 */
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_timer_init(&t, fire_handler));
}

/* ====================================================================== */
/*  Test runner                                                           */
/* ====================================================================== */

int run_timer_tests(void)
{
    UNITY_BEGIN();

    /* --- init --- */
    RUN_TEST(test_timer_init_basic);

    /* --- start --- */
    RUN_TEST(test_timer_start_once);
    RUN_TEST(test_timer_start_periodic);
    RUN_TEST(test_timer_init_reject_null_handler);
    RUN_TEST(test_timer_null_guards);
    RUN_TEST(test_timer_start_twice);
    RUN_TEST(test_timer_start_running_rejected);

    /* --- stop --- */
    RUN_TEST(test_timer_stop_running);
    RUN_TEST(test_timer_stop_already_stopped);
    RUN_TEST(test_timer_stop_after_timeout);

    /* --- resume --- */
    RUN_TEST(test_timer_resume_no_elapsed);
    RUN_TEST(test_timer_resume_with_elapsed);
    RUN_TEST(test_timer_resume_running);
    RUN_TEST(test_timer_resume_after_timeout);

    /* --- join --- */
    RUN_TEST(test_timer_join_empty_list);
    RUN_TEST(test_timer_join_single_existing_head);
    RUN_TEST(test_timer_join_single_existing_tail);
    RUN_TEST(test_timer_join_as_earliest);
    RUN_TEST(test_timer_join_as_latest);
    RUN_TEST(test_timer_join_in_middle);
    RUN_TEST(test_timer_join_same_timeout);

    /* --- detach --- */
    RUN_TEST(test_timer_detach_running);
    RUN_TEST(test_timer_detach_stopped);

    /* --- check --- */
    RUN_TEST(test_timer_check_empty);
    RUN_TEST(test_timer_check_not_expired);
    RUN_TEST(test_timer_check_exact_expired);
    RUN_TEST(test_timer_check_past_expired);
    RUN_TEST(test_timer_check_once_no_refire);
    RUN_TEST(test_timer_check_periodic_refire);

    RUN_TEST(test_timer_check_multi_partial);
    RUN_TEST(test_timer_check_multi_all);
    RUN_TEST(test_timer_check_periodic_keeps_order);

    return UNITY_END();
}
