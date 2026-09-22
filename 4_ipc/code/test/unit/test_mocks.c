#include "test_mocks.h"
#include <stdio.h>
#include <stdlib.h>

/* ====================================================================== */
/*  Assert capture (overrides the weak mimi_assert_failed in mimi.c)      */
/* ====================================================================== */

jmp_buf      assert_jmp_buf;
int          assert_jmp_armed;
int          assert_failed_cnt;
const char  *assert_expr;

void mimi_assert_failed(const char *x, const char *file, uint32_t line)
{
    assert_failed_cnt++;
    assert_expr = x;

    if (assert_jmp_armed) {
        longjmp(assert_jmp_buf, 1);
    }

    /* 没人承接: 这是测试代码自己的 bug, 不能让进程卡死在 while(1) 里 */
    fprintf(stderr, "\n[unexpected assert] (%s) at %s:%u\n", x, file, line);
    fflush(stderr);
    abort();
}

/* ====================================================================== */
/*  Clock                                                                 */
/* ====================================================================== */

uint32_t fake_tick;

uint32_t mimi_sys_tick(void)    { return fake_tick; }

/* ====================================================================== */
/*  IRQ stubs  (single-thread test, no real interrupts)                   */
/* ====================================================================== */

void mimi_disable_irq(void) { }
void mimi_enable_irq(void)  { }

/* ====================================================================== */
/*  Scheduler stubs                                                       */
/* ====================================================================== */

int schedule_called;

void mimi_schedule(void)          { schedule_called++; }
void mimi_isr_schedule_check(void)      { }
void mimi_sched_detach(mimi_tcb *t)  { (void)t; }
void mimi_sched_join(mimi_tcb *t, mimi_bool s) { (void)t; (void)s; }

/* ====================================================================== */
/*  Spinlock / critical section (x86 test: no real interrupts)            */
/* ====================================================================== */

uint32_t mimi_enter_critical(void)           { return 0; }
void     mimi_exit_critical(uint32_t level)  { (void)level; }

uint32_t mimi_spin_lock(mimi_spinlock *lock)   { (void)lock; return 0; }
void     mimi_spin_unlock(mimi_spinlock *lock, uint32_t level) { (void)lock; (void)level; }

/* ====================================================================== */
/*  Thread stubs — shared by mutex and mqueue tests                       */
/* ====================================================================== */

mimi_tcb  tA, tB, tC;
mimi_tcb *test_curr_thread;

static void test_thread_timer_handler(mimi_timer *timer) { (void)timer; }

void test_ipc_reset_all(void)
{
    memset(&tA, 0, sizeof(tA));
    tA.status = MIMI_THREAD_READY;
    tA.priority = 5;
    tA.origin_priority = 5;
    strncpy(tA.name, "tA", THREAD_NAME_LEN);

    memset(&tB, 0, sizeof(tB));
    tB.status = MIMI_THREAD_READY;
    tB.priority = 3;
    tB.origin_priority = 3;
    strncpy(tB.name, "tB", THREAD_NAME_LEN);

    memset(&tC, 0, sizeof(tC));
    tC.status = MIMI_THREAD_READY;
    tC.priority = 8;
    tC.origin_priority = 8;
    strncpy(tC.name, "tC", THREAD_NAME_LEN);

    test_curr_thread = &tA;
    fake_tick = 0;

    /* Timers must be brought back to STOP explicitly: a plain memset() would
     * leave status == 0, which is MIMI_TIMER_RUNNING, and mimi_timer_start()
     * then rejects the timer with MIMI_ERROR.
     */
    mimi_timer_init(&tA.timer, test_thread_timer_handler);
    mimi_timer_init(&tB.timer, test_thread_timer_handler);
    mimi_timer_init(&tC.timer, test_thread_timer_handler);

    /* tracking */
    sleep_called     = 0;
    wakeup_called    = 0;
    schedule_called  = 0;
    sleep_thread     = NULL;
    sleep_list       = NULL;
    prio_raise_cnt   = 0;
    prio_recover_cnt = 0;
    raised_to        = 0;
}

mimi_tcb *mimi_thread_current(void) { return test_curr_thread; }

int       sleep_called;
int       wakeup_called;
mimi_tcb *sleep_thread;
mimi_list *sleep_list;

int       prio_raise_cnt;
int       prio_recover_cnt;
uint8_t   raised_to;

mimi_err mimi_thread_suspend_to_list(mimi_tcb *thread, uint32_t timeout,
                                   mimi_list *list)
{
    /* keep in sync with kernel/src/thread.c */
    if (thread == NULL || list == NULL)
        return MIMI_EPARAMETER;
    if (thread->status == MIMI_THREAD_DEAD || thread->status == MIMI_THREAD_SUSPEND)
        return MIMI_ERESOURCE;
    sleep_called  = 1;
    sleep_thread  = thread;
    sleep_list    = list;
    thread->pending_list = list;
    thread->status = MIMI_THREAD_SUSPEND;
    mimi_list_push_back(list, &thread->node);
    if (timeout != MIMI_TIMEOUT_FOREVER)
        mimi_timer_start(&thread->timer, timeout, MIMI_TIMER_ONCE);
    return MIMI_EOK;
}

mimi_err mimi_thread_wakeup_from_ipc(mimi_tcb *thread)
{
    /* keep in sync with kernel/src/thread.c */
    if (thread == NULL)
        return MIMI_EPARAMETER;
    wakeup_called++;
    /* mirrors kernel/src/thread.c: the sleep timeout must be detached */
    mimi_timer_detach(&thread->timer);
    thread->pending_list = NULL;
    thread->status = MIMI_THREAD_READY;
    return MIMI_EOK;
}

void mimi_thread_prio_raise(mimi_tcb *t, uint8_t p)
{
    prio_raise_cnt++;
    raised_to = p;
    t->priority = p;
}

void mimi_thread_prio_recover(mimi_tcb *t)
{
    prio_recover_cnt++;
    t->priority = t->origin_priority;
}
