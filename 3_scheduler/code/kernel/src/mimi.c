

#include "thread.h"
#include "cpuport.h"
#include "sched.h"
#include "conf.h"
#include "mimi.h"
#include "timer.h"
#include "log.h"
#include "board.h"
#include "export.h"

static volatile uint32_t sys_tick = 0;

uint32_t mimi_sys_tick(void)
{
    return sys_tick;
}

void mimi_sys_tick_handler(void)
{
    mimi_atom_add(&sys_tick, 1);
    mimi_timer_check();
    mimi_sched_tick_increase(1);
    mimi_isr_schedule_check();
}

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

    mimi_cpu_init(OS_TICK_PER_SECOND, mimi_sys_tick_handler);
    mimi_sched_init();

    mimi_component_init();

    mimi_sched_run();
}

mimi_weak void mimi_assert_failed(const char *x, const char *file, uint32_t line)
{
    mimi_disable_irq();

    MIMI_LOG_E("(%s) assert failed at %s:%d !!!\n", x, file, line);

    // TODO: more debug info

    while(1) {}
}
