/*
 * mimimi_os utils — unified test entry
 */

#include <stdio.h>
#include "list.h"
#include "test/unit/test_mocks.h"

/* shared setUp / tearDown — all test suites use the same */
extern mimi_list mimi_timer_list;

void setUp(void)
{
    mimi_list_init(&mimi_timer_list);
    fake_tick = 0;
}

void tearDown(void) { }

/* Declared in individual test files */
extern int run_list_tests(void);
extern int run_timer_tests(void);

int main(void)
{
    int failures = 0;

    printf("\n");
    printf("====================================\n");
    printf("  mimimi_os utils — test suite\n");
    printf("====================================\n");

    printf("\n--- list tests ---\n");
    failures += run_list_tests();

    printf("\n--- timer tests ---\n");
    failures += run_timer_tests();

    printf("\n====================================\n");
    if (failures == 0) {
        printf("  ALL TESTS PASSED\n");
    } else {
        printf("  TOTAL FAILURES: %d\n", failures);
    }
    printf("====================================\n\n");

    return failures;
}
