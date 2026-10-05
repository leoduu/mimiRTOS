/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Thread-Metric Component -- Basic Processing Test
 *
 * Single-thread processing throughput baseline.
 */
#include "tm_api.h"


/* Define the counters used in the demo application... */

volatile unsigned long tm_basic_processing_counter;


/* Test array.  We will just do a series of calculations on the
 * test array to eat up processing bandwidth. The idea is that
 * all RTOSes should produce the same metric here if everything
 * else is equal, e.g. processor speed, memory speed, etc.
 */

volatile unsigned long tm_basic_processing_array[1024];


/* Define the test thread prototypes. */

void tm_basic_processing_thread_0_entry(void);


/* Define the reporting thread prototype. */

void tm_basic_processing_thread_report(void);


/* Define the initialization prototype. */

void tm_basic_processing_initialize(void);


/* Define main entry point. */

void tm_main(void)
{
    /* Initialize the test. */
    tm_initialize(tm_basic_processing_initialize);
}


/* Define the basic processing test initialization. */

void tm_basic_processing_initialize(void)
{
    /* Create thread 0 at priority 10. */
    TM_CHECK(tm_thread_create(0, 10, tm_basic_processing_thread_0_entry));

    /* Resume thread 0. */
    TM_CHECK(tm_thread_resume(0));

    /* Create the reporting thread. It will preempt the other
     * threads and print out the test results.
     */
    TM_CHECK(tm_thread_create(5, 2, tm_basic_processing_thread_report));
    TM_CHECK(tm_thread_resume(5));
}


/* Define the basic processing thread. */
void tm_basic_processing_thread_0_entry(void)
{
    int i;
    unsigned long counter_snapshot;

    /* Initialize the test array. */
    for (i = 0; i < 1024; i++) {
        /* Clear the basic processing array. */
        tm_basic_processing_array[i] = 0;
    }

    while (1) {
        /* Snapshot the counter once per outer iteration so the volatile
         * qualifier does not force a memory reload on every inner-loop
         * access (which would measure memory traffic, not processing).
         */
        counter_snapshot = tm_basic_processing_counter;

        /* Loop through the basic processing array, add the previous
         * contents with the contents of the tm_basic_processing_counter
         * and xor the result with the previous value...   just to eat
         * up some time.
         */
        for (i = 0; i < 1024; i++) {
            /* Update each array entry. */
            tm_basic_processing_array[i] =
                (tm_basic_processing_array[i] + counter_snapshot) ^
                tm_basic_processing_array[i];
        }

        /* Increment the basic processing counter. */
        tm_basic_processing_counter++;
    }
}


/* Define the basic processing reporting thread. */
void tm_basic_processing_thread_report(void)
{
    unsigned long last_counter;
    unsigned long relative_time;


    /* Initialize the last counter. */
    last_counter = 0;

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
            "**** Thread-Metric Basic Single Thread Processing Test **** "
            "Relative Time: %lu\n",
            relative_time);

        /* See if there are any errors. */
        if (tm_basic_processing_counter == last_counter) {
            tm_printf(
                "ERROR: Invalid counter value(s). Basic processing thread "
                "died!\n");
        }

        /* Show the time period total. */
        tm_printf("Time Period Total:  %lu\n\n",
                  tm_basic_processing_counter - last_counter);

        /* Save the last counter. */
        last_counter = tm_basic_processing_counter;
    }

    TM_REPORT_FINISH;
}
