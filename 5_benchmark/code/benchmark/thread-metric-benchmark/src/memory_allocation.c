/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Thread-Metric Component -- Memory Allocation Test
 *
 * Single-thread 128-byte block allocate/deallocate cycle.
 */
#include "tm_api.h"


/* Define the counters used in the demo application... */

volatile unsigned long tm_memory_allocation_counter;


/* Define the test thread prototypes. */

void tm_memory_allocation_thread_0_entry(void);


/* Define the reporting thread prototype. */

void tm_memory_allocation_thread_report(void);


/* Define the initialization prototype. */

void tm_memory_allocation_initialize(void);


/* Define main entry point. */

void tm_main(void)
{
    /* Initialize the test. */
    tm_initialize(tm_memory_allocation_initialize);
}


/* Define the memory allocation processing test initialization. */

void tm_memory_allocation_initialize(void)
{
    /* Create thread 0 at priority 10. */
    TM_CHECK(tm_thread_create(0, 10, tm_memory_allocation_thread_0_entry));

    /* Resume thread 0. */
    TM_CHECK(tm_thread_resume(0));

    /* Create a memory pool. */
    TM_CHECK(tm_memory_pool_create(0));

    /* Create the reporting thread. It will preempt the other
     * threads and print out the test results.
     */
    TM_CHECK(tm_thread_create(5, 2, tm_memory_allocation_thread_report));
    TM_CHECK(tm_thread_resume(5));
}


/* Define the memory allocation processing thread. */
void tm_memory_allocation_thread_0_entry(void)
{
    int status;
    unsigned char *memory_ptr;


    while (1) {
        /* Allocate memory from pool. */
        status = tm_memory_pool_allocate(0, &memory_ptr);

        /* Check for invalid allocation. */
        if (status != TM_SUCCESS)
            break;

        /* Release the memory back to the pool. */
        status = tm_memory_pool_deallocate(0, memory_ptr);

        /* Check for invalid memory allocation/deallocation. */
        if (status != TM_SUCCESS)
            break;

        /* Increment the number of memory allocations sent and received. */
        tm_memory_allocation_counter++;
    }
}


/* Define the memory allocation test reporting thread. */
void tm_memory_allocation_thread_report(void)
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
            "**** Thread-Metric Memory Allocation Test **** Relative Time: "
            "%lu\n",
            relative_time);

        /* See if there are any errors. */
        if (tm_memory_allocation_counter == last_counter) {
            tm_printf(
                "ERROR: Invalid counter value(s). Error "
                "allocating/deallocating memory!\n");
        }

        /* Show the time period total. */
        tm_printf("Time Period Total:  %lu\n\n",
                  tm_memory_allocation_counter - last_counter);

        /* Save the last counter. */
        last_counter = tm_memory_allocation_counter;
    }

    TM_REPORT_FINISH;
}
