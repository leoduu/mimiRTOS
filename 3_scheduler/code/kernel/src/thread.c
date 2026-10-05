
#include "thread.h"
#include <string.h>
#include <stdio.h>
#include "mimi_port.h"
#include "list.h"
#include "log.h"
#include "sched.h"

void mimi_thread_timeout_handler_default(mimi_timer *timer)
{
    mimi_tcb *thread = container_of_tcb_by_timer(timer);
    if (thread->status == MIMI_THREAD_SUSPEND) {
        thread->error = MIMI_ETIMEOUT;
        mimi_thread_resume(thread);
    }
}

void mimi_thread_exit_func(void)
{
    mimi_thread_kill(mimi_thread_current());
    // unreachable
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

    void *exit = exit_func ? exit_func : mimi_thread_exit_func;
    thread->error = MIMI_EOK;
    thread->status = MIMI_THREAD_READY;
    thread->priority = priority;
    thread->tick_slice = tick_slice ? tick_slice : THREAD_TIME_SLICE_DEFAULT;
    thread->remaining_tick = thread->tick_slice;
    thread->stack_size = stack_size;
    thread->sp = mimi_stack_init(entry, param, exit, stack, stack_size);
    thread->entry = entry;
    thread->param = param;
    if (name == NULL) {
        sprintf(thread->name, "0x%08X", (unsigned int)entry);
    } else {
        int len = mimi_min(THREAD_NAME_LEN - 1, strlen(name));
        memcpy(thread->name, name, len);
        thread->name[len] = '\0';
    }
    mimi_timer_init(&thread->timer, mimi_thread_timeout_handler_default);
    mimi_sched_join(thread);

    return MIMI_EOK;
}

void mimi_thread_yield(void)
{
    mimi_schedule_rr();
}

mimi_err mimi_thread_delay(uint32_t delay)
{
    return mimi_thread_suspend(mimi_thread_current(), delay);
}

mimi_err mimi_thread_suspend(mimi_tcb *thread, uint32_t timeout)
{
    mimi_assert(thread != NULL);

    uint32_t level = mimi_enter_critical();

    if (unlikely(thread->status == MIMI_THREAD_SUSPEND)) {
        mimi_exit_critical(level);
        return MIMI_ERROR;
    }

    MIMI_LOG_V("%s suspend %ld ms\n", thread->name, timeout);

    thread->status = MIMI_THREAD_SUSPEND;
    thread->error = MIMI_EOK;
    if (timeout != MIMI_TIMEOUT_FOREVER) {
        mimi_timer_start(&thread->timer, timeout, MIMI_TIMER_ONCE);
    }
    mimi_sched_detach(thread);

    if (thread == mimi_thread_current()) {
        mimi_schedule();
    }
    mimi_exit_critical(level);
    return MIMI_EOK;
}

mimi_err mimi_thread_resume(mimi_tcb *thread)
{
    mimi_assert(thread != NULL);

    uint32_t level = mimi_enter_critical();

    if (unlikely(thread->status == MIMI_THREAD_READY)) {
        mimi_exit_critical(level);
        return MIMI_EOK;
    }

    MIMI_LOG_V("%s resume\n", thread->name);

    thread->status = MIMI_THREAD_READY;
    mimi_timer_detach(&thread->timer);
    mimi_sched_join(thread);

    mimi_schedule();
    mimi_exit_critical(level);
    return MIMI_EOK;
}

mimi_err mimi_thread_kill(mimi_tcb *thread)
{
    mimi_assert(thread != NULL);

    uint32_t level = mimi_enter_critical();

    if (thread->status == MIMI_THREAD_READY) {
        mimi_sched_detach(thread);
    }
    thread->status = MIMI_THREAD_DEAD;
    mimi_timer_detach(&thread->timer);

    MIMI_LOG_D("%s is dead\n", thread->name);

    if (thread == mimi_thread_current()) {
        mimi_schedule();
    }

    mimi_exit_critical(level);
    return MIMI_EOK;
}
