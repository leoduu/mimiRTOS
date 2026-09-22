
#include "ipc.h"
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "mimi.h"
#include "list.h"
#include "log.h"
#include "thread.h"
#include "sched.h"

/* -------------------------------------------------------------------------- */
/*  semaphore                                                                 */
/* -------------------------------------------------------------------------- */
void mimi_sem_init(mimi_sem *sem, uint32_t max_cnt, uint32_t init_cnt)
{
    mimi_assert(sem != NULL);

    sem->max_cnt = max_cnt;
    sem->cnt = init_cnt;
    mimi_node_reset(&sem->node);
    mimi_list_init(&sem->suspend_list);
}

mimi_err mimi_sem_take(mimi_sem *sem, uint32_t timeout)
{
    mimi_assert(sem != NULL);

    mimi_tcb *curr = mimi_thread_current();
    uint32_t level = mimi_spin_lock(&sem->lock);

    if (sem->cnt == 0) {
        if (timeout == MIMI_TIMEOUT_NOWAIT) {
            mimi_spin_unlock(&sem->lock, level);
            return MIMI_ERESOURCE;
        }

        int ret = mimi_thread_suspend_to_list(curr, timeout, &sem->suspend_list);
        mimi_spin_unlock(&sem->lock, level);
        if (ret != MIMI_EOK) {
            return ret;
        }
        mimi_schedule();

        /* When the thread is awakened, the code continues to run here, and
         * then check the cause of the wake-up.
         */
        level = mimi_spin_lock(&sem->lock);
        if (curr->timer.status == MIMI_TIMER_TIMEROUT) {
            MIMI_LOG_V(("%s wait semaphore timeout", curr->name));
            mimi_list_remove(&sem->suspend_list, &curr->node);
            mimi_spin_unlock(&sem->lock, level);
            return MIMI_ETIMEOUT;
        }
    }

    sem->cnt--;
    mimi_spin_unlock(&sem->lock, level);

    return MIMI_EOK;
}

mimi_err mimi_sem_release(mimi_sem *sem)
{
    mimi_assert(sem != NULL);

    uint32_t level = mimi_spin_lock(&sem->lock);

    sem->cnt++;
    if (sem->cnt > sem->max_cnt) {
        sem->cnt = sem->max_cnt;
    }
    if (!mimi_list_empty(&sem->suspend_list)) {

        mimi_tcb *thread = container_of_tcb(sem->suspend_list.head);
        mimi_list_pop_front(&sem->suspend_list);
        mimi_spin_unlock(&sem->lock, level);

        mimi_thread_wakeup_from_ipc(thread);
        /* a thread-switch maybe occur */
        return MIMI_EOK;
    }

    mimi_spin_unlock(&sem->lock, level);
    return MIMI_EOK;
}


/* -------------------------------------------------------------------------- */
/*  mutex                                                                     */
/* -------------------------------------------------------------------------- */
void mimi_mutex_init(mimi_mutex *mtx)
{
    mimi_assert(mtx != NULL);

    mtx->lock_cnt = 0;
    mtx->thread = NULL;
    mtx->highest_prio = THREAD_PRIORITY_MAX;
    mimi_node_reset(&mtx->node);
    mimi_list_init(&mtx->suspend_list);
}

mimi_err mimi_mutex_lock(mimi_mutex *mtx, uint32_t timeout)
{
    mimi_assert(mtx != NULL);

    uint32_t level = mimi_spin_lock(&mtx->lock);
    mimi_tcb *curr = mimi_thread_current();
    if (curr == mtx->thread) {
        mtx->lock_cnt++;

    } else {
        if (mtx->lock_cnt > 0) {
            if (timeout == MIMI_TIMEOUT_NOWAIT) {
                mimi_spin_unlock(&mtx->lock, level);
                return MIMI_ERESOURCE;
            }
            /* Temporarily raise the current thread's priority to the level of the
            * highest-priority waiter, so it can release the mutex without delay.
            */
            if (curr->priority > mtx->highest_prio) {
                mimi_thread_prio_raise(curr, mtx->highest_prio);
            } else {
                mtx->highest_prio = curr->priority;
            }

            int ret = mimi_thread_suspend_to_list(curr, timeout, &mtx->suspend_list);
            mimi_spin_unlock(&mtx->lock, level);
            if (ret != MIMI_EOK) {
                return ret;
            }
            mimi_schedule();

            /* When the thread is awakened, the code continues to run here, and
            * then check the cause of the wake-up.
            */
            level = mimi_spin_lock(&mtx->lock);
            if (curr->timer.status == MIMI_TIMER_TIMEROUT) {
                MIMI_LOG_V(("%s wait mutex timeout", curr->name));
                mimi_list_remove(&mtx->suspend_list, &curr->node);
                mimi_spin_unlock(&mtx->lock, level);
                return MIMI_ETIMEOUT;
            }
        }
        mtx->lock_cnt = 1;
        mtx->thread = curr;
    }

    mimi_spin_unlock(&mtx->lock, level);
    return MIMI_EOK;
}

mimi_err mimi_mutex_unlock(mimi_mutex *mtx)
{
    mimi_assert(mtx != NULL);

    uint32_t level = mimi_spin_lock(&mtx->lock);
    mimi_tcb *curr = mimi_thread_current();

    if (curr != mtx->thread) {
        mimi_spin_unlock(&mtx->lock, level);
        return MIMI_ERROR;
    }

    mtx->lock_cnt--;
    if (mtx->lock_cnt == 0) {
        if (!mimi_list_empty(&mtx->suspend_list)) {
            mimi_tcb *thread = container_of_tcb(mtx->suspend_list.head);
            mimi_list_pop_front(&mtx->suspend_list);
            mimi_spin_unlock(&mtx->lock, level);

            mimi_thread_wakeup_from_ipc(thread);
            /* a thread-switch maybe occur */
            return MIMI_EOK;
        }
        mimi_thread_prio_recover(mtx->thread);
        mtx->thread = NULL;
    }

    mimi_spin_unlock(&mtx->lock, level);
    return MIMI_EOK;
}


/* -------------------------------------------------------------------------- */
/*  message queue                                                             */
/* -------------------------------------------------------------------------- */
mimi_err mimi_mqueue_init(mimi_mqueue *mq, uint32_t msg_size, void *buffer, uint32_t buffer_size)
{
    mimi_assert(mq != NULL);
    mimi_assert(buffer != NULL);
    mimi_assert(msg_size != 0);
    mimi_assert(buffer_size >= msg_size);

    mq->msg_size = msg_size;
    mq->capacity = buffer_size / msg_size;
    mq->cnt = 0;
    mq->prod = 0;
    mq->cons = 0;
    mq->buffer = buffer;

    mimi_node_reset(&mq->node);
    mimi_list_init(&mq->recv_suspend_list);

    return MIMI_EOK;
}

mimi_err mimi_mqueue_send(mimi_mqueue *mq, const void *buffer)
{
    mimi_assert(mq != NULL);
    mimi_assert(buffer != NULL);

    uint32_t level = mimi_spin_lock(&mq->lock);

    if (mq->cnt == mq->capacity) {
        mimi_spin_unlock(&mq->lock, level);
        return MIMI_ERESOURCE;
    }

    void *prod_ptr = (uint8_t *)mq->buffer + mq->msg_size * mq->prod;
    memcpy(prod_ptr, buffer, mq->msg_size);
    mq->prod = (mq->prod + 1) % mq->capacity;
    mq->cnt++;

    if (!mimi_list_empty(&mq->recv_suspend_list)) {
        mimi_tcb *thread = container_of_tcb(mq->recv_suspend_list.head);
        mimi_list_pop_front(&mq->recv_suspend_list);
        mimi_spin_unlock(&mq->lock, level);
        mimi_thread_wakeup_from_ipc(thread);

        return MIMI_EOK;
    }

    mimi_spin_unlock(&mq->lock, level);
    return MIMI_EOK;
}

mimi_err mimi_mqueue_recv(mimi_mqueue *mq, void *buffer, uint32_t timeout)
{
    mimi_assert(mq != NULL);
    mimi_assert(buffer != NULL);

    mimi_tcb *curr = mimi_thread_current();
    uint32_t level = mimi_spin_lock(&mq->lock);

    if (mq->cnt == 0) {
        if (timeout == MIMI_TIMEOUT_NOWAIT) {
            mimi_spin_unlock(&mq->lock, level);
            return MIMI_ERESOURCE;
        }

        int ret = mimi_thread_suspend_to_list(curr, timeout, &mq->recv_suspend_list);
        mimi_spin_unlock(&mq->lock, level);
        if (ret != MIMI_EOK) {
            return ret;
        }
        mimi_schedule();

        /* When the thread is awakened, the code continues to run here, and
         * then check the cause of the wake-up.
         */
        level = mimi_spin_lock(&mq->lock);
        if (curr->timer.status == MIMI_TIMER_TIMEROUT) {
            MIMI_LOG_V(("%s wait mqueue timeout", curr->name));
            mimi_list_remove(&mq->recv_suspend_list, &curr->node);
            mimi_spin_unlock(&mq->lock, level);
            return MIMI_ETIMEOUT;
        }
    }

    void *cons_ptr = (uint8_t *)mq->buffer + mq->msg_size * mq->cons;
    memcpy(buffer, cons_ptr, mq->msg_size);
    mq->cons = (mq->cons + 1) %  mq->capacity;
    mq->cnt--;

    mimi_spin_unlock(&mq->lock, level);

    return MIMI_EOK;
}
