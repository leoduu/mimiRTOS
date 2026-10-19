

#include "thread.h"
#include "mimi_port.h"
#include "sched.h"
#include "conf.h"
#include "mimi.h"
#include "timer.h"
#include "log.h"
#include "board.h"
#include "export.h"
#include "mimi_string.h"

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

    mimi_log_init();
    mimi_board_init();
    mimi_cpu_init(OS_TICK_PER_SECOND, mimi_sys_tick_handler);
    mimi_sched_init();

    mimi_component_init();

    mimi_sched_run();
}

#if MIMI_DEBUG_ASSERT

mimi_weak void mimi_assert_output(const char *s, int len)
{
    (void)s;
    (void)len;
}

mimi_weak void mimi_assert_failed(const char *x, const char *file, uint32_t line)
{
    char buf[128];
    int len;

    mimi_disable_irq();

    len = mimi_snprintf(buf, sizeof(buf),
                        "(%s) assert failed at %s:%d !!!\n",
                        x, file, (int)line);
    if (len > 0) {
        if (len >= (int)sizeof(buf)) {
            len = (int)sizeof(buf) - 1;
        }
        mimi_assert_output(buf, (size_t)len);
    }

    while (1) {}
}

#endif
