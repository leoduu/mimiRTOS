
#include "thread.h"
#include <string.h>
#include "cpuport.h"
#include "list.h"
#include "log.h"
#include "sched.h"

void mimi_thread_timeout_handler_default(mimi_timer *timer)
{
    mimi_tcb *thread = container_of_tcb_by_timer(timer);
    if (thread->status == MIMI_THREAD_SUSPEND) {
        thread->error = MIMI_ETIMEOUT;
        mimi_thread_resume(thread);
        mimi_schedule();
    }
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

    void *exit = exit_func ? exit_func : mimi_thread_exit_func;
    thread->error = MIMI_EOK;
    thread->status = MIMI_THREAD_READY;
    thread->priority = priority;
    thread->origin_priority = priority;
    thread->tick_slice = tick_slice ? tick_slice : THREAD_TIME_SLICE_DEFAULT;
    thread->remaining_tick = thread->tick_slice;
    thread->stack_size = stack_size;
    thread->sp = mimi_stack_init(entry, param, exit, stack, stack_size);
    thread->entry = entry;
    thread->param = param;
    thread->suspend_list = NULL;
    thread->mtx_hold = 0;
    thread->mq_buffer = NULL;
    if (name == NULL) {
        name = "anonymity";
    }
    strncpy(thread->name, name, mimi_min(THREAD_NAME_LEN - 1, strlen(name)));
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

    if (unlikely(thread->status == MIMI_THREAD_SUSPEND)) {
        mimi_exit_critical(level);
        return MIMI_ERROR;
    }

    thread->status = MIMI_THREAD_SUSPEND;
    thread->error = MIMI_EOK;
    if (timeout != MIMI_TIMEOUT_FOREVER) {
        mimi_timer_start(&thread->timer, timeout, MIMI_TIMER_ONCE);
    }
    mimi_sched_detach(thread);
    mimi_exit_critical(level);

    MIMI_LOG_V("%s sleep %ld ms\n", thread->name, timeout);
    return MIMI_EOK;
}

static void insert_suspend_list_by_prio(mimi_list *list, mimi_tcb *thread)
{
    /* insert by priority */
    mimi_node *node, *tmp;
    mimi_list_for_each_safe_start(list, node, tmp) {
        mimi_tcb *temp = container_of_tcb(node);
        if (temp->priority > thread->priority) {
            mimi_list_insert_front(list, &temp->node, &thread->node);
            break;
        }
    } mimi_list_for_each_safe_end(list, node, tmp);
    if (mimi_node_isolated(&thread->node)) {
        mimi_list_push_back(list, &thread->node);
    }
}

mimi_err mimi_thread_block(mimi_tcb *thread, uint32_t timeout, mimi_list *list)
{
    mimi_assert(thread != NULL);
    mimi_assert(list != NULL);

    uint32_t level = mimi_enter_critical();

    if (unlikely(thread->status == MIMI_THREAD_SUSPEND)) {
        mimi_exit_critical(level);
        return MIMI_ERROR;
    }

    thread->status = MIMI_THREAD_SUSPEND;
    thread->error = MIMI_EOK;
    thread->suspend_list = list;
    if (timeout != MIMI_TIMEOUT_FOREVER) {
        mimi_timer_start(&thread->timer, timeout, MIMI_TIMER_ONCE);
    }
    mimi_sched_detach(thread);
    insert_suspend_list_by_prio(list, thread);

    MIMI_LOG_V("%s sleep to list %ld ms\n", thread->name, timeout);

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

    thread->status = MIMI_THREAD_READY;
    if (thread->suspend_list) {
        if (!mimi_node_isolated(&thread->node)) {
            mimi_list_remove(thread->suspend_list, &thread->node);
        }
        thread->suspend_list = NULL;
    }
    mimi_timer_detach(&thread->timer);
    mimi_sched_join(thread);

    MIMI_LOG_V("%s resume\n", thread->name);

    mimi_exit_critical(level);
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
    if (thread->suspend_list != NULL) {
        mimi_list_remove(thread->suspend_list, &thread->node);
        thread->suspend_list = NULL;
    }

    MIMI_LOG_D("%s is dead\n", thread->name);

    if (sched_flag) {
        mimi_schedule();
    }

    mimi_exit_critical(level);
    return MIMI_EOK;
}

void mimi_thread_prio_raise(mimi_tcb *thread, uint8_t prio)
{
    if (thread->priority <= prio) {
        return;
    }

    if (thread->status == MIMI_THREAD_READY) {
        mimi_sched_detach(thread);
        thread->priority = prio;
        mimi_sched_join(thread);

    } else if (thread->status == MIMI_THREAD_SUSPEND) {
        thread->priority = prio;
        if (thread->suspend_list != NULL) {
            mimi_list_remove(thread->suspend_list, &thread->node);
            insert_suspend_list_by_prio(thread->suspend_list, thread);
        }
    }
}

void mimi_thread_prio_recover(mimi_tcb *thread)
{
    if (thread->mtx_hold != 0) {
        return;
    }

    if (thread->status == MIMI_THREAD_READY) {
        mimi_sched_detach(thread);
        thread->priority = thread->origin_priority;
        mimi_sched_join(thread);

    } else if (thread->status == MIMI_THREAD_SUSPEND) {
        thread->priority = thread->origin_priority;
        if (thread->suspend_list != NULL) {
            mimi_list_remove(thread->suspend_list, &thread->node);
            insert_suspend_list_by_prio(thread->suspend_list, thread);
        }
    }
}
