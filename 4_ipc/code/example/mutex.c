
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
