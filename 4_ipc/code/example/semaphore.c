
#include <stdio.h>
#include "thread.h"
#include "export.h"
#include "log.h"
#include "ipc.h"
#include "sched.h"

#define STACK_SIZE  1024
#define TEST_CNT    5

mimi_tcb sem_prod;
mimi_tcb sem_cons11;
mimi_tcb sem_cons22;

uint8_t sem_prod_stack[STACK_SIZE] mimi_aligned(8) = {0};
uint8_t sem_cons11_stack[STACK_SIZE] mimi_aligned(8) = {0};
uint8_t sem_cons22_stack[STACK_SIZE] mimi_aligned(8) = {0};

mimi_sem sem;

void sem_prod_entry(void *param)
{
    for (int i = 0; i < TEST_CNT; i++) {
        MIMI_LOG_I("\trelease\n");
        mimi_sem_release(&sem);
        mimi_thread_delay(1000);
    }
}

void sem_cons_entry(void *param)
{
    while (1) {
        mimi_sem_take(&sem, MIMI_TIMEOUT_FOREVER);
        MIMI_LOG_I("take\n");
    }
}

int sem_test_init(void)
{
    mimi_thread_init(&sem_cons11, "semCons11", 1, 5,
                     sem_cons11_stack, STACK_SIZE, sem_cons_entry, NULL, NULL);
    mimi_thread_init(&sem_cons22, "semCons22", 1, 5,
                     sem_cons22_stack, STACK_SIZE, sem_cons_entry, NULL, NULL);

    mimi_sem_init(&sem, 2, 0);
    return MIMI_EOK;
}
MIMI_INIT_APP_EXPORT(sem_test_init);

int sem_test(void)
{
    mimi_thread_init(&sem_prod, "semProd", 1, 5,
                     sem_prod_stack, STACK_SIZE, sem_prod_entry, NULL, NULL);

    return MIMI_EOK;
}
MIMI_CMD_EXPORT(semtest, sem_test);
