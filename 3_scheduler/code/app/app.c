#include <stdio.h>
#include "main.h"
#include "log.h"
#include "usart.h"
#include "gpio.h"
#include "thread.h"
#include "timer.h"
#include "export.h"
#include "mimi.h"

#define STACK_SIZE  1024

/* ==================== 时间片轮转（同级忙等） ====================
 * rr_a / rr_b 同级(2)，纯忙等，不 sleep 不 yield。
 * 靠时间片(默认 10ms)强制切换。每秒打印本轮计数，两者增长率
 * 应接近，证明时间片公平分配 CPU。
 */
mimi_tcb rr_a_thread;
uint8_t rr_a_stack[STACK_SIZE] mimi_aligned(8) = {0};

mimi_tcb rr_b_thread;
uint8_t rr_b_stack[STACK_SIZE] mimi_aligned(8) = {0};

void rr_a_entry(void *param)
{
    uint32_t cnt = 0;
    uint32_t last = mimi_sys_tick();
    while (1) {
        ++cnt;
        if (mimi_sys_tick() - last >= 1000) {
            last += 1000;
            MIMI_LOG_D("[rr_a] %u/s\n", cnt);
            cnt = 0;
        }
    }
}

void rr_b_entry(void *param)
{
    uint32_t cnt = 0;
    uint32_t last = mimi_sys_tick();
    while (1) {
        ++cnt;
        if (mimi_sys_tick() - last >= 1000) {
            last += 1000;
            MIMI_LOG_D("[rr_b] %u/s\n", cnt);
            cnt = 0;
        }
    }
}

/* ==================== 抢占式调度（高优先级打断） ====================
 * preempt 优先级 1，比 rr(2) 高。每 1000ms 醒来打印一次，然后 sleep。
 * sleep 期间 rr 轮转，醒来瞬间抢占。
 */
mimi_tcb preempt_thread;
uint8_t preempt_stack[STACK_SIZE] mimi_aligned(8) = {0};

void preempt_thread_entry(void *param)
{
    uint32_t cnt = 0;
    while (1) {
        MIMI_LOG_D("[preempt] wake %u\n", ++cnt);
        mimi_thread_delay(1000);
    }
}

/* ==================== 软件定时器（周期回调） ====================
 * 周期定时器，每 1000 tick(1s) 触发。回调在 SysTick 中断上下文执行，
 * 只翻转 GPIO，不做耗时操作。
 */
mimi_timer heart_timer;

void heart_timeout(mimi_timer *timer)
{
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

/* ==================== 空闲线程（兜底） ====================
 * idle 由内核 mimi_sched_init 自建，优先级 31 最低，本用例不显式创建。
 * rr 永远就绪，idle 抢不到 CPU，这本身就是 idle 兜底性质的体现。
 * 想观察 idle 接管，把 rr_a/rr_b 入口换成 mimi_thread_delay(10000)，
 * 所有业务线程阻塞后系统仍稳定。
 */

int app_init(void)
{
    mimi_thread_init(&preempt_thread, "preempt", 1, 0, preempt_stack, STACK_SIZE,
                     preempt_thread_entry, NULL, NULL);

    mimi_thread_init(&rr_a_thread, "rr_a", 2, 0, rr_a_stack, STACK_SIZE,
                     rr_a_entry, NULL, NULL);
    mimi_thread_init(&rr_b_thread, "rr_b", 2, 0, rr_b_stack, STACK_SIZE,
                     rr_b_entry, NULL, NULL);

    mimi_timer_init(&heart_timer, heart_timeout);
    mimi_timer_start(&heart_timer, 1000, MIMI_TIMER_PERIODIC);

    return MIMI_EOK;
}
MIMI_INIT_APP_EXPORT(app_init);
