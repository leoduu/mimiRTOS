#ifndef __MIMI_THREAD__
#define __MIMI_THREAD__

#include "list.h"
#include "timer.h"
#include "conf.h"

#define container_of_tcb(ptr)           (container_of(ptr, mimi_tcb, node))
#define container_of_tcb_by_timer(ptr)  (container_of(ptr, mimi_tcb, timer))

typedef enum {
    MIMI_THREAD_READY,
    MIMI_THREAD_SUSPEND,
    MIMI_THREAD_DEAD,
} mimi_thread_status;

typedef struct {
    mimi_node   node;

    mimi_err    error;
    mimi_thread_status status;

    void        *sp;
    uint32_t    stack_size;

    void        *entry;
    void        *param;

    char        name[THREAD_NAME_LEN];
    uint8_t     priority;
    uint8_t     origin_priority;
    uint8_t     mtx_hold;
    uint32_t    tick_slice;
    uint32_t    remaining_tick;

    mimi_list   *suspend_list;  /* IPC suspend_list while blocked */
    void        *mq_buffer;     /* Used for message queue zero copy */
    mimi_timer  timer;
} mimi_tcb;

/* without schedule */
mimi_err mimi_thread_init(mimi_tcb   *thread,
                            const char  *name,
                            uint8_t     priority,
                            uint32_t    tick_slice,
                            void        *stack,
                            uint32_t    stack_size,
                            void        (*entry)(void* param),
                            void        *param,
                            void        (*exit_func)(void));
mimi_err mimi_thread_suspend(mimi_tcb *thread, uint32_t timeout);
mimi_err mimi_thread_resume(mimi_tcb *thread);
mimi_err mimi_thread_block(mimi_tcb *thread, uint32_t timeout, mimi_list *list);
void mimi_thread_prio_raise(mimi_tcb *thread, uint8_t prio);
void mimi_thread_prio_recover(mimi_tcb *thread);

/* with schedule */
void mimi_thread_yield(void);
mimi_err mimi_thread_delay(uint32_t delay);
mimi_err mimi_thread_kill(mimi_tcb *thread);

#endif // __MIMI_THREAD__
