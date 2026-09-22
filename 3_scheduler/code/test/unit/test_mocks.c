#include "test_mocks.h"
#include <stdio.h>
#include <stdlib.h>

/* ====================================================================== */
/*  Assert capture (overrides the weak mimi_assert_failed in mimi.c)      */
/* ====================================================================== */

jmp_buf      assert_jmp_buf;
int          assert_jmp_armed;
int          assert_failed_cnt;
const char  *assert_expr;

void mimi_assert_failed(const char *x, const char *file, uint32_t line)
{
    assert_failed_cnt++;
    assert_expr = x;

    if (assert_jmp_armed) {
        longjmp(assert_jmp_buf, 1);
    }

    /* 没人承接: 这是测试代码自己的 bug, 不能让进程卡死在 while(1) 里 */
    fprintf(stderr, "\n[unexpected assert] (%s) at %s:%u\n", x, file, line);
    fflush(stderr);
    abort();
}

/* ====================================================================== */
/*  Clock                                                                 */
/* ====================================================================== */

uint32_t fake_tick;

uint32_t mimi_sys_tick(void)    { return fake_tick; }

/* ====================================================================== */
/*  IRQ stubs  (single-thread test, no real interrupts)                   */
/* ====================================================================== */

void mimi_disable_irq(void) { }
void mimi_enable_irq(void)  { }

/* ---------------------------------------------------------------------- */
/*  Critical section / spinlock (x86: no real interrupts)                  */
/* ---------------------------------------------------------------------- */
uint32_t mimi_enter_critical(void)           { return 0; }
void     mimi_exit_critical(uint32_t level)  { (void)level; }
