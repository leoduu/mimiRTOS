/*
 * mimimi_os porting layer for Thread-Metric benchmark suite.
 *
 * Adapted to the current mimiOS kernel API:
 *   - mimi_thread_init(tcb, name, prio, tick_slice, stack, size, entry, param, exit)
 *   - mimi_thread_suspend(tcb, timeout) / mimi_thread_resume(tcb)
 *   - mimi_mqueue_send(mq, buf, flag)   (flag added)
 *   - mimi_sched_run()                  (starts scheduler, never returns)
 *   - mimi_enter_isr() / mimi_exit_isr() (replaces mimi_isr_schedule_check)
 *
 * Design notes vs. the other ports:
 *   - tm_initialize() creates the test objects, then calls mimi_sched_run().
 *     Every Thread-Metric test creates a dedicated *reporting thread* inside
 *     its test_init(), so nothing after tm_initialize() is expected to run.
 *     This mirrors vTaskStartScheduler() / tx_kernel_enter() semantics.
 *   - Priority mapping: TM 1 (highest) -> mimi 0, clamped below the
 *     kernel's own threads (log = MAX-3, console = MAX-2, idle = MAX-1).
 *   - Console output writes USART2->TDR directly.  The mimiOS log thread
 *     runs at THREAD_PRIORITY_MAX-3 and is starved by the never-blocking
 *     benchmark workers, so buffered output would never drain.  HAL's
 *     UART helpers are avoided because board.c owns huart2 via DMA.
 *   - ISR simulation: NVIC IRQ 31 pended by tm_cause_interrupt().  The
 *     handler brackets itself with mimi_enter_isr()/mimi_exit_isr() so the
 *     scheduler's pending-switch logic runs on ISR exit.
 *   - Memory pool: O(1) freelist (identical to the FreeRTOS port).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "stm32l4xx.h"
#include "thread.h"
#include "sched.h"
#include "ipc.h"
#include "mimi.h"
#include "conf.h"
#include "mimi_port.h"
#include "log.h"
#include "tm_api.h"

/* ====================================================================== */
/*  Constants                                                              */
/* ====================================================================== */
#define TM_MIMI_MAX_THREADS     10
#define TM_MIMI_MAX_QUEUES      1
#define TM_MIMI_MAX_SEMAPHORES  1
#define TM_MIMI_MAX_POOLS       1

/* Bytes.  The reporting thread runs tm_printf(), which is deeper than the
 * worker threads; keep these generous enough that the stack is never the
 * reason a benchmark result is wrong.
 */
#define TM_STACK_SIZE           2048
#define TM_QUEUE_DEPTH          10
#define TM_QUEUE_MSG_SIZE       (4 * sizeof(unsigned long))

/* memory pool: O(1) freelist, 128-byte blocks */
#define TM_BLOCK_SIZE           128
#define TM_POOL_SIZE            2048
#define TM_BLOCK_COUNT          (TM_POOL_SIZE / TM_BLOCK_SIZE)

/* Highest mimiOS priority usable by benchmark threads.
 * The kernel reserves MAX-3 (log), MAX-2 (console) and MAX-1 (idle).
 */
#define TM_MIMI_PRIO_CEILING    (THREAD_PRIORITY_MAX - 4)

/* Time slice handed to benchmark threads.  Overridable from the build
 * (-DTM_TICK_SLICE=n) so the two rotation sources -- explicit yield and
 * tick-driven round robin in mimi_sched_tick_increase() -- can be measured
 * independently.  A very large value disables the tick-driven source.
 */
#ifndef TM_TICK_SLICE
#define TM_TICK_SLICE           THREAD_TIME_SLICE_DEFAULT
#endif

/* ====================================================================== */
/*  Thread arrays                                                          */
/* ====================================================================== */
static mimi_tcb   tm_tcb[TM_MIMI_MAX_THREADS];
static uint8_t    tm_stack[TM_MIMI_MAX_THREADS][TM_STACK_SIZE]
    __attribute__((aligned(8)));
static void      (*tm_entry[TM_MIMI_MAX_THREADS])(void);

static void tm_trampoline(void *p)
{
    int id = (int)(uintptr_t)p;
    tm_entry[id]();
    /* Benchmark threads loop forever.  If one ever returns, fall through to
     * mimi_thread_exit_func() (installed by mimi_thread_init) which kills
     * the thread instead of returning into garbage.
     */
}

/* ====================================================================== */
/*  IPC objects                                                           */
/* ====================================================================== */
static mimi_sem     tm_sem[TM_MIMI_MAX_SEMAPHORES];
static mimi_mqueue  tm_mq[TM_MIMI_MAX_QUEUES];
static uint32_t     tm_mq_buf[TM_QUEUE_DEPTH * (TM_QUEUE_MSG_SIZE / 4)];

/* ====================================================================== */
/*  Memory pool                                                           */
/* ====================================================================== */
static uint8_t  tm_pool[TM_MIMI_MAX_POOLS][TM_POOL_SIZE]
    __attribute__((aligned(sizeof(void *))));
static void    *tm_pool_free[TM_MIMI_MAX_POOLS];

/* ====================================================================== */
/*  ISR simulation (NVIC software interrupt)                              */
/* ====================================================================== */
#define NVIC_ISER0 (*(volatile unsigned long *) 0xE000E100UL)
#define NVIC_ISPR0 (*(volatile unsigned long *) 0xE000E200UL)
#define NVIC_IPR7  (*(volatile unsigned long *) 0xE000E41CUL)

/* IRQ number used for software interrupt dispatch.
 *
 * On STM32L452 the ST vector table names handlers after peripherals:
 * IRQ 31 is I2C1_EV_IRQHandler.  There is no generic "IRQ31_Handler"
 * symbol in the table, so the handler below MUST carry the peripheral
 * name to override the weak Default_Handler alias.  I2C1 is unused by
 * this benchmark (only USART2 + DMA1 ch6/ch7 are initialized).
 */
#define TM_ISR_IRQ 31

__attribute__((weak)) void tm_interrupt_handler(void) {}
__attribute__((weak)) void tm_interrupt_preemption_handler(void) {}

/* Benchmark-only interrupt-context marker used by tm_cause_interrupt_sync(). */
static volatile bool tm_benchmark_interrupt_active;

/*
 * Vector-table entry for IRQ 31 (I2C1_EV) — overrides the weak
 * Default_Handler alias in startup_stm32l452xx.s.
 */
void I2C1_EV_IRQHandler(void)
{
    tm_interrupt_handler();
    tm_interrupt_preemption_handler();
}

bool tm_benchmark_interrupt_context_active(void)
{
    return tm_benchmark_interrupt_active;
}

/*
 * Initialize IRQ 31 in the NVIC: set priority and enable.
 * Priority 0xE0 (priority 7 << 5) keeps it below the kernel's own
 * exceptions so PendSV always runs last.
 */
void tm_isr_dispatch_init(void)
{
    NVIC_IPR7 = (NVIC_IPR7 & 0x00FFFFFFUL) | (0xE0UL << 24);
    NVIC_ISER0 = (1UL << TM_ISR_IRQ);
}

/*
 * Trigger a software interrupt by pending IRQ 31.
 */
void tm_cause_interrupt(void)
{
    NVIC_ISPR0 = (1UL << TM_ISR_IRQ);
}

/* Synchronous variant: run the handler in-line on the caller's stack,
 * skipping the NVIC round-trip.  See tm_api.h for the contract.
 *
 * IRQs are masked around the flag + handler so a SysTick landing mid-handler
 * cannot context-switch to a task that observes the flag still set in thread
 * mode.
 */
void tm_cause_interrupt_sync(void)
{
    uint32_t level = mimi_enter_critical();
    tm_benchmark_interrupt_active = true;
    tm_interrupt_handler();
    tm_benchmark_interrupt_active = false;
    mimi_exit_critical(level);
}

/* ====================================================================== */
/*  tm_putchar — direct USART2 register write                             */
/* ====================================================================== */
/*
 * Why not the mimiOS log ringbuffer:
 *   the log thread runs at THREAD_PRIORITY_MAX-3 (29).  Benchmark worker
 *   threads run at 0..2 and never block, so the log thread is starved for
 *   the whole run and nothing buffered would ever reach the wire.
 *
 * Why not HAL_UART_Transmit():
 *   board.c drives USART2 with HAL_UART_Transmit_DMA() plus a completion
 *   semaphore owned by the log thread.  HAL_UART_Transmit() returns
 *   HAL_BUSY whenever huart2.gState is BUSY_TX, silently dropping the
 *   character.  Bypassing the HAL state machine avoids that entirely.
 *
 * Trade-off: this writes TDR behind the HAL's back.  It is safe here only
 * because the log thread is starved for the duration of the benchmark, so
 * no DMA transmit is ever in flight.  Restore a coordinated path if the
 * benchmark is ever changed to let lower-priority threads run.
 */
void tm_putchar(int c)
{
    /* TXE is set by hardware as soon as TDR is empty; this cannot spin
     * forever while the USART clock is running.
     */
    while ((USART2->ISR & USART_ISR_TXE) == 0U) {
    }
    USART2->TDR = (uint8_t)c;
}

/* ====================================================================== */
/*  tm_initialize — create test objects, then start the scheduler         */
/* ====================================================================== */
void tm_initialize(void (*test_init)(void))
{
    /* Enable the software-interrupt path used by tm_cause_interrupt(). */
    tm_isr_dispatch_init();

    /* Let the test create its threads / objects.  Every test also creates
     * its own reporting thread here and resumes it.
     */
    test_init();

    /* TEMP DEBUG: remove after bring-up */
    tm_printf("[tm] init done, starting scheduler\n");

    /* Start the scheduler.  Does not return — the reporting thread owns the
     * benchmark loop from here on.
     */
    mimi_sched_run();
}

/* ====================================================================== */
/*  Priority mapping: TM 1 (highest) -> mimi 0                            */
/* ====================================================================== */
static inline uint8_t tm_map_prio(int tm_prio)
{
    int p = tm_prio - 1;
    if (p < 0) {
        p = 0;
    }
    if (p > TM_MIMI_PRIO_CEILING) {
        p = TM_MIMI_PRIO_CEILING;
    }
    return (uint8_t)p;
}

/* ====================================================================== */
/*  Thread management                                                      */
/* ====================================================================== */
int tm_thread_create(int id, int priority, void (*entry)(void))
{
    if (id < 0 || id >= TM_MIMI_MAX_THREADS || priority < 1 || priority > 31) {
        return TM_ERROR;
    }

    if (entry == NULL) {
        return TM_ERROR;
    }

    tm_entry[id] = entry;

    mimi_err ret = mimi_thread_init(
        &tm_tcb[id], "tm", tm_map_prio(priority), TM_TICK_SLICE,
        tm_stack[id], TM_STACK_SIZE,
        tm_trampoline, (void *)(uintptr_t)id, NULL);

    if (ret != MIMI_EOK) {
        return TM_ERROR;
    }

    /* Create in suspended state (matches ThreadX TX_DONT_START).
     * mimi_thread_init() joins the ready list, so detach again here.
     */
    if (mimi_thread_suspend(&tm_tcb[id], MIMI_TIMEOUT_FOREVER) != MIMI_EOK) {
        return TM_ERROR;
    }

    return TM_SUCCESS;
}

int tm_thread_resume(int id)
{
    if (id < 0 || id >= TM_MIMI_MAX_THREADS) {
        return TM_ERROR;
    }

    /* mimi_thread_resume() is a no-op success when the thread is already
     * READY, so no pre-check here — TM_CHECK() in the tests must not fail
     * just because a thread was resumed twice.
     */
    if (mimi_thread_resume(&tm_tcb[id]) != MIMI_EOK) {
        return TM_ERROR;
    }

    return TM_SUCCESS;
}

int tm_thread_suspend(int id)
{
    if (id < 0 || id >= TM_MIMI_MAX_THREADS)
        return TM_ERROR;

    mimi_tcb *t = &tm_tcb[id];

    if (mimi_thread_suspend(t, MIMI_TIMEOUT_FOREVER) != MIMI_EOK) {
        return TM_ERROR;
    }

    return TM_SUCCESS;
}

void tm_thread_relinquish(void)
{
    mimi_thread_yield();
}

/* TEMP DIAGNOSTIC, enabled by -DTM_DBG_COUNTERS.
 *
 * The report thread is the only caller of tm_thread_sleep(), so dumping here
 * samples the cooperative counters exactly once per reporting period, from the
 * target itself -- no OpenOCD halt, which would stop the core mid-turn and
 * perturb the very rotation being measured.  Remove together with the
 * -DTM_DBG_COUNTERS flag once the drift investigation is closed.
 */
#ifdef TM_DBG_COUNTERS
extern volatile unsigned long tm_cooperative_thread_0_counter;
extern volatile unsigned long tm_cooperative_thread_1_counter;
extern volatile unsigned long tm_cooperative_thread_2_counter;
extern volatile unsigned long tm_cooperative_thread_3_counter;
extern volatile unsigned long tm_cooperative_thread_4_counter;

static void tm_dump_counters(void)
{
    static int seq;
    tm_printf("DBG %d %lu %lu %lu %lu %lu\n", seq++,
              tm_cooperative_thread_0_counter, tm_cooperative_thread_1_counter,
              tm_cooperative_thread_2_counter, tm_cooperative_thread_3_counter,
              tm_cooperative_thread_4_counter);
}
#endif

void tm_thread_sleep(int seconds)
{
#ifdef TM_DBG_COUNTERS
    tm_dump_counters();
#endif
    mimi_thread_delay((uint32_t)seconds * OS_TICK_PER_SECOND);
}

/* ====================================================================== */
/*  Semaphore management                                                   */
/* ====================================================================== */
int tm_semaphore_create(int id)
{
    if (id < 0 || id >= TM_MIMI_MAX_SEMAPHORES) {
        return TM_ERROR;
    }

    /* Start available (count = 1), matching ThreadX initial count. */
    mimi_sem_init(&tm_sem[id], 1, 1);
    return TM_SUCCESS;
}

int tm_semaphore_get(int id)
{
    if (id < 0 || id >= TM_MIMI_MAX_SEMAPHORES) {
        return TM_ERROR;
    }

    return (mimi_sem_take(&tm_sem[id], MIMI_TIMEOUT_NOWAIT) == MIMI_EOK)
               ? TM_SUCCESS : TM_ERROR;
}

int tm_semaphore_put(int id)
{
    if (id < 0 || id >= TM_MIMI_MAX_SEMAPHORES) {
        return TM_ERROR;
    }

    return (mimi_sem_release(&tm_sem[id]) == MIMI_EOK) ? TM_SUCCESS : TM_ERROR;
}

/* ====================================================================== */
/*  Message queue management                                               */
/* ====================================================================== */
int tm_queue_create(int id)
{
    if (id < 0 || id >= TM_MIMI_MAX_QUEUES) {
        return TM_ERROR;
    }

    return (mimi_mqueue_init(&tm_mq[id], TM_QUEUE_MSG_SIZE,
                             tm_mq_buf, sizeof(tm_mq_buf)) == MIMI_EOK)
               ? TM_SUCCESS : TM_ERROR;
}

int tm_queue_send(int id, unsigned long *msg)
{
    if (id < 0 || id >= TM_MIMI_MAX_QUEUES) {
        return TM_ERROR;
    }

    /* Non-blocking: drop when full, matching the timeout-0 contract. */
    return (mimi_mqueue_send(&tm_mq[id], msg, MIMI_MQ_FULL_DROP) == MIMI_EOK)
               ? TM_SUCCESS : TM_ERROR;
}

int tm_queue_receive(int id, unsigned long *msg)
{
    if (id < 0 || id >= TM_MIMI_MAX_QUEUES) {
        return TM_ERROR;
    }

    return (mimi_mqueue_recv(&tm_mq[id], msg, MIMI_TIMEOUT_NOWAIT) == MIMI_EOK)
               ? TM_SUCCESS : TM_ERROR;
}

/* ====================================================================== */
/*  Memory pool — O(1) freelist                                            */
/* ====================================================================== */
int tm_memory_pool_create(int id)
{
    if (id < 0 || id >= TM_MIMI_MAX_POOLS) {
        return TM_ERROR;
    }

    uint8_t *base = tm_pool[id];
    tm_pool_free[id] = (void *)base;

    for (int i = 0; i < TM_BLOCK_COUNT - 1; i++) {
        void **blk = (void **)(base + (unsigned)i * TM_BLOCK_SIZE);
        *blk = (void *)(base + (unsigned)(i + 1) * TM_BLOCK_SIZE);
    }
    *(void **)(base + (unsigned)(TM_BLOCK_COUNT - 1) * TM_BLOCK_SIZE) = NULL;

    return TM_SUCCESS;
}

int tm_memory_pool_allocate(int id, unsigned char **ptr)
{
    if (id < 0 || id >= TM_MIMI_MAX_POOLS) {
        return TM_ERROR;
    }

    void *blk = tm_pool_free[id];
    if (!blk) {
        return TM_ERROR;
    }

    tm_pool_free[id] = *(void **)blk;
    *ptr = (unsigned char *)blk;
    return TM_SUCCESS;
}

int tm_memory_pool_deallocate(int id, unsigned char *ptr)
{
    if (id < 0 || id >= TM_MIMI_MAX_POOLS) {
        return TM_ERROR;
    }

    if (ptr == NULL) {
        return TM_ERROR;
    }

    *(void **)ptr = tm_pool_free[id];
    tm_pool_free[id] = (void *)ptr;
    return TM_SUCCESS;
}
