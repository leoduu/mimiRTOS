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

    mimi_thread_status status;

    void        *sp;
    uint32_t    stack_size;

    void        *entry;
    void        *param;

    char        name[THREAD_NAME_LEN];
    uint8_t     priority;
    uint32_t    tick_slice;
    uint32_t    remaining_tick;

    mimi_timer  timer;
} mimi_tcb;

mimi_err mimi_thread_init(mimi_tcb   *thread,
                            const char  *name,
                            uint8_t     priority,
                            uint32_t    tick_slice,
                            void        *stack,
                            uint32_t    stack_size,
                            void        (*entry)(void* param),
                            void        *param,
                            void        (*exit_func)(void));
void mimi_thread_yield(void);
mimi_err mimi_thread_delay(uint32_t delay);
mimi_err mimi_thread_suspend(mimi_tcb *thread, uint32_t timeout);
mimi_err mimi_thread_wakeup(mimi_tcb *thread);
mimi_err mimi_thread_kill(mimi_tcb *thread);

#endif // __MIMI_THREAD__
