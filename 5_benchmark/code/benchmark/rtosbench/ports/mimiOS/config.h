#ifndef CONFIG_H_
#define CONFIG_H_

/*
 * RTOSBench porting configuration for mimimi / Nucleo-L452RE.
 *
 * Everything in this file is OS-specific glue.  The test sources under
 * rtosbench/tests/ are shared verbatim by all three RTOS ports.
 */

#include "mimi.h"
#include "thread.h"
#include "ipc.h"

/*
 * Rename the test's entry point.  Each test file declares
 * `no_main_retval_t main(no_main_argument_t)`, which would collide with the
 * board's own main() in bsp/nucleo-l452/Core/Src/main.c.  The rename is done
 * here, not with -D, so it only applies to translation units that include
 * porting_layer.h, i.e. the test sources.
 */
#define main rtb_test_main

/*
 * Base priority for the benchmark tasks.  mimimi allows 0..30 (31 is idle).
 * RTOSBench derives the other priorities from this via
 * NO_DECREASE_TASK_PRIO, so 10 leaves room for BASE_PRIO+2 on the low side
 * and for the RTOSBench host thread further down.
 */
#define BASE_PRIO 10

/*
 * mimimi follows the RT-Thread/Zephyr convention: a *lower* number means a
 * *higher* priority.  Upstream RTOSBench defaults to (prio - decrease_by),
 * which is the FreeRTOS convention.  Getting this backwards silently turns
 * every blocking test into an uncontended one, so it is overridden here.
 */
#define NO_DECREASE_TASK_PRIO(prio, decrease_by) ((prio) + (decrease_by))

/*
 * The port uses one shared software interrupt.  On Cortex-M the NVIC clears
 * the pending bit when the exception is taken, so the generated handler does
 * not have to ack anything.
 */
#define NO_DECLARE_INT_HANDLER(NAME, TIME_CNTR_SUFFIX) \
void NAME(int nIRQ, void *pParam) { \
	WRITE_T2_COUNTER(TIME_CNTR_SUFFIX) \
}

/**
 * Type definitions
 */
typedef void     no_task_retval_t;
typedef void*    no_task_argument_t;
typedef int      no_main_retval_t;
typedef int      no_main_argument_t;
typedef no_task_retval_t (*no_task_entry_t)(no_task_argument_t);
typedef mimi_tcb*         no_task_handle_t;
typedef uint32_t          no_time_t;
typedef mimi_sem          no_sem_t;
typedef mimi_mutex        no_mutex_t;
typedef mimi_sem          no_event_t;
typedef mimi_mqueue       no_mq_t;
typedef void (*no_int_handler_t)(int, void*);

/**
 * Macros
 */
#define INITIALIZATION_TASK_RETVAL int
#define INITIALIZATION_TASK_ARG    void
#define INITIALIZATION_TASK_NAME   main

#define TASK_DEFAULT_RETURN
#define MAIN_DEFAULT_RETURN 0

/* 80 MHz: one cycle is 12.5 ns. */
#define TIMER_COUNT_TO_NS(x) ((int64_t)(x) * 25 / 2)
#define NO_INIT_MAX_TIME_VALUE 0x6FFFFFFF

#endif
