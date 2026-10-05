/* Copyright (c) 2019, Guillaume Champagne
 * All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "porting_layer.h"

#ifdef TRACING
#define NO_TEST_ID NOBENCH_TEST_ID_PREEMPT
#endif

#ifndef NB_ITER
#define NB_ITER 5000
#endif

#define NB_TASK 3

no_task_retval_t task(no_task_argument_t args);
no_task_retval_t preempt_initialize_test(no_task_argument_t args);

no_task_handle_t tasks_handle[NB_TASK];
char tasks_name[NB_TASK][5];
no_sem_t sem_high;
no_sem_t sem_mid;

DECLARE_TIME_COUNTERS(no_time_t, _)

volatile int tasks_idx;

no_main_retval_t main(no_main_argument_t args)
{
	no_initialize_test(preempt_initialize_test);
	return MAIN_DEFAULT_RETURN;
}

/*
 * Three tasks, three priorities, one semaphore per hop.
 *
 *   low  --signal(sem_mid)-->  mid  --signal(sem_high)-->  high
 *
 * Every signal immediately preempts the task that issued it, because the
 * woken task always outranks the waker.  Unlike sem.c -- which measures a
 * single wake-up from the signaller's timestamp to the waiter's -- this test
 * spans **two preemption hops**: the counter starts in the lowest priority
 * task and stops in the highest one, so the result covers both context
 * switches plus both wake-up paths, not just one of each.
 *
 * round_robin.c is the opposite case: there every task sits at the same
 * priority and gives up the CPU voluntarily with no_task_yield(), so no
 * preemption happens at all.  Reading the two together separates the cost of
 * "a task let go of the CPU" from "the scheduler took it away".
 */
no_task_retval_t preempt_initialize_test(no_task_argument_t args)
{
	int32_t i;

	tasks_idx = 0;

	no_sem_create(&sem_high, 0);
	no_sem_create(&sem_mid, 0);

	for (i = 0 ; i < NB_TASK; i++)
	{
		tasks_name[i][0] = 72 + i; /* H, M, L -- high, mid, low priority. */
		tasks_name[i][1] = (80 + i) % 255;
		tasks_name[i][2] = (81 + i) % 255;
		tasks_name[i][3] = (82 + i) % 255;
		tasks_name[i][4] = '\0';

		/* NO_DECREASE_TASK_PRIO keeps "lower priority" portable across the
		 * two numbering schemes: priority drops as i grows on every port. */
		tasks_handle[i] = no_create_task(task, tasks_name[i],
				NO_DECREASE_TASK_PRIO(BASE_PRIO, i));
	}
}

no_task_retval_t task(no_task_argument_t args)
{
	int32_t i;
	int32_t local_idx;

	local_idx = tasks_idx++;

	if (local_idx == 0)
	{
		/* Highest priority.  Owns the counters: it is the last hop of the
		 * chain and the only place where the measurement window closes. */
		DECLARE_TIME_STATS(int64_t)

		for (i = 0; i < NB_ITER; i++)
		{
			no_sem_wait(&sem_high);
			WRITE_T2_COUNTER(_)
			COMPUTE_TIME_STATS(_, i);
#ifndef TRACING
			no_cycle_reset_counter();
#endif
		}

		REPORT_BENCHMARK_RESULTS("-- Preemption: 2 hops to high prio --")
	}
	else if (local_idx == 1)
	{
		/* Middle priority.  Preempted by the high task on the way out of
		 * no_sem_signal(), resumes once the high task blocks again. */
		for (i = 0; i < NB_ITER; i++)
		{
			no_sem_wait(&sem_mid);
			no_sem_signal(&sem_high);
		}
	}
	else
	{
		/* Lowest priority.  Opens the window and kicks the chain off. */
		for (i = 0; i < NB_ITER; i++)
		{
			WRITE_T1_COUNTER(_)
			no_sem_signal(&sem_mid);
		}
	}

	no_task_suspend_self();

	return TASK_DEFAULT_RETURN;
}
