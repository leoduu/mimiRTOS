/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Thread-Metric Component -- Application Interface (API)
 *
 * RTOS-neutral API for the Thread-Metric performance test suite.
 * All service prototypes and data structure definitions are here.
 */

#ifndef TM_API_H
#define TM_API_H

/* Determine if a C++ compiler is being used.  If so, ensure that standard
 * C is used to process the API information.
 */

#ifdef __cplusplus

/* Yes, C++ compiler is present.  Use standard C. */
extern "C" {

#endif

/* Define API constants. */

#define TM_SUCCESS 0
#define TM_ERROR 1


/* Define the time interval in seconds. This can be changed with a -D compiler
 * option.
 */

#ifndef TM_TEST_DURATION
#define TM_TEST_DURATION 30
#endif

/* Number of reporting cycles before the test calls exit(0).  Default 0
 * means infinite (original behavior).  Set to 1 for CI / QEMU semihosting
 * runs so the program terminates cleanly via _exit() -> SYS_EXIT.
 */
#ifndef TM_TEST_CYCLES
#define TM_TEST_CYCLES 0
#endif

/* Runtime-configurable test parameters.  Initialised from the
 * compile-time TM_TEST_DURATION / TM_TEST_CYCLES defaults;
 * tm_report_init() may override them from environment variables
 * on hosted (non-semihosting) platforms.
 */
extern int tm_test_duration;
extern int tm_test_cycles;

/* Report helpers and tiny printf implemented in src/tm_report.c.
 * tm_putchar() is the only function each porting layer must supply
 * for console output; tm_printf() calls it internally.
 */
void tm_report_init(void);
void tm_report_init_argv(int argc, char **argv);
void tm_report_finish(void);
void tm_check_fail(const char *msg);
void tm_putchar(int c);
void tm_printf(const char *fmt, ...);

/* Reporter loop helpers -- centralise the bounded-cycle logic so every
 * test file does not duplicate it.  C89 compatible.  Usage:
 *     TM_REPORT_LOOP {
 *         ... sleep, print, check counters ...
 *     } TM_REPORT_FINISH
 */
#define TM_REPORT_LOOP                                                     \
    {                                                                      \
        int _tm_cycle;                                                     \
        for (_tm_cycle = 0; !tm_test_cycles || _tm_cycle < tm_test_cycles; \
             tm_test_cycles ? _tm_cycle++ : 0)

#define TM_REPORT_FINISH \
    }                    \
    tm_report_finish()

/* Init-time check: abort on failure so mis-configured porting layers
 * are caught immediately instead of producing silent hangs.
 */
#define TM_CHECK(call)                                  \
    do {                                                \
        if ((call) != TM_SUCCESS)                       \
            tm_check_fail("FATAL: " #call " failed\n"); \
    } while (0)


/* Define RTOS Neutral APIs. RTOS vendors should fill in the guts of the
 * following API. Once this is done the Thread-Metric tests can be successfully
 * run.
 */

void tm_initialize(void (*test_initialization_function)(void));
int tm_thread_create(int thread_id, int priority, void (*entry_function)(void));
int tm_thread_resume(int thread_id);
int tm_thread_suspend(int thread_id);
void tm_thread_relinquish(void);
void tm_thread_sleep(int seconds);
int tm_queue_create(int queue_id);
int tm_queue_send(int queue_id, unsigned long *message_ptr);
int tm_queue_receive(int queue_id, unsigned long *message_ptr);
int tm_semaphore_create(int semaphore_id);
int tm_semaphore_get(int semaphore_id);
int tm_semaphore_put(int semaphore_id);
int tm_memory_pool_create(int pool_id);
int tm_memory_pool_allocate(int pool_id, unsigned char **memory_ptr);
int tm_memory_pool_deallocate(int pool_id, unsigned char *memory_ptr);

/* Trigger an asynchronous "interrupt" event.
 *
 * Implementations must route execution through the same trap/IRQ mechanism the
 * RTOS uses for real hardware interrupts so that:
 *   - The interrupted thread's context is saved and restored.
 *   - A handler-triggered higher-priority resume causes preemption.
 *   - tm_cause_interrupt() does not return until the handler has run.
 *
 * Used by interrupt_preemption_processing.c, which relies on the
 * preemption side-effect of the handler resuming a higher-priority
 * thread.
 */
void tm_cause_interrupt(void);

/* Synchronous variant: invoke the interrupt handler in-line and return to the
 * caller without going through a trap or scheduler round-trip.
 *
 * The interrupt_processing.c test uses this to measure the cost of the ISR body
 * plus a semaphore handshake without paying the trap entry/exit cost the
 * preemption test specifically measures. Ports typically implement this as a
 * direct call to tm_interrupt_handler(); they MUST NOT post a semaphore to a
 * dedicated ISR thread or pend a software IRQ, because those add a scheduler
 * round-trip that tm_cause_interrupt() already exercises.
 *
 * If the port's tm_* APIs called by the handler dispatch on ISR vs. thread
 * context (e.g. FreeRTOS xSemaphoreGiveFromISR), the port is responsible for
 * ensuring those calls execute safely in thread mode. The FreeRTOS Cortex-M
 * port masks IRQs (PRIMASK) around the inline call so no real interrupt or
 * context switch can race with the benchmark-only ISR-context flag. See
 * ports/freertos/cortex-m/tm_isr_dispatch.c for the reference pattern.
 *
 * Ports with no safe inline path may alias this to tm_cause_interrupt();
 * interrupt_processing then measures the same path as
 * interrupt_preemption_processing, which is still valid (just coarser).
 */
void tm_cause_interrupt_sync(void);


/* Determine if a C++ compiler is being used.  If so, complete the standard
 * C conditional started above.
 */
#ifdef __cplusplus
}
#endif

#endif
