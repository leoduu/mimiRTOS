
#include "timer.h"
#include <stdlib.h>
#include "list.h"
#include "mimi.h"
#include "cpuport.h"

mimi_list mimi_timer_list;

mimi_err mimi_timer_init(mimi_timer *timer, timeout_handler handler)
{
    mimi_assert(timer != NULL);
    mimi_assert(handler != NULL);

    mimi_node_reset(&timer->node);
    timer->timeout = 0;
    timer->stop_tick = 0;
    timer->timeout_tick = 0;
    timer->status = MIMI_TIMER_STOP;
    timer->flag = 0;
    timer->handler = handler;
    return MIMI_EOK;
}

mimi_err mimi_timer_start(mimi_timer *timer, uint32_t timeout_tick,
                             mimi_timer_flag flag)
{
    mimi_assert(timer != NULL);

    uint32_t level = mimi_enter_critical();

    if (timer->status == MIMI_TIMER_RUNNING) {
        mimi_exit_critical(level);
        return MIMI_ERROR;
    }

    timer->timeout = timeout_tick;
    timer->timeout_tick = mimi_sys_tick() + timeout_tick;
    timer->status = MIMI_TIMER_RUNNING;
    timer->flag = flag;
    mimi_timer_join(timer);

    mimi_exit_critical(level);
    return MIMI_EOK;
}

mimi_err mimi_timer_stop(mimi_timer *timer)
{
    mimi_assert(timer != NULL);

    uint32_t level = mimi_enter_critical();

    if (timer->status != MIMI_TIMER_RUNNING) {
        mimi_exit_critical(level);
        return MIMI_ERROR;
    }

    timer->stop_tick = mimi_sys_tick();
    timer->status = MIMI_TIMER_STOP;
    mimi_list_remove(&mimi_timer_list, &timer->node);

    mimi_exit_critical(level);
    return MIMI_EOK;
}

mimi_err mimi_timer_resume(mimi_timer *timer)
{
    mimi_assert(timer != NULL);

    uint32_t level = mimi_enter_critical();

    if (timer->status != MIMI_TIMER_STOP) {
        mimi_exit_critical(level);
        return MIMI_ERROR;
    }

    uint32_t elapsed = mimi_sys_tick() - timer->stop_tick;
    timer->stop_tick = 0;
    timer->timeout_tick += elapsed;
    timer->status = MIMI_TIMER_RUNNING;
    mimi_timer_join(timer);

    mimi_exit_critical(level);
    return MIMI_EOK;
}

mimi_err mimi_timer_join(mimi_timer *timer)
{
    mimi_assert(timer != NULL);

    uint32_t level = mimi_enter_critical();

    mimi_timer *timer_temp;
    mimi_node *node;
    mimi_list *list = &mimi_timer_list;

    // timeout tick from small to large
    mimi_list_for_each_start(list, node) {
        timer_temp = container_of_timer(node);
        if (timer->timeout_tick < timer_temp->timeout_tick) {
            mimi_list_insert_front(list, &timer_temp->node, &timer->node);
            break;
        }
    } mimi_list_for_each_end(list, node);

    // largest timeout tick
    if (mimi_node_isolated(&timer->node)) {
        mimi_list_push_back(list, &timer->node);
    }

    mimi_exit_critical(level);
    return MIMI_EOK;
}

mimi_err mimi_timer_detach(mimi_timer *timer)
{
    mimi_assert(timer != NULL);

    uint32_t level = mimi_enter_critical();

    if (timer->status == MIMI_TIMER_RUNNING) {
        mimi_list_remove(&mimi_timer_list, &timer->node);
        timer->status = MIMI_TIMER_STOP;
    }
    timer->stop_tick = 0;

    mimi_exit_critical(level);
    return MIMI_EOK;
}

void mimi_timer_check(void)
{
    uint32_t level = mimi_enter_critical();

    if (mimi_list_empty(&mimi_timer_list)) {
        mimi_exit_critical(level);
        return;
    }

    mimi_timer *timer;
    mimi_node *node;
    mimi_node *tmp;
    mimi_list *list = &mimi_timer_list;

    mimi_list_for_each_safe_start(list, node, tmp) {

        timer = container_of_timer(node);
        if (timer->timeout_tick > mimi_sys_tick()) {
            break;
        }

        // timeout
        mimi_list_remove(list, node);
        timer->status = MIMI_TIMER_TIMEROUT;
        timer->handler(timer);
        if (timer->flag & MIMI_TIMER_PERIODIC) {
            timer->status = MIMI_TIMER_RUNNING;
            timer->timeout_tick = mimi_sys_tick() + timer->timeout;
            mimi_timer_join(timer);
        }
    } mimi_list_for_each_safe_end(list, node, tmp);

    mimi_exit_critical(level);
}
