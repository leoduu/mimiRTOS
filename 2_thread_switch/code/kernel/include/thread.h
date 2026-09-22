#ifndef __MIMI_THREAD__
#define __MIMI_THREAD__

#include "list.h"

#define container_of_tcb(ptr)  (container_of(ptr, mimi_tcb, node))

typedef struct {
    mimi_node   node;

    void        *sp;
    uint32_t    stack_size;
    void        *entry;
    void        *param;
} mimi_tcb;

mimi_err mimi_thread_init(mimi_tcb   *thread,
                            void        *stack,
                            uint32_t    stack_size,
                            void        (*entry)(void* param),
                            void        *param);

mimi_tcb *mimi_thread_first(void);
void mimi_thread_yield(void);

#endif // __MIMI_THREAD__
