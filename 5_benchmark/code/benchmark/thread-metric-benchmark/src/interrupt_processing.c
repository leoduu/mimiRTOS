/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Thread-Metric Component -- Interrupt Processing Test
 *
 * Software trap -> ISR posts semaphore -> thread picks it up.
 * No preemption from the interrupt handler.
 */
#include "tm_api.h"


/* Define the counters used in the demo application... */

volatile unsigned long tm_interrupt_thread_0_counter;
volatile unsigned long tm_interrupt_handler_counter;


/* Define the test thread prototypes. */

void tm_interrupt_thread_0_entry(void);
void tm_interrupt_handler_entry(void);


/* Define the reporting thread prototype. */

void tm_interrupt_thread_report(void);


/* Define the interrupt handler.  This must be called from the RTOS. */

void tm_interrupt_handler(void);


/* Define the initialization prototype. */

void tm_interrupt_processing_initialize(void);


/* Define main entry point. */

void tm_main(void)
{
    /* Initialize the test. */
    tm_initialize(tm_interrupt_processing_initialize);
}


/* Define the interrupt processing test initialization. */

void tm_interrupt_processing_initialize(void)
{
    /* Create thread that generates the interrupt at priority 10. */
    TM_CHECK(tm_thread_create(0, 10, tm_interrupt_thread_0_entry));

    /* Create a semaphore that will be posted from the interrupt
     * handler.
     */
    TM_CHECK(tm_semaphore_create(0));

    /* Resume just thread 0. */
    TM_CHECK(tm_thread_resume(0));

    /* Create the reporting thread. It will preempt the other
     * threads and print out the test results.
     */
    TM_CHECK(tm_thread_create(5, 2, tm_interrupt_thread_report));
    TM_CHECK(tm_thread_resume(5));
}


/* Define the thread that generates the interrupt. */
void tm_interrupt_thread_0_entry(void)
{
    int status;


    /* Pickup the semaphore since it is initialized to 1 by default. */
    status = tm_semaphore_get(0);

    /* Check for good status. */
    if (status != TM_SUCCESS)
        return;

    while (1) {
        /* Force an interrupt. The underlying RTOS must see that the
         * the interrupt handler is called from the appropriate software
         * interrupt or trap.
         */
        tm_cause_interrupt_sync();

        /* We won't get back here until the interrupt processing is complete,
         * including the setting of the semaphore from the interrupt
         * handler.
         */

        /* Pickup the semaphore set by the interrupt handler. */
        status = tm_semaphore_get(0);

        /* Check for good status. */
        if (status != TM_SUCCESS)
            return;

        /* Increment this thread's counter. */
        tm_interrupt_thread_0_counter++;
    }
}


/* Define the interrupt handler.  This must be called from the RTOS trap
 * handler. To be fair, it must behave just like a processor interrupt, i.e. it
 * must save the full context of the interrupted thread during the preemption
 * processing.
 */
void tm_interrupt_handler(void)
{
    /* Increment the interrupt count. */
    tm_interrupt_handler_counter++;

    /* Put the semaphore from the interrupt handler. */
    tm_semaphore_put(0);
}


/* Define the interrupt test reporting thread. */
void tm_interrupt_thread_report(void)
{
    unsigned long total;
    unsigned long last_total;
    unsigned long relative_time;
    unsigned long average;
    unsigned long ct, ch;

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
            "**** Thread-Metric Interrupt Processing Test **** Relative Time: "
            "%lu\n",
            relative_time);

        /* Snapshot counters for consistent total and tolerance check. */
        ct = tm_interrupt_thread_0_counter;
        ch = tm_interrupt_handler_counter;

        /* Calculate the total of all the counters. */
        total = ct + ch;

        /* Calculate the average of all the counters. */
        average = total / 2;

        /* See if there are any errors.  Skip when average is 0 to
         * avoid unsigned wraparound on (average - 1).
         */
        if (average > 0 && ((ct < (average - 1)) || (ct > (average + 1)) ||
                            (ch < (average - 1)) || (ch > (average + 1)))) {
            tm_printf(
                "ERROR: Invalid counter value(s). Interrupt processing test "
                "has failed!\n");
        }

        /* Show the total interrupts for the time period. */
        tm_printf("Time Period Total:  %lu\n\n", ch - last_total);

        /* Save the last total number of interrupts. */
        last_total = ch;
    }

    TM_REPORT_FINISH;
}
