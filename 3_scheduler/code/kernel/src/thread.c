
#include "thread.h"
#include <string.h>
#include "cpuport.h"
#include "list.h"
#include "log.h"
#include "sched.h"

void mimi_thread_timeout_handler_default(mimi_timer *timer)
{
    mimi_thread_wakeup(container_of_tcb_by_timer(timer));
}

void mimi_thread_exit_func(void)
{
    mimi_thread_kill(mimi_thread_current());
}

mimi_err mimi_thread_init(mimi_tcb   *thread,
                            const char  *name,
                            uint8_t     priority,
                            uint32_t    tick_slice,
                            void        *stack,
                            uint32_t    stack_size,
                            void        (*entry)(void* param),
                            void        *param,
                            void        (*exit_func)(void))
{
    mimi_assert(thread != NULL);
    mimi_assert(stack != NULL);
    mimi_assert(entry != NULL);
    mimi_assert(stack_size > 0);
    mimi_assert(priority < THREAD_PRIORITY_MAX);

    void *exit = exit_func ? exit_func : mimi_thread_exit_func;
    thread->status = MIMI_THREAD_READY;
    thread->priority = priority;
    thread->tick_slice = tick_slice ? tick_slice : THREAD_TIME_SLICE_DEFAULT;
    thread->remaining_tick = thread->tick_slice;
    thread->stack_size = stack_size;
    thread->sp = mimi_stack_init(entry, param, exit, stack, stack_size);
    thread->entry = entry;
    thread->param = param;
    if (name == NULL) {
        name = "anonymity";
    }
    memset(thread->name, 0, THREAD_NAME_LEN);
    strncpy(thread->name, name, mimi_min(THREAD_NAME_LEN - 1, strlen(name)));
    mimi_timer_init(&thread->timer, mimi_thread_timeout_handler_default);
    mimi_sched_join(thread, MIMI_FALSE);

    return MIMI_EOK;
}

void mimi_thread_yield(void)
{
    mimi_schedule();
}

mimi_err mimi_thread_delay(uint32_t delay)
{
    int ret = mimi_thread_suspend(mimi_thread_current(), delay);
    if (ret == MIMI_EOK) {
        mimi_schedule();
    }
    return ret;
}

mimi_err mimi_thread_suspend(mimi_tcb *thread, uint32_t timeout)
{
    mimi_assert(thread != NULL);

    uint32_t level = mimi_enter_critical();

    if (unlikely(thread->status != MIMI_THREAD_READY)) {
        mimi_exit_critical(level);
        return MIMI_ERROR;
    }

    thread->status = MIMI_THREAD_SUSPEND;
    if (timeout != MIMI_TIMEOUT_FOREVER) {
        mimi_timer_start(&thread->timer, timeout, MIMI_TIMER_ONCE);
    }
    mimi_sched_detach(thread);
    mimi_exit_critical(level);

    MIMI_LOG_V("%s sleep %ld ms\n", thread->name, timeout);
    return MIMI_EOK;
}

mimi_err mimi_thread_wakeup(mimi_tcb *thread)
{
    mimi_assert(thread != NULL);

    uint32_t level = mimi_enter_critical();

    if (unlikely(thread->status != MIMI_THREAD_SUSPEND)) {
        mimi_exit_critical(level);
        return MIMI_EOK;
    }

    thread->status = MIMI_THREAD_READY;
    mimi_timer_detach(&thread->timer);

    mimi_exit_critical(level);
    mimi_sched_join(thread, MIMI_TRUE);

    MIMI_LOG_V("%s wakeup\n", thread->name);
    return MIMI_EOK;
}

mimi_err mimi_thread_kill(mimi_tcb *thread)
{
    mimi_assert(thread != NULL);

    uint32_t level = mimi_enter_critical();
    mimi_bool sched_flag = MIMI_FALSE;

    if (thread->status == MIMI_THREAD_READY) {
        mimi_sched_detach(thread);
        if (thread == mimi_thread_current()) {
            sched_flag = MIMI_TRUE;
        }
    }
    thread->status = MIMI_THREAD_DEAD;
    mimi_timer_detach(&thread->timer);

    mimi_exit_critical(level);

    MIMI_LOG_D("%s is dead\n", thread->name);

    if (sched_flag) {
        mimi_schedule();
    }
    return MIMI_EOK;
}
