#ifndef __TEST_MOCKS__
#define __TEST_MOCKS__

#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include "mimi.h"
#include "list.h"

/* ---------------------------------------------------------------------- */
/*  Assert capture                                                        */
/* ---------------------------------------------------------------------- */
/* mimi_assert_failed() 是 weak 符号, 默认实现是 while(1){}, 一旦触发测试就
 * 永久卡住。这里给出测试替身: 记录触发次数并 longjmp 回调用点, 让断言可测。
 * 没有 TEST_EXPECT_ASSERT 包裹却触发了断言 = 测试代码自己的问题, 直接 abort。
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

#endif /* __TEST_MOCKS__ */
