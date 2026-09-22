
#include "thread.h"
#include "cpuport.h"
#include "list.h"

mimi_list mimi_thread_list;

mimi_err mimi_thread_init(mimi_tcb   *thread,
                            void        *stack,
                            uint32_t    stack_size,
                            void        (*entry)(void* param),
                            void        *param)
{
    mimi_assert(thread != NULL);
    mimi_assert(stack != NULL);
    mimi_assert(entry != NULL);
    mimi_assert(stack_size > 0);

    thread->stack_size = stack_size;
    thread->entry = entry;
    thread->param = param;
    thread->sp = mimi_stack_init(entry, param, stack, stack_size);

    mimi_list_push_back(&mimi_thread_list, &thread->node);
    return MIMI_EOK;
}

mimi_tcb *mimi_thread_first(void)
{
    return container_of_tcb(mimi_thread_list.head);
}

void mimi_thread_yield(void)
{
    mimi_tcb *from_tcb = container_of_tcb(mimi_thread_list.head);
    mimi_list_rotate(&mimi_thread_list);
    mimi_tcb *to_tcb = container_of_tcb(mimi_thread_list.head);

    if (from_tcb == to_tcb) {
        return;
    }
    mimi_context_switch((void *)&from_tcb->sp, (void *)&to_tcb->sp);
}
