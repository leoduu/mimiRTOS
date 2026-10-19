#include "test_mocks.h"
#include <stdio.h>
#include <stdlib.h>
#include "mimi_port.h"

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
/*  Log stub (ipc.c 走 MIMI_LOG_E 打错误分支)                             */
/* ====================================================================== */

void mimi_log_printf(char level, const char *fmt, ...) { (void)level; (void)fmt; }

/* ====================================================================== */
/*  Scheduler stubs                                                       */
/* ====================================================================== */

int schedule_called;
int test_in_isr;            /* set to 1 to pretend the caller is in an ISR */
static uint32_t test_critical_nest;

void mimi_schedule(void)          { schedule_called++; }
void mimi_sched_detach(mimi_tcb *t)  { (void)t; }
void mimi_sched_join(mimi_tcb *t)    { (void)t; }

/* ====================================================================== */
/*  Spinlock / critical section (x86 test: no real interrupts)            */
/* ====================================================================== */

uint32_t mimi_enter_critical(void)
{
    uint32_t level = test_critical_nest;

    test_critical_nest++;
    return level;
}

void mimi_exit_critical(uint32_t level)
{
    test_critical_nest = level;
}

mimi_bool mimi_is_in_isr(void)
{
    return test_in_isr ? MIMI_TRUE : MIMI_FALSE;
}

/* ====================================================================== */
/*  Thread stubs — shared by the IPC tests                                */
/* ====================================================================== */

mimi_tcb  tA, tB, tC;
mimi_tcb *mimi_curr_thread;

static void test_thread_timer_handler(mimi_timer *timer)
{
    mimi_tcb *thread = container_of_tcb_by_timer(timer);
    if (thread->status == MIMI_THREAD_SUSPEND) {
        thread->error = MIMI_ETIMEOUT;
        mimi_thread_resume(thread);
        mimi_schedule();
    }
}

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
    mimi_list_init(&tA.mtx_list);
    mimi_list_init(&tB.mtx_list);
    mimi_list_init(&tC.mtx_list);
    test_critical_nest = 0;
    test_in_isr = 0;

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

/* 镜像 thread.c 的 insert_suspend_list_by_prio: 插到第一个优先级比自己低的等待者前面 */
static void insert_suspend_list_by_prio(mimi_list *list, mimi_tcb *thread)
{
    mimi_node *node, *tmp;
    mimi_list_for_each_safe_start(list, node, tmp) {
        mimi_tcb *temp = container_of_tcb(node);
        if (temp->priority > thread->priority) {
            mimi_list_insert_front(list, &temp->node, &thread->node);
            break;
        }
    } mimi_list_for_each_safe_end(list, node, tmp);
    if (mimi_node_isolated(&thread->node))
        mimi_list_push_back(list, &thread->node);
}

mimi_err mimi_thread_block(mimi_tcb *thread, uint32_t timeout, mimi_list *list)
{
    mimi_assert(thread != NULL);
    mimi_assert(list != NULL);

    if (thread->status == MIMI_THREAD_SUSPEND)
        return MIMI_ERROR;

    sleep_called++;
    sleep_thread = thread;
    sleep_list   = list;

    thread->status = MIMI_THREAD_SUSPEND;
    thread->error = MIMI_EOK;
    thread->suspend_list = list;
    if (timeout != MIMI_TIMEOUT_FOREVER)
        mimi_timer_start(&thread->timer, timeout, MIMI_TIMER_ONCE);

    insert_suspend_list_by_prio(list, thread);

    return MIMI_EOK;
}

mimi_err mimi_thread_resume(mimi_tcb *thread)
{
    mimi_assert(thread != NULL);

    if (thread->status == MIMI_THREAD_READY)
        return MIMI_EOK;

    wakeup_called++;
    thread->status = MIMI_THREAD_READY;
    if (thread->suspend_list != NULL) {
        if (!mimi_node_isolated(&thread->node)) {
            mimi_list_remove(thread->suspend_list, &thread->node);
        }
        thread->suspend_list = NULL;
    }
    mimi_timer_detach(&thread->timer);

    return MIMI_EOK;
}

/* 与 thread.c 一致: 对 DEAD 线程不处理, 对 SUSPEND 线程重排等待队列,
 * 这样优先级继承测试能同时检查优先级和队列顺序。
 */
void mimi_thread_prio_raise(mimi_tcb *t, uint8_t p)
{
    if (t->status == MIMI_THREAD_DEAD)
        return;

    /* 只抬不降。owner 可能同时被好几把锁继承, 无条件赋值会把别的锁抬上去的
     * 优先级反手打回去。 */
    if (t->priority <= p)
        return;

    prio_raise_cnt++;
    raised_to = p;
    t->priority = p;

    if (t->status == MIMI_THREAD_SUSPEND && t->suspend_list != NULL) {
        mimi_list_remove(t->suspend_list, &t->node);
        insert_suspend_list_by_prio(t->suspend_list, t);
    }
}

void mimi_thread_prio_recover(mimi_tcb *t)
{
    if (t->status == MIMI_THREAD_DEAD)
        return;

    /* 延迟恢复: 只要还持有别的 mutex, 继承来的优先级就不能降,
     * 否则那把锁上的等待者会被饿着。*/
    if (!mimi_list_empty(&t->mtx_list))
        return;

    prio_recover_cnt++;
    t->priority = t->origin_priority;

    if (t->status == MIMI_THREAD_SUSPEND && t->suspend_list != NULL) {
        mimi_list_remove(t->suspend_list, &t->node);
        insert_suspend_list_by_prio(t->suspend_list, t);
    }
}
