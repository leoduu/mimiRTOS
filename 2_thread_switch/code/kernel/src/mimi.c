
#include "mimi.h"
#include <stdio.h>
#include "thread.h"
#include "board.h"
#include "cpuport.h"
#include "export.h"

void mimi_component_init(void)
{
    extern const size_t __mimi_init_start;
    extern const size_t __mimi_init_end;
    volatile const init_fn_t *fn_ptr;

    for (fn_ptr = (init_fn_t *)&__mimi_init_start;
         fn_ptr < (init_fn_t *)&__mimi_init_end; fn_ptr++) {
        (*fn_ptr)();
    }
}

static int mimi_init_placeholder(void)
{
    return 0;
}
INIT_EXPORT(mimi_init_placeholder, "9");

void mimi_main(void)
{
    mimi_disable_irq();

    mimi_board_init();
    mimi_cpu_init();
    mimi_component_init();

    mimi_tcb *first = mimi_thread_first();
    mimi_first_switch((void *)&first->sp);
}

mimi_weak void mimi_assert_failed(const char *x, const char *file, unsigned int line)
{
    mimi_disable_irq();

    printf("(%s) assert failed at %s:%d !!!\n", x, file, line);

    // TODO: more debug info

    while(1) {}
}
