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

    fprintf(stderr, "\n[unexpected assert] (%s) at %s:%u\n", x, file, line);
    fflush(stderr);
    abort();
}
