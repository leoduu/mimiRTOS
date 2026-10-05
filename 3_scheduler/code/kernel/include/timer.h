#ifndef __MIMI_TIMER__
#define __MIMI_TIMER__

#include "mimi.h"
#include "list.h"

#define container_of_timer(ptr)  (container_of(ptr, mimi_timer, node))

struct mimi_timer;
typedef void (*timeout_handler)(struct mimi_timer *timer);

typedef enum {
    MIMI_TIMER_RUNNING,
    MIMI_TIMER_STOP,
} mimi_timer_status;

typedef enum {
    MIMI_TIMER_ONCE      = 0x01,
    MIMI_TIMER_PERIODIC  = 0x02,
} mimi_timer_flag;

typedef struct mimi_timer {
    mimi_node  node;

    uint32_t timeout;
    uint32_t stop_tick;
    uint32_t timeout_tick;
    mimi_timer_status status;
    mimi_timer_flag flag;
    timeout_handler handler;
} mimi_timer;

mimi_err mimi_timer_init(mimi_timer *timer, timeout_handler handler);
mimi_err mimi_timer_start(mimi_timer *timer, uint32_t timeout_tick, mimi_timer_flag flag);
mimi_err mimi_timer_stop(mimi_timer *timer);
mimi_err mimi_timer_resume(mimi_timer *timer);
mimi_err mimi_timer_join(mimi_timer *timer);
mimi_err mimi_timer_detach(mimi_timer *timer);
void mimi_timer_check(void);

#endif  // __MIMI_TIMER__
