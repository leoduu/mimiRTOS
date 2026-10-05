#ifndef __MIMI_IPC__
#define __MIMI_IPC__

#include "conf.h"
#include "mimi.h"
#include "list.h"
#include "thread.h"

/* -------------------------------------------------------------------------- */
/*  semaphore                                                                 */
/* -------------------------------------------------------------------------- */
typedef struct {
    uint32_t max_cnt;
    uint32_t cnt;
    mimi_list suspend_list;
} mimi_sem;

void mimi_sem_init(mimi_sem *sem, uint32_t max_cnt, uint32_t init_cnt);
mimi_err mimi_sem_take(mimi_sem *sem, uint32_t timeout);
mimi_err mimi_sem_release(mimi_sem *sem);


/* -------------------------------------------------------------------------- */
/*  mutex                                                                     */
/* -------------------------------------------------------------------------- */
typedef struct {
    uint8_t lock_nest;
    mimi_tcb *owner;
    mimi_list suspend_list;
} mimi_mutex;

void mimi_mutex_init(mimi_mutex *mtx);
mimi_err mimi_mutex_lock(mimi_mutex *mtx, uint32_t timeout);
mimi_err mimi_mutex_unlock(mimi_mutex *mtx);


/* -------------------------------------------------------------------------- */
/*  message queue                                                             */
/* -------------------------------------------------------------------------- */
typedef struct {
    uint32_t msg_size;
    uint32_t capacity;
    uint32_t cnt;

    unsigned long *prod;
    unsigned long *cons;
    unsigned long *buffer;
    unsigned long *end;

    mimi_list recv_suspend_list;
} mimi_mqueue;

typedef enum {
    MIMI_MQ_FULL_DROP,
    MIMI_MQ_FULL_OVERWRITE,
    MIMI_MQ_URGENT,
} mimi_mqueue_flag;

mimi_err mimi_mqueue_init(mimi_mqueue *mqueue, uint32_t msg_size, void *buffer, uint32_t buffer_size);
mimi_err mimi_mqueue_send(mimi_mqueue *mqueue, const void *buffer, mimi_mqueue_flag flag);
mimi_err mimi_mqueue_recv(mimi_mqueue *mqueue, void *buffer, uint32_t timeout);

#endif  // __MIMI_IPC__
