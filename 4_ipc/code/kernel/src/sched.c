
#include "sched.h"
#include "thread.h"
#include "list.h"
#include "log.h"
#include "conf.h"
#include "cpuport.h"

struct mimi_sched {
    mimi_bool isr_flag;
    uint32_t priority_mask;
    mimi_list ready_list[THREAD_PRIORITY_MAX];
    mimi_list death_list;
} mimi_sched;

mimi_tcb * volatile mimi_curr_thread = NULL;
mimi_tcb *mimi_next_thread = NULL;

mimi_tcb idle_thread;
uint8_t idle_thread_stack[IDLE_STACK_SIZE] mimi_aligned(8) = {0};
void mimi_idle_thread_entry(void *param);


static inline uint8_t highest_priority(void)
{
    return mimi_ctz(mimi_sched.priority_mask);
}

static inline void set_priority(uint8_t priority)
{
    mimi_sched.priority_mask |= 1 << priority;
}

static inline void clear_priority(uint8_t priority)
{
    mimi_sched.priority_mask &= ~(1 << priority);
}


mimi_tcb *mimi_thread_current(void)
{
    return mimi_curr_thread;
}

mimi_err mimi_sched_init(void)
{
    mimi_sched.priority_mask = 0;
    mimi_sched.isr_flag = MIMI_FALSE;
    mimi_list_init(&mimi_sched.death_list);
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
}

static void mimi_sched_schedule(void)
{
    mimi_disable_irq();

    mimi_list *list = &mimi_sched.ready_list[highest_priority()];
    mimi_tcb *thread = mimi_thread_current();

    if (thread->remaining_tick == 0) {
        thread->remaining_tick = thread->tick_slice;
    }

    if (mimi_list_head(list) == &thread->node) {
        mimi_list_rotate(list);
    }
    mimi_tcb *next_thread = container_of_tcb(mimi_list_head(list));
    if (thread == next_thread) {
        mimi_enable_irq();
        return;
    }

    mimi_next_thread = next_thread;
    mimi_context_switch((void *)&thread->sp, (void *)&next_thread->sp);
}

void mimi_sched_tick_increase(uint32_t tick)
{
    mimi_tcb *thread = mimi_thread_current();
    if (thread->remaining_tick > tick) {
        thread->remaining_tick -= tick;
    } else {
        thread->remaining_tick = 0;
        mimi_schedule();
    }
}

/* -------------------------------------------------------------------------- */
/*  thread api                                                                */
/* -------------------------------------------------------------------------- */
void mimi_sched_join(mimi_tcb *thread, mimi_bool schedule)
{
    uint32_t level = mimi_enter_critical();

    uint8_t curr_prio = highest_priority();
    uint8_t prio = thread->priority;

    if (schedule) {
        mimi_list_push_front(&mimi_sched.ready_list[prio], &thread->node);
    } else {
        mimi_list_push_back(&mimi_sched.ready_list[prio], &thread->node);
    }
    set_priority(prio);

    mimi_exit_critical(level);

    if (schedule && prio <= curr_prio) {
        mimi_schedule();
    }
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
    if (mimi_is_in_isr()) {
        mimi_sched.isr_flag = MIMI_TRUE;
    } else {
        mimi_sched_schedule();
    }
}

void mimi_isr_schedule_check(void)
{
    if (!mimi_sched.isr_flag) {
        return;
    }
    mimi_sched.isr_flag = MIMI_FALSE;
    mimi_sched_schedule();
}

/* -------------------------------------------------------------------------- */
/*  idle thread                                                               */
/* -------------------------------------------------------------------------- */
void mimi_idle_thread_entry(void *param)
{
    while (1) {
        if (!mimi_list_empty(&mimi_sched.death_list)) {
            mimi_list *list = &mimi_sched.death_list;
            mimi_node *node;
            mimi_node *tmp;
            mimi_list_for_each_safe_start(list, node, tmp) {
                mimi_list_remove(list, node);
            } mimi_list_for_each_safe_end(list, node, tmp);
        }
    }
}
