/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Thread-Metric Component -- Cooperative Scheduling Test
 *
 * Five equal-priority threads doing round-robin relinquish.
 */
#include "tm_api.h"


/* Define the counters used in the demo application... */

volatile unsigned long tm_cooperative_thread_0_counter;
volatile unsigned long tm_cooperative_thread_1_counter;
volatile unsigned long tm_cooperative_thread_2_counter;
volatile unsigned long tm_cooperative_thread_3_counter;
volatile unsigned long tm_cooperative_thread_4_counter;


/* Define the test thread prototypes. */

void tm_cooperative_thread_0_entry(void);
void tm_cooperative_thread_1_entry(void);
void tm_cooperative_thread_2_entry(void);
void tm_cooperative_thread_3_entry(void);
void tm_cooperative_thread_4_entry(void);


/* Define the reporting thread prototype. */

void tm_cooperative_thread_report(void);


/* Define the initialization prototype. */

void tm_cooperative_scheduling_initialize(void);


/* Define main entry point. */

void tm_main(void)
{
    /* Initialize the test. */
    tm_initialize(tm_cooperative_scheduling_initialize);
}


/* Define the cooperative scheduling test initialization. */

void tm_cooperative_scheduling_initialize(void)
{
    /* Create all 5 threads at priority 3. */
    TM_CHECK(tm_thread_create(0, 3, tm_cooperative_thread_0_entry));
    TM_CHECK(tm_thread_create(1, 3, tm_cooperative_thread_1_entry));
    TM_CHECK(tm_thread_create(2, 3, tm_cooperative_thread_2_entry));
    TM_CHECK(tm_thread_create(3, 3, tm_cooperative_thread_3_entry));
    TM_CHECK(tm_thread_create(4, 3, tm_cooperative_thread_4_entry));

    /* Resume all 5 threads. */
    TM_CHECK(tm_thread_resume(0));
    TM_CHECK(tm_thread_resume(1));
    TM_CHECK(tm_thread_resume(2));
    TM_CHECK(tm_thread_resume(3));
    TM_CHECK(tm_thread_resume(4));

    /* Create the reporting thread. It will preempt the other
     * threads and print out the test results.
     */
    TM_CHECK(tm_thread_create(5, 2, tm_cooperative_thread_report));
    TM_CHECK(tm_thread_resume(5));
}


/* Define the first cooperative thread. */
void tm_cooperative_thread_0_entry(void)
{
    while (1) {
        /* Relinquish to all other threads at same priority. */
        tm_thread_relinquish();

        /* Increment this thread's counter. */
        tm_cooperative_thread_0_counter++;
    }
}

/* Define the second cooperative thread. */
void tm_cooperative_thread_1_entry(void)
{
    while (1) {
        /* Relinquish to all other threads at same priority. */
        tm_thread_relinquish();

        /* Increment this thread's counter. */
        tm_cooperative_thread_1_counter++;
    }
}

/* Define the third cooperative thread. */
void tm_cooperative_thread_2_entry(void)
{
    while (1) {
        /* Relinquish to all other threads at same priority. */
        tm_thread_relinquish();

        /* Increment this thread's counter. */
        tm_cooperative_thread_2_counter++;
    }
}


/* Define the fourth cooperative thread. */
void tm_cooperative_thread_3_entry(void)
{
    while (1) {
        /* Relinquish to all other threads at same priority. */
        tm_thread_relinquish();

        /* Increment this thread's counter. */
        tm_cooperative_thread_3_counter++;
    }
}


/* Define the fifth cooperative thread. */
void tm_cooperative_thread_4_entry(void)
{
    while (1) {
        /* Relinquish to all other threads at same priority. */
        tm_thread_relinquish();

        /* Increment this thread's counter. */
        tm_cooperative_thread_4_counter++;
    }
}


/* Define the cooperative test reporting thread. */
void tm_cooperative_thread_report(void)
{
    unsigned long total;
    unsigned long relative_time;
    unsigned long last_total;
    unsigned long average;
    unsigned long c0, c1, c2, c3, c4;

    /* Initialize the last total. */
    last_total = 0;

    /* Initialize the relative time. */
    relative_time = 0;

    TM_REPORT_LOOP
    {
        /* Sleep to allow the test to run. */
        tm_thread_sleep(tm_test_duration);

        /* Increment the relative time. */
        relative_time = relative_time + tm_test_duration;

        /* Print results to the stdio window. */
        tm_printf(
            "**** Thread-Metric Cooperative Scheduling Test **** Relative "
            "Time: %lu\n",
            relative_time);

        /* Snapshot counters so the tolerance check uses values consistent with
         * the total (workers keep incrementing).
         */
        c0 = tm_cooperative_thread_0_counter;
        c1 = tm_cooperative_thread_1_counter;
        c2 = tm_cooperative_thread_2_counter;
        c3 = tm_cooperative_thread_3_counter;
        c4 = tm_cooperative_thread_4_counter;

        /* Calculate the total of all the counters. */
        total = c0 + c1 + c2 + c3 + c4;

        /* Calculate the average of all the counters. */
        average = total / 5;

        /* See if there are any errors. Skip when average is 0 to avoid unsigned
         * wraparound on (average - 1).
         */
        if (average > 0 && ((c0 < (average - 1)) || (c0 > (average + 1)) ||
                            (c1 < (average - 1)) || (c1 > (average + 1)) ||
                            (c2 < (average - 1)) || (c2 > (average + 1)) ||
                            (c3 < (average - 1)) || (c3 > (average + 1)) ||
                            (c4 < (average - 1)) || (c4 > (average + 1)))) {
            tm_printf(
                "ERROR: Invalid counter value(s). Cooperative counters should "
                "not be more that 1 different than the average!\n");
        }

        /* Show the time period total. */
        tm_printf("Time Period Total:  %lu\n\n", total - last_total);

        /* Save the last total. */
        last_total = total;
    }

    TM_REPORT_FINISH;
}
