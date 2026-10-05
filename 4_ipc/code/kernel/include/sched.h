#ifndef __MIMI_SCHED__
#define __MIMI_SCHED__

#include "mimi.h"
#include "thread.h"

mimi_err mimi_sched_init(void);
void mimi_sched_run(void);
void mimi_sched_tick_increase(uint32_t tick);

mimi_tcb *mimi_thread_current(void);

void mimi_sched_join(mimi_tcb *thread);
void mimi_sched_detach(mimi_tcb *thread);

void mimi_schedule(void);
void mimi_schedule_rr(void);

#endif  // __MIMI_SCHED__
