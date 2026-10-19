#ifndef __TEST_MOCKS__
#define __TEST_MOCKS__

#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include "mimi.h"
#include "list.h"
#include "thread.h"
#include "timer.h"

/* the kernel's timer list lives in kernel/src/timer.c */
extern mimi_list mimi_timer_list;

/* ---------------------------------------------------------------------- */
/*  Assert capture                                                        */
/* ---------------------------------------------------------------------- */
/* mimi_assert_failed() 是 weak 符号, 默认实现是 while(1){}, 一旦触发测试就
 * 永久卡住。这里给出测试替身: 记录触发次数并 longjmp 回调用点, 让断言可测。
 * 未被 TEST_EXPECT_ASSERT 捕获的断言会直接 abort。
 */
extern jmp_buf      assert_jmp_buf;
extern int          assert_jmp_armed;
extern int          assert_failed_cnt;
extern const char  *assert_expr;

/* 期望 call 触发一次断言; 没触发算失败 */
#define TEST_EXPECT_ASSERT(call)                                          \
    do {                                                                  \
        assert_failed_cnt = 0;                                            \
        assert_jmp_armed  = 1;                                            \
        if (setjmp(assert_jmp_buf) == 0) {                                \
            call;                                                         \
            assert_jmp_armed = 0;                                         \
            TEST_FAIL_MESSAGE("expected an assert, but none fired");      \
        }                                                                 \
        assert_jmp_armed = 0;                                             \
        TEST_ASSERT_EQUAL_INT(1, assert_failed_cnt);                      \
    } while (0)

/* ---------------------------------------------------------------------- */
/*  Clock                                                                 */
/* ---------------------------------------------------------------------- */
extern uint32_t fake_tick;

/* ---------------------------------------------------------------------- */
/*  IRQ stubs                                                             */
/* ---------------------------------------------------------------------- */
void mimi_disable_irq(void);
void mimi_enable_irq(void);

/* ---------------------------------------------------------------------- */
/*  Scheduler stubs                                                       */
/* ---------------------------------------------------------------------- */
extern int  schedule_called;
void mimi_schedule(void);
void mimi_sched_detach(mimi_tcb *t);
void mimi_sched_join(mimi_tcb *t);

/* ---------------------------------------------------------------------- */
/*  Critical section (x86: no real interrupts)                             */
/* ---------------------------------------------------------------------- */
uint32_t mimi_enter_critical(void);
void     mimi_exit_critical(uint32_t level);

/* ---------------------------------------------------------------------- */
/*  Thread stubs — used by mutex / mqueue / sem tests                     */
/* ---------------------------------------------------------------------- */
extern mimi_tcb  tA, tB, tC;

/* sched.h 的 mimi_thread_current() 是 always_inline, 单测里 ipc.c 展开后
 * 直接引用内核符号 mimi_curr_thread (定义在 kernel/src/sched.c), 而 sched.c
 * 不参与单测构建, 所以由 test_mocks.c 提供该符号的定义。 */
extern mimi_tcb *mimi_curr_thread;
#define test_curr_thread mimi_curr_thread
extern int       sleep_called;
extern int       wakeup_called;
extern mimi_tcb *sleep_thread;
extern mimi_list *sleep_list;
extern int       prio_raise_cnt;
extern int       prio_recover_cnt;
extern uint8_t   raised_to;
extern int       test_in_isr;   /* 1 = pretend the caller is in an ISR */

void test_ipc_reset_all(void);

/* mirrors kernel/include/thread.h */
mimi_err mimi_thread_block(mimi_tcb *thread, uint32_t timeout,
                           mimi_list *list);
mimi_err mimi_thread_resume(mimi_tcb *thread);
void     mimi_thread_prio_raise(mimi_tcb *t, uint8_t p);
void     mimi_thread_prio_recover(mimi_tcb *t);

#endif /* __TEST_MOCKS__ */
