/*
 * mimiOS entry point for Thread-Metric benchmarks.
 *
 * Every test defines tm_main(), so this single shim works for all tests.
 * tm_main() calls tm_initialize(), which creates the test objects and then
 * calls mimi_sched_run().  The scheduler never returns, so everything after
 * tm_main() here is unreachable in practice.
 *
 * Registered via MIMI_INIT_APP_EXPORT: mimi_component_init() runs this
 * before mimi_sched_run(), i.e. with mimi_thread_current() == NULL.  Only
 * object creation and log-buffered output are safe at this point — no
 * blocking kernel calls.
 */

#include "tm_api.h"
#include "export.h"
#include "log.h"

void tm_main(void);

int tm_test(void)
{
    MIMI_LOG_I("tm_test\n");

    tm_report_init();
    tm_printf("Thread-Metric: reporting interval = %d s\n", tm_test_duration);

    tm_main();
    return 0;
}
MIMI_INIT_APP_EXPORT(tm_test);
