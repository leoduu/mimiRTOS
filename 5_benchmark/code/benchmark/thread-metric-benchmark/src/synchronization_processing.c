/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Thread-Metric Component -- Synchronization Processing Test
 *
 * Single-thread semaphore get/put cycle.
 */
#include "tm_api.h"


/* Define the counters used in the demo application... */

volatile unsigned long tm_synchronization_processing_counter;


/* Define the test thread prototypes. */

void tm_synchronization_processing_thread_0_entry(void);


/* Define the reporting thread prototype. */

void tm_synchronization_processing_thread_report(void);


/* Define the initialization prototype. */

void tm_synchronization_processing_initialize(void);


/* Define main entry point. */

void tm_main(void)
{
    /* Initialize the test. */
    tm_initialize(tm_synchronization_processing_initialize);
}


/* Define the synchronization processing test initialization. */

void tm_synchronization_processing_initialize(void)
{
    /* Create thread 0 at priority 10. */
    TM_CHECK(
        tm_thread_create(0, 10, tm_synchronization_processing_thread_0_entry));

    /* Resume thread 0. */
    TM_CHECK(tm_thread_resume(0));

    /* Create a semaphore for the test. */
    TM_CHECK(tm_semaphore_create(0));

    /* Create the reporting thread. It will preempt the other
     * threads and print out the test results.
     */
    TM_CHECK(
        tm_thread_create(5, 2, tm_synchronization_processing_thread_report));
    TM_CHECK(tm_thread_resume(5));
}


/* Define the synchronization processing thread. */
void tm_synchronization_processing_thread_0_entry(void)
{
    int status;

    while (1) {
        /* Get the semaphore. */
        status = tm_semaphore_get(0);

        /* Check for semaphore get error. */
        if (status != TM_SUCCESS)
            break;

        /* Release the semaphore. */
        status = tm_semaphore_put(0);

        /* Check for semaphore put error. */
        if (status != TM_SUCCESS)
            break;

        /* Increment the number of semaphore get/puts. */
        tm_synchronization_processing_counter++;
    }
}


/* Define the synchronization test reporting thread. */
void tm_synchronization_processing_thread_report(void)
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
            "**** Thread-Metric Synchronization Processing Test **** Relative "
            "Time: %lu\n",
            relative_time);

        /* See if there are any errors. */
        if (tm_synchronization_processing_counter == last_counter) {
            tm_printf(
                "ERROR: Invalid counter value(s). Error getting/putting "
                "semaphore!\n");
        }

        /* Show the time period total. */
        tm_printf("Time Period Total:  %lu\n\n",
                  tm_synchronization_processing_counter - last_counter);

        /* Save the last counter. */
        last_counter = tm_synchronization_processing_counter;
    }

    TM_REPORT_FINISH;
}
