/*
 * mimimi_os utils — unified test entry
 */

#include <stdio.h>
#include "list.h"

/* Declared in individual test files */
extern int run_list_tests(void);
void setUp(void) { }
void tearDown(void) { }

int main(void)
{
    int failures = 0;

    printf("\n");
    printf("====================================\n");
    printf("  mimimi_os utils — test suite\n");
    printf("====================================\n");

    printf("\n--- list tests ---\n");
    failures += run_list_tests();

    printf("\n====================================\n");
    if (failures == 0) {
        printf("  ALL TESTS PASSED\n");
    } else {
        printf("  TOTAL FAILURES: %d\n", failures);
    }
    printf("====================================\n\n");

    return failures;
}
