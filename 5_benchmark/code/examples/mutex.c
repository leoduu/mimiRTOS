
#include <stdio.h>
#include "thread.h"
#include "export.h"
#include "log.h"
#include "ipc.h"
#include "sched.h"

#define STACK_SIZE  1024
#define TEST_CNT    50

mimi_tcb mtx_thread1;
mimi_tcb mtx_thread2;

uint8_t mtx_thread1_stack[STACK_SIZE] mimi_aligned(8) = {0};
uint8_t mtx_thread2_stack[STACK_SIZE] mimi_aligned(8) = {0};

mimi_mutex mtx;
int shared_cnt = 0;

void mutex_thread_entry(void *param)
{
    for (int i = 0; i < TEST_CNT; i++) {
        mimi_mutex_lock(&mtx, MIMI_TIMEOUT_FOREVER);
        shared_cnt++;
        MIMI_LOG_I("\tcnt:%ld\n", shared_cnt);
        mimi_mutex_unlock(&mtx);
    }
}

int mutex_test_init(void)
{
    mimi_mutex_init(&mtx);
    return MIMI_EOK;
}
MIMI_INIT_APP_EXPORT(mutex_test_init);

int mutex_test(void)
{
    mimi_thread_init(&mtx_thread1, "mutex111", 1, 1, mtx_thread1_stack,
                     STACK_SIZE, mutex_thread_entry, NULL, NULL);
    mimi_thread_init(&mtx_thread2, "mutex222", 1, 1, mtx_thread2_stack,
                     STACK_SIZE, mutex_thread_entry, NULL, NULL);

    return MIMI_EOK;
}
MIMI_CMD_EXPORT(mtxtest, mutex_test);


/* -------------------------------------------------------------------------- */
/*  优先级翻转 / 优先级继承                                                    */
/* -------------------------------------------------------------------------- */
#define PI_PRIO_LOW     20
#define PI_PRIO_MID     15
#define PI_PRIO_HIGH    8

#define PI_CHUNKS       4
#define PI_HALF         (PI_CHUNKS / 2)
#define PI_LOOPS        200000

#define PI_MID_DELAY    50
#define PI_HIGH_DELAY   100

mimi_tcb pi_low;
mimi_tcb pi_mid;
mimi_tcb pi_high;

uint8_t pi_low_stack[STACK_SIZE]  mimi_aligned(8) = {0};
uint8_t pi_mid_stack[STACK_SIZE]  mimi_aligned(8) = {0};
uint8_t pi_high_stack[STACK_SIZE] mimi_aligned(8) = {0};

mimi_mutex pi_mtx;

/*  piLow / piMid 用来对齐节奏, 演示用的节拍器 */
volatile int pi_high_waiting;

/* CPU 密集的任务片, 每片打一条带当前优先级的进度日志 */
static void pi_work(const char *tag, int from, int to)
{
    volatile uint32_t sink = 0;

    for (int c = from; c <= to; c++) {
        for (uint32_t i = 0; i < PI_LOOPS; i++) {
            sink += i;
        }
        MIMI_LOG_I("%s work %d/%d (prio %d)\n", tag, c, PI_CHUNKS,
                   (int)mimi_thread_current()->priority);
    }
}

static void pi_low_entry(void *param)
{
    mimi_mutex_lock(&pi_mtx, MIMI_TIMEOUT_FOREVER);
    MIMI_LOG_I("hold lock (prio %d)\n", (int)mimi_thread_current()->priority);

    pi_work("low", 1, PI_HALF);

    while (!pi_high_waiting) { }

    /* 已经被抬到 piHigh 的优先级, piMid 插不进来了 */
    pi_work("low", PI_HALF + 1, PI_CHUNKS);

    MIMI_LOG_I("release lock (prio %d)\n", (int)mimi_thread_current()->priority);
    mimi_mutex_unlock(&pi_mtx);
}

static void pi_mid_entry(void *param)
{
    mimi_thread_delay(PI_MID_DELAY);

    pi_work("mid", 1, PI_HALF);

    while (!pi_high_waiting) { }

    pi_work("mid", PI_HALF + 1, PI_CHUNKS);
    MIMI_LOG_I("mid done\n");
}

static void pi_high_entry(void *param)
{
    mimi_thread_delay(PI_HIGH_DELAY);

    pi_high_waiting = 1;                /* 先把节拍放出去, 再去撞锁 */
    MIMI_LOG_I("try lock\n");

    mimi_mutex_lock(&pi_mtx, MIMI_TIMEOUT_FOREVER);
    MIMI_LOG_I("got lock (prio %d)\n", (int)mimi_thread_current()->priority);
    mimi_mutex_unlock(&pi_mtx);

    MIMI_LOG_I("high done\n");
}

int mutex_pi_test(void)
{
    pi_high_waiting = 0;
    mimi_mutex_init(&pi_mtx);

    mimi_thread_init(&pi_low,  "piLow",  PI_PRIO_LOW,  5,
                     pi_low_stack,  STACK_SIZE, pi_low_entry,  NULL, NULL);
    mimi_thread_init(&pi_mid,  "piMid",  PI_PRIO_MID,  5,
                     pi_mid_stack,  STACK_SIZE, pi_mid_entry,  NULL, NULL);
    mimi_thread_init(&pi_high, "piHigh", PI_PRIO_HIGH, 5,
                     pi_high_stack, STACK_SIZE, pi_high_entry, NULL, NULL);

    return MIMI_EOK;
}
MIMI_CMD_EXPORT(mtxpitest, mutex_pi_test);
