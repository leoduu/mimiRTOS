
#include "ipc.h"
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "mimi.h"
#include "list.h"
#include "log.h"
#include "thread.h"
#include "sched.h"
#include "mimi_port.h"

/* -------------------------------------------------------------------------- */
/*  semaphore                                                                 */
/* -------------------------------------------------------------------------- */
void mimi_sem_init(mimi_sem *sem, uint32_t max_cnt, uint32_t init_cnt)
{
    mimi_assert(sem != NULL);

    sem->max_cnt = max_cnt;
    sem->cnt = init_cnt;
    mimi_list_init(&sem->suspend_list);
}

mimi_err mimi_sem_take(mimi_sem *sem, uint32_t timeout)
{
    mimi_assert(sem != NULL);

    mimi_err ret = MIMI_EOK;
    uint32_t level = mimi_enter_critical();

    mimi_assert((level == 0U) && !mimi_is_in_isr());

    if (sem->cnt > 0) {
        sem->cnt--;
        goto _sem_take_exit;
    }
    if (timeout == MIMI_TIMEOUT_NOWAIT) {
        ret = MIMI_ERESOURCE;
        goto _sem_take_exit;
    }

    mimi_tcb *curr = mimi_thread_current();
    ret = mimi_thread_block(curr, timeout, &sem->suspend_list);
    if (ret != MIMI_EOK) {
        goto _sem_take_exit;
    }
    mimi_exit_critical(level);

    /* When the thread is awakened, the code continues to run here, and
        * then check the cause of the wake-up.
        */
    ret = curr->error;
    if (ret != MIMI_EOK) {
        MIMI_LOG_E("%s take sem failed:%d", curr->name, ret);
    }
    return ret;

_sem_take_exit:
    mimi_exit_critical(level);
    return ret;
}

mimi_err mimi_sem_release(mimi_sem *sem)
{
    mimi_assert(sem != NULL);

    uint32_t level = mimi_enter_critical();

    if (!mimi_list_empty(&sem->suspend_list)) {
        mimi_node *head = mimi_list_pop_front(&sem->suspend_list);
        mimi_tcb *thread = container_of_tcb(head);
        mimi_thread_resume(thread);

    } else {
        sem->cnt++;
        if (sem->cnt > sem->max_cnt) {
            sem->cnt = sem->max_cnt;
        }
    }

    mimi_exit_critical(level);
    return MIMI_EOK;
}


/* -------------------------------------------------------------------------- */
/*  mutex                                                                     */
/* -------------------------------------------------------------------------- */
void mimi_mutex_init(mimi_mutex *mtx)
{
    mimi_assert(mtx != NULL);

    mtx->lock_nest = 0;
    mtx->owner = NULL;
    mimi_node_reset(&mtx->node);
    mimi_list_init(&mtx->suspend_list);
}

mimi_err mimi_mutex_lock(mimi_mutex *mtx, uint32_t timeout)
{
    mimi_assert(mtx != NULL);

    mimi_err ret = MIMI_EOK;
    uint32_t level = mimi_enter_critical();
    mimi_tcb *curr = mimi_thread_current();

    mimi_assert(curr != NULL);
    mimi_assert((level == 0U) && !mimi_is_in_isr());

    if (mtx->lock_nest == 0 || mtx->owner == NULL) {
        mtx->lock_nest = 1;
        mtx->owner = curr;
        mimi_list_push_back(&curr->mtx_list, &mtx->node);
        goto _mutex_lock_exit;
    }
    if (curr == mtx->owner) {
        mtx->lock_nest++;
        goto _mutex_lock_exit;
    }
    if (timeout == MIMI_TIMEOUT_NOWAIT) {
        ret = MIMI_ERESOURCE;
        goto _mutex_lock_exit;
    }

    /* Temporarily raise the thread's priority to the level of the
    * highest-priority waiter, so it can release the mutex without delay.
    */
    if (curr->priority < mtx->owner->priority) {
        mimi_thread_prio_raise(mtx->owner, curr->priority);
    }
    ret = mimi_thread_block(curr, timeout, &mtx->suspend_list);
    if (ret != MIMI_EOK) {
        goto _mutex_lock_exit;
    }
    mimi_exit_critical(level);

    ret = curr->error;
    if (ret != MIMI_EOK) {
        MIMI_LOG_E("%s lock mutex failed:%d", curr->name, ret);
    }
    return ret;

_mutex_lock_exit:
    mimi_exit_critical(level);
    return ret;
}

static void mimi_mutex_handoff(mimi_mutex *mtx)
{
    if (!mimi_list_empty(&mtx->suspend_list)) {
        mimi_node *head = mimi_list_pop_front(&mtx->suspend_list);
        mimi_tcb *thread = container_of_tcb(head);

        mimi_assert(thread->status != MIMI_THREAD_DEAD);

        mtx->lock_nest = 1;
        mtx->owner = thread;
        mimi_list_push_back(&thread->mtx_list, &mtx->node);
        mimi_thread_resume(thread);
    } else {
        mtx->lock_nest = 0;
        mtx->owner = NULL;
    }
}

mimi_err mimi_mutex_unlock(mimi_mutex *mtx)
{
    mimi_assert(mtx != NULL);

    uint32_t level = mimi_enter_critical();
    mimi_tcb *curr = mimi_thread_current();

    if (curr != mtx->owner) {
        mimi_exit_critical(level);
        return MIMI_ERROR;
    }

    mtx->lock_nest--;
    if (mtx->lock_nest == 0) {
        mimi_list_remove(&curr->mtx_list, &mtx->node);

        if (mimi_list_empty(&curr->mtx_list)) {
            mimi_thread_prio_recover(curr);
        }
        mimi_mutex_handoff(mtx);
    }

    mimi_exit_critical(level);
    return MIMI_EOK;
}

void mimi_mutex_release_all(mimi_tcb *thread)
{
    mimi_assert(thread != NULL);

    mimi_node *node;
    mimi_node *tmp;
    uint32_t level = mimi_enter_critical();

    mimi_list_for_each_safe_start(&thread->mtx_list, node, tmp) {
        mimi_mutex *mtx = container_of(node, mimi_mutex, node);

        mtx->lock_nest = 0;
        mtx->owner = NULL;
        mimi_list_remove(&thread->mtx_list, node);
        mimi_mutex_handoff(mtx);

    } mimi_list_for_each_safe_end(&thread->mtx_list, node, tmp);

    mimi_exit_critical(level);
}


/* -------------------------------------------------------------------------- */
/*  message queue                                                             */
/* -------------------------------------------------------------------------- */
#define mimi_mqueue_advance(mq, ptr)                 \
    do {                                             \
        (mq)->ptr += (mq)->msg_size;                 \
        if ((mq)->ptr >= (mq)->end) {                \
            (mq)->ptr = (mq)->buffer;                \
        }                                            \
    } while (0)

mimi_inline void mq_copy_words(void *dst, const void *src, uint32_t words)
{
    uint32_t *d = (uint32_t *)dst;
    const uint32_t *s = (const uint32_t *)src;

    while (words--) {
        *d++ = *s++;
    }
}

mimi_err mimi_mqueue_init(mimi_mqueue *mq, uint32_t msg_size, void *buffer, uint32_t buffer_size)
{
    mimi_assert(mq != NULL);
    mimi_assert(buffer != NULL);
    mimi_assert(msg_size != 0);
    mimi_assert(buffer_size >= msg_size);
    mimi_assert(msg_size % 4 == 0);

    mq->msg_size = msg_size / 4;
    mq->capacity = buffer_size / msg_size;
    mq->cnt = 0;
    mq->prod = (unsigned long*)buffer;
    mq->cons = (unsigned long*)buffer;
    mq->buffer = (unsigned long*)buffer;
    mq->end = mq->buffer + (mq->capacity * mq->msg_size);
    mimi_list_init(&mq->recv_suspend_list);

    return MIMI_EOK;
}

mimi_err mimi_mqueue_send(mimi_mqueue *mq, const void *buffer, mimi_mqueue_flag flag)
{
    mimi_assert(mq != NULL);
    mimi_assert(buffer != NULL);

    uint32_t level = mimi_enter_critical();

    if (!mimi_list_empty(&mq->recv_suspend_list)) {
        mimi_node *head = mimi_list_pop_front(&mq->recv_suspend_list);
        mimi_tcb *thread = container_of_tcb(head);
        mq_copy_words(thread->mq_buffer, buffer, mq->msg_size);
        thread->mq_buffer = NULL;
        mimi_thread_resume(thread);

        mimi_exit_critical(level);
        return MIMI_EOK;
    }

    unsigned long *ptr;
    if (mq->cnt == mq->capacity) {
        switch (flag) {
        case MIMI_MQ_URGENT:
            ptr = mq->cons;
            break;
        case MIMI_MQ_FULL_OVERWRITE:
            ptr = mq->cons;
            mimi_mqueue_advance(mq, cons);
            mq->prod = mq->cons;
            break;
        case MIMI_MQ_FULL_DROP:
        default:
            mimi_exit_critical(level);
            return MIMI_ERESOURCE;
        }
    } else {
        if (flag == MIMI_MQ_URGENT) {
            ptr = mq->cons == mq->buffer ? mq->end - mq->msg_size : mq->cons - mq->msg_size;
            mq->cons = ptr;
        } else {
            /* Normal */
            ptr = mq->prod;
            mimi_mqueue_advance(mq, prod);
        }
        mq->cnt++;
    }
    mq_copy_words(ptr, buffer, mq->msg_size);

    mimi_exit_critical(level);
    return MIMI_EOK;
}

mimi_err mimi_mqueue_recv(mimi_mqueue *mq, void *buffer, uint32_t timeout)
{
    mimi_assert(mq != NULL);
    mimi_assert(buffer != NULL);

    mimi_err ret = MIMI_EOK;
    uint32_t level = mimi_enter_critical();

    mimi_assert((level == 0U) && !mimi_is_in_isr());

    if (mq->cnt > 0) {
        mq_copy_words(buffer, mq->cons, mq->msg_size);
        mimi_mqueue_advance(mq, cons);
        mq->cnt--;
        goto _mqueue_recv_exit;
    }

    if (timeout == MIMI_TIMEOUT_NOWAIT) {
        ret = MIMI_ERESOURCE;
        goto _mqueue_recv_exit;
    }

    mimi_tcb *curr = mimi_thread_current();
    curr->mq_buffer = buffer;
    ret = mimi_thread_block(curr, timeout, &mq->recv_suspend_list);
    if (ret != MIMI_EOK) {
        curr->mq_buffer = NULL;
        goto _mqueue_recv_exit;
    }
    mimi_exit_critical(level);

    ret = curr->error;
    if (ret != MIMI_EOK) {
        curr->mq_buffer = NULL;
        MIMI_LOG_E("%s mqueue recv failed:%d", curr->name, ret);
    }
    return ret;

_mqueue_recv_exit:
    mimi_exit_critical(level);
    return ret;
}
