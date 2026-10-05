/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Thread-Metric Component -- Preemptive Scheduling Test
 *
 * Five threads at different priorities doing resume/suspend chains.
 */
#include "tm_api.h"


/* Define the counters used in the demo application... */

volatile unsigned long tm_preemptive_thread_0_counter;
volatile unsigned long tm_preemptive_thread_1_counter;
volatile unsigned long tm_preemptive_thread_2_counter;
volatile unsigned long tm_preemptive_thread_3_counter;
volatile unsigned long tm_preemptive_thread_4_counter;


/* Define the test thread prototypes. */

void tm_preemptive_thread_0_entry(void);
void tm_preemptive_thread_1_entry(void);
void tm_preemptive_thread_2_entry(void);
void tm_preemptive_thread_3_entry(void);
void tm_preemptive_thread_4_entry(void);


/* Define the reporting thread prototype. */

void tm_preemptive_thread_report(void);


/* Define the initialization prototype. */

void tm_preemptive_scheduling_initialize(void);


/* Define main entry point. */

void tm_main(void)
{
    /* Initialize the test. */
    tm_initialize(tm_preemptive_scheduling_initialize);
}


/* Define the preemptive scheduling test initialization. */

void tm_preemptive_scheduling_initialize(void)
{
    /* Create thread 0 at priority 10. */
    TM_CHECK(tm_thread_create(0, 10, tm_preemptive_thread_0_entry));

    /* Create thread 1 at priority 9. */
    TM_CHECK(tm_thread_create(1, 9, tm_preemptive_thread_1_entry));

    /* Create thread 2 at priority 8. */
    TM_CHECK(tm_thread_create(2, 8, tm_preemptive_thread_2_entry));

    /* Create thread 3 at priority 7. */
    TM_CHECK(tm_thread_create(3, 7, tm_preemptive_thread_3_entry));

    /* Create thread 4 at priority 6. */
    TM_CHECK(tm_thread_create(4, 6, tm_preemptive_thread_4_entry));

    /* Resume just thread 0. */
    TM_CHECK(tm_thread_resume(0));

    /* Create the reporting thread. It will preempt the other
     * threads and print out the test results.
     */
    TM_CHECK(tm_thread_create(5, 2, tm_preemptive_thread_report));
    TM_CHECK(tm_thread_resume(5));
}


/* Define the first preemptive thread. */
void tm_preemptive_thread_0_entry(void)
{
    while (1) {
        /* Resume thread 1. */
        tm_thread_resume(1);

        /* We won't get back here until threads 1, 2, 3, and 4 all execute
         * and self-suspend.
         */

        /* Increment this thread's counter. */
        tm_preemptive_thread_0_counter++;
    }
}

/* Define the second preemptive thread. */
void tm_preemptive_thread_1_entry(void)
{
    while (1) {
        /* Resume thread 2. */
        tm_thread_resume(2);

        /* We won't get back here until threads 2, 3, and 4 all execute
         * and self-suspend.
         */

        /* Increment this thread's counter. */
        tm_preemptive_thread_1_counter++;

        /* Suspend self! */
        tm_thread_suspend(1);
    }
}

/* Define the third preemptive thread. */
void tm_preemptive_thread_2_entry(void)
{
    while (1) {
        /* Resume thread 3. */
        tm_thread_resume(3);

        /* We won't get back here until threads 3 and 4 execute and
         * self-suspend.
         */

        /* Increment this thread's counter. */
        tm_preemptive_thread_2_counter++;

        /* Suspend self! */
        tm_thread_suspend(2);
    }
}


/* Define the fourth preemptive thread. */
void tm_preemptive_thread_3_entry(void)
{
    while (1) {
        /* Resume thread 4. */
        tm_thread_resume(4);

        /* We won't get back here until thread 4 executes and
         * self-suspends.
         */

        /* Increment this thread's counter. */
        tm_preemptive_thread_3_counter++;

        /* Suspend self! */
        tm_thread_suspend(3);
    }
}


/* Define the fifth preemptive thread. */
void tm_preemptive_thread_4_entry(void)
{
    while (1) {
        /* Increment this thread's counter. */
        tm_preemptive_thread_4_counter++;

        /* Self suspend thread 4. */
        tm_thread_suspend(4);
    }
}


/* Define the preemptive test reporting thread. */
void tm_preemptive_thread_report(void)
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
            "**** Thread-Metric Preemptive Scheduling Test **** Relative Time: "
            "%lu\n",
            relative_time);

        /* Snapshot counters so the tolerance check uses values consistent with
         * the total (workers keep incrementing).
         */
        c0 = tm_preemptive_thread_0_counter;
        c1 = tm_preemptive_thread_1_counter;
        c2 = tm_preemptive_thread_2_counter;
        c3 = tm_preemptive_thread_3_counter;
        c4 = tm_preemptive_thread_4_counter;

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
                "ERROR: Invalid counter value(s). Preemptive counters should "
                "not be more that 1 different than the average!\n");
        }

        /* Show the time period total. */
        tm_printf("Time Period Total:  %lu\n\n", total - last_total);

        /* Save the last total. */
        last_total = total;
    }

    TM_REPORT_FINISH;
}
