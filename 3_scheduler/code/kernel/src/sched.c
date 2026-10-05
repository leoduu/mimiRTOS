
#include "sched.h"
#include "thread.h"
#include "list.h"
#include "log.h"
#include "conf.h"
#include "cpuport.h"

struct mimi_sched {
    uint32_t priority_mask;
    mimi_list ready_list[THREAD_PRIORITY_MAX];
} mimi_sched;

mimi_tcb * volatile mimi_curr_thread = NULL;
mimi_tcb *mimi_next_thread = NULL;

mimi_tcb idle_thread;
uint8_t idle_thread_stack[IDLE_STACK_SIZE] mimi_aligned(8) = {0};
void mimi_idle_thread_entry(void *param);


mimi_inline uint8_t highest_priority(void)
{
    return mimi_ctz(mimi_sched.priority_mask);
}

mimi_inline void set_priority(uint8_t priority)
{
    mimi_sched.priority_mask |= 1u << priority;
}

mimi_inline void clear_priority(uint8_t priority)
{
    mimi_sched.priority_mask &= ~(1u << priority);
}

mimi_tcb *mimi_thread_current(void)
{
    return mimi_curr_thread;
}

mimi_err mimi_sched_init(void)
{
    mimi_sched.priority_mask = 0;
    for (int i = 0; i < THREAD_PRIORITY_MAX; i++) {
        mimi_list_init(&mimi_sched.ready_list[i]);
    }

    mimi_thread_init(&idle_thread, "idle", IDLE_THREAD_PRIORITY,
                    IDLE_THREAD_TICK_SLICE, idle_thread_stack,
                    IDLE_STACK_SIZE, mimi_idle_thread_entry, NULL, NULL);

    MIMI_LOG_I(("scheduler init\r\n"));

    return MIMI_EOK;
}

void mimi_sched_run(void)
{
    MIMI_LOG_I(("mimimi os run\r\n"));

    mimi_list *list = &mimi_sched.ready_list[highest_priority()];
    mimi_tcb *first = container_of_tcb(mimi_list_head(list));
    mimi_next_thread = first;

    mimi_context_switch_to((void *)&first->sp);

    mimi_enable_irq();
    mimi_unreachable;
}

mimi_inline void mimi_sched_schedule(void)
{
    mimi_tcb *thread = mimi_curr_thread;
    mimi_list *list = &mimi_sched.ready_list[highest_priority()];

    mimi_tcb *next_thread = container_of_tcb(mimi_list_head(list));
    if (thread == next_thread) {
        return;
    }
    mimi_next_thread = next_thread;

    mimi_context_switch((void *)&thread->sp, (void *)&next_thread->sp);
}

void mimi_sched_tick_increase(uint32_t tick)
{
    mimi_tcb *thread = mimi_curr_thread;

    if (thread->remaining_tick > tick) {
        thread->remaining_tick -= tick;
    } else {
        mimi_schedule_rr();
    }
}

/* -------------------------------------------------------------------------- */
/*  thread api                                                                */
/* -------------------------------------------------------------------------- */
void mimi_sched_join(mimi_tcb *thread)
{
    uint32_t level = mimi_enter_critical();

    uint8_t prio = thread->priority;
    mimi_list_push_back(&mimi_sched.ready_list[prio], &thread->node);
    set_priority(prio);

    mimi_exit_critical(level);
}

void mimi_sched_detach(mimi_tcb *thread)
{
    uint32_t level = mimi_enter_critical();

    uint8_t prio = thread->priority;
    mimi_list_remove(&mimi_sched.ready_list[prio], &thread->node);
    if (mimi_list_empty(&mimi_sched.ready_list[prio])) {
        clear_priority(prio);
    }

    mimi_exit_critical(level);
}

void mimi_schedule(void)
{
    uint32_t level = mimi_enter_critical();

    mimi_sched_schedule();

    mimi_exit_critical(level);
}

void mimi_schedule_rr(void)
{
    uint32_t level = mimi_enter_critical();
    mimi_tcb *thread = mimi_curr_thread;

    thread->remaining_tick = thread->tick_slice;
    mimi_list *list = &mimi_sched.ready_list[thread->priority];
    if (list->head == &thread->node) {
        mimi_list_rotate(list);
    }

    mimi_sched_schedule();
    mimi_exit_critical(level);
}

/* -------------------------------------------------------------------------- */
/*  idle thread                                                               */
/* -------------------------------------------------------------------------- */
void mimi_idle_thread_entry(void *param)
{
    while (1) {
    }
}
