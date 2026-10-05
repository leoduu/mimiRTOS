/*
 * RTOSBench porting layer for mimimi on Nucleo-L452RE (STM32L452, Cortex-M4F).
 *
 * Implements the 28 no_* entry points declared in
 * rtosbench/include/porting_layer.h.  The four no_event_* entry points are
 * not implemented because no test in the upstream suite references them.
 *
 * Timing source is DWT CYCCNT at 80 MHz.  Unlike the Zephyr latency_measure
 * port, RTOSBench resets the counter after every iteration (see
 * no_cycle_reset_counter), so a 32-bit counter survives NB_ITER = 1e6.
 */

#include "porting_layer.h"

#include "stm32l4xx.h"
#include "export.h"
#include "sched.h"
#include "mimi_port.h"

/* ================================================================== */
/*  Console output                                                     */
/* ================================================================== */
/*
 * Direct USART2 register write, for the same reason as the latency_measure
 * port: the mimimi log thread runs at priority 29 and is starved for the
 * whole benchmark run, so anything routed through the log ringbuffer would
 * never reach the wire.  HAL_UART_Transmit() is equally unusable because
 * board.c owns USART2 through the DMA state machine.
 */
static void rtb_putchar(char c)
{
    while ((USART2->ISR & USART_ISR_TXE) == 0U) {
    }
    USART2->TDR = (uint8_t)c;
}

static void rtb_print(const char *s)
{
    while (*s != '\0') {
        rtb_putchar(*s++);
    }
}

static void rtb_print_i64(int64_t v)
{
    char    buf[24];
    int     i = 23;
    int64_t n = v;

    buf[i--] = '\0';
    if (n < 0) {
        rtb_putchar('-');
        n = -n;
    }
    if (n == 0) {
        buf[i--] = '0';
    }
    while (n != 0) {
        buf[i--] = (char)('0' + (int)(n % 10));
        n /= 10;
    }
    rtb_print(&buf[i + 1]);
}

/* ================================================================== */
/*  Cycle counter                                                      */
/* ================================================================== */
static void rtb_dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ================================================================== */
/*  Task storage                                                       */
/* ================================================================== */
/*
 * no_create_task() has no stack argument, but mimi_thread_init() requires
 * the caller to own the stack.  A fixed pool keeps this off the heap.
 */
#define RTB_MAX_TASKS       8
#define RTB_STACK_BYTES     1024

static mimi_tcb  rtb_tasks[RTB_MAX_TASKS];
static uint32_t  rtb_stacks[RTB_MAX_TASKS][RTB_STACK_BYTES / 4];
static int       rtb_task_used;

/* ================================================================== */
/*  Software interrupt                                                 */
/* ================================================================== */
/*
 * IRQ 31 is I2C1_EV on STM32L452.  I2C1 is unused here, and the ST vector
 * table has no generic IRQ31_Handler symbol, so the handler must carry the
 * peripheral name to override the weak Default_Handler alias.
 *
 * Priority 14 keeps it above SysTick and PendSV (both 15 in
 * cortexm_interrupt_init()).
 */
#define RTB_ISR_PRIORITY    14

static no_int_handler_t rtb_isr_handler;

void I2C1_EV_IRQHandler(void)
{
    if (rtb_isr_handler != NULL) {
        rtb_isr_handler(0, NULL);
    }
}

/* ================================================================== */
/*  Porting API                                                        */
/* ================================================================== */
static void rtb_host_entry(void *param);

/*
 * Verification hook, deliberately outside the 28 no_* entry points that
 * porting_layer.h declares: no upstream test calls it, and only the mimiOS
 * port implements it.
 *
 * Masks the SysTick interrupt request while leaving the counter free-
 * running.  sem.c blocks with MIMI_TIMEOUT_FOREVER on both no_sem_wait()
 * and no_task_suspend_self(), so it never arms a timer and never consumes
 * a tick -- for that test the 1 kHz ISR is pure overhead.  PENDSTCLR drops
 * an already-latched tick so the ISR cannot fire once after the mask.
 *
 * Only compiled in when RTB_TICK_DISABLE is defined.  Three tests
 * (mq_workload, mutex_workload, sem_workload) pace themselves with
 * no_task_delay() and would hang without the tick, so this cannot be
 * unconditional.
 *
 * RTB_TICK_MASK selects whether the two stores actually run.  It exists so
 * that rtb_sem_notick (mask=1) and rtb_sem_tickctl (mask=0) compile to
 * byte-identical code and differ only in run-time behaviour; without it the
 * two builds land at different FLASH addresses and the layout itself moves
 * the numbers.  volatile stops -O2 from folding the constant and shrinking
 * the mask=0 build.
 */
#ifndef RTB_TICK_MASK
#define RTB_TICK_MASK 1
#endif

static volatile int rtb_tick_mask = RTB_TICK_MASK;

void no_tick_disable(void)
{
    uint32_t level = mimi_enter_critical();

    if (rtb_tick_mask) {
        SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
        /*
         * 直接赋值, 不能用 |=.  ICSR 是 read-modify-write, bit26 PENDSTSET 读回
         * 的就是 SysTick 的 pending 状态; 一个锁存的 tick 会让 |= 同时写下
         * PENDSTCLR=1 和 PENDSTSET=1, 而 ARM 对这两位同时写 1 的定义是
         * UNPREDICTABLE, "丢掉锁存 tick" 的意图可能失效, 残留一次 ISR.
         * 写 0 到 PENDSTSET 是 no-op, 所以直接赋值等价且确定.
         */
        SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    }

    mimi_exit_critical(level);
}

void no_initialize_test(no_task_entry_t init_test_fct)
{
    rtb_print("[rtb] init begin\r\n");
    init_test_fct(NULL);
    rtb_print("[rtb] init done\r\n");

#ifdef RTB_TICK_DISABLE
    /* Test init is over; nothing from here on needs a periodic tick. */
    no_tick_disable();
#endif

    /*
     * Release the CPU.  mimi_thread_init() joins every new thread to the
     * ready list but never schedules, and mimi_sched_run() has already been
     * called by the time this runs, so nothing starts until the host thread
     * steps aside.
     */
    mimi_thread_suspend(mimi_thread_current(), MIMI_TIMEOUT_FOREVER);
}

no_task_handle_t no_create_task(no_task_entry_t task_entry, char task_name[4], unsigned int prio)
{
    int id;

    if (rtb_task_used >= RTB_MAX_TASKS) {
        return NULL;
    }
    id = rtb_task_used++;

    /*
     * tick_slice = 0xFFFFFFFF disables preemptive round-robin for this
     * thread.  RTOSBench drives the switch points explicitly, so the 1 kHz
     * SysTick must not insert its own rotations.
     */
    mimi_thread_init(&rtb_tasks[id],
                     task_name,
                     (uint8_t)prio,
                     0xFFFFFFFFu,
                     rtb_stacks[id],
                     RTB_STACK_BYTES,
                     (void (*)(void *))task_entry,
                     NULL,
                     NULL);

    return &rtb_tasks[id];
}

void no_task_yield(void)
{
    mimi_thread_yield();
}

void no_task_suspend_self(void)
{
    mimi_thread_delay(MIMI_TIMEOUT_FOREVER);
}

/*
 * Upstream declares nanoseconds but the FreeRTOS example passes
 * vTaskDelay(milliseconds).  mimimi only has tick granularity and
 * OS_TICK_PER_SECOND is 1000, so a tick is a millisecond; the argument is
 * forwarded as-is and documented as milliseconds in the README.
 */
void no_task_delay(unsigned int milliseconds)
{
    mimi_thread_delay(milliseconds);
}

no_time_t no_time_get(void)
{
    return DWT->CYCCNT;
}

long no_time_diff(const no_time_t *t1, const no_time_t *t2)
{
    return (long)((long)*t2 - (long)*t1);
}

no_time_t no_add_times(const no_time_t *base, unsigned int ticks)
{
    return (no_time_t)(*base + ticks);
}

void no_serial_write(const char *string)
{
    rtb_print(string);
}

void no_result_report(int64_t max, int64_t min, int64_t average)
{
    rtb_print("min=");
    rtb_print_i64(min);
    rtb_print(" (");
    rtb_print_i64(TIMER_COUNT_TO_NS(min));
    rtb_print(" ns)  avg=");
    rtb_print_i64(average);
    rtb_print(" (");
    rtb_print_i64(TIMER_COUNT_TO_NS(average));
    rtb_print(" ns)  max=");
    rtb_print_i64(max);
    rtb_print(" (");
    rtb_print_i64(TIMER_COUNT_TO_NS(max));
    rtb_print(" ns)\r\n");
}

void no_single_result_report(char *prefix, int64_t time)
{
    rtb_print(prefix);
    rtb_print_i64(time);
    rtb_putchar('\n');
}

void no_cycle_reset_counter(void)
{
    DWT->CYCCNT = 0;
}

/* ------------------------------------------------------------------ */
/*  semaphore                                                          */
/* ------------------------------------------------------------------ */
void no_sem_create(no_sem_t *sem, int current_value)
{
    /* No practical ceiling: sem.c signals NB_ITER+1 times up front. */
    mimi_sem_init(sem, 0xFFFFFFFFu, (uint32_t)current_value);
}

void no_sem_wait(no_sem_t *sem)
{
    mimi_sem_take(sem, MIMI_TIMEOUT_FOREVER);
}

void no_sem_signal(no_sem_t *sem)
{
    mimi_sem_release(sem);
}

/* ------------------------------------------------------------------ */
/*  mutex                                                              */
/* ------------------------------------------------------------------ */
void no_mutex_create(no_mutex_t *mutex)
{
    mimi_mutex_init(mutex);
}

void no_mutex_acquire(no_mutex_t *mutex)
{
    mimi_mutex_lock(mutex, MIMI_TIMEOUT_FOREVER);
}

void no_mutex_release(no_mutex_t *mutex)
{
    mimi_mutex_unlock(mutex);
}

/* ------------------------------------------------------------------ */
/*  message queue                                                      */
/* ------------------------------------------------------------------ */
/*
 * mq_processing asks for QUEUE_DEPTH=50 x sizeof(int32_t) = 200 bytes, which
 * is the largest request in the suite.  The previous 32-byte pool let
 * mimi_mqueue_init() compute end = buffer + 200 bytes and march off the array;
 * it never actually got there because that test sends and receives in
 * lockstep, but the margin should be real rather than lucky.
 */
static uint32_t rtb_mq_storage[64];

void no_mq_create(no_mq_t *mq, unsigned int length, unsigned int msgsize)
{
    mimi_mqueue_init(mq, msgsize, rtb_mq_storage, length * msgsize);
}

void no_mq_send(no_mq_t *mq, unsigned int msg)
{
    mimi_mqueue_send(mq, &msg, MIMI_MQ_FULL_DROP);
}

void no_mq_receive(no_mq_t *mq)
{
    unsigned int msg;

    mimi_mqueue_recv(mq, &msg, MIMI_TIMEOUT_FOREVER);
}

/* ------------------------------------------------------------------ */
/*  software interrupt                                                 */
/* ------------------------------------------------------------------ */
void no_interrupt_do_sgi(void)
{
    NVIC->ISPR[0] = (1UL << (I2C1_EV_IRQn & 0x1FU));
}

void no_interrupt_enable_sgi(void)
{
    NVIC_SetPriority(I2C1_EV_IRQn, RTB_ISR_PRIORITY);
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_EV_IRQn);
}

void no_interrupt_register_sgi_handler(no_int_handler_t fn)
{
    rtb_isr_handler = fn;
}

/* ================================================================== */
/*  Entry point                                                        */
/* ================================================================== */
/*
 * The test's main() is renamed to rtb_test_main() by config.h.  It has to
 * run in thread context because no_initialize_test() suspends the caller,
 * and MIMI_INIT_APP_EXPORT hooks run before mimi_sched_run() with
 * mimi_thread_current() == NULL.  So the app hook only creates a host
 * thread; that thread calls the test entry point.
 */
#define RTB_HOST_PRIO       (BASE_PRIO + 10)

extern int rtb_test_main(int args);

static mimi_tcb  rtb_host_tcb;
static uint32_t  rtb_host_stack[1024];

static void rtb_host_entry(void *param)
{
    rtb_print("[rtb] host thread running\r\n");
    rtb_test_main(0);
    rtb_print("[rtb] host returned\r\n");

    /* Not reached for the tests that suspend themselves; keeps the thread
     * from returning into the default exit path if one does. */
    mimi_thread_suspend(mimi_thread_current(), MIMI_TIMEOUT_FOREVER);
}

int rtb_app(void)
{
    rtb_print("[rtb] app init\r\n");
    rtb_dwt_init();
    no_interrupt_enable_sgi();

    mimi_thread_init(&rtb_host_tcb,
                     "rtb",
                     RTB_HOST_PRIO,
                     0xFFFFFFFFu,
                     rtb_host_stack,
                     sizeof(rtb_host_stack),
                     rtb_host_entry,
                     NULL,
                     NULL);

    return 0;
}
MIMI_INIT_APP_EXPORT(rtb_app);
