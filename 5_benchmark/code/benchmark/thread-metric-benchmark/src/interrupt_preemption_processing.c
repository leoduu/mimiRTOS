/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Thread-Metric Component -- Interrupt Preemption Processing Test
 *
 * Software trap -> ISR resumes higher-priority thread -> preemption.
 */

#include "tm_api.h"


/* Define the counters used in the demo application... */

volatile unsigned long tm_interrupt_preemption_thread_0_counter;
volatile unsigned long tm_interrupt_preemption_thread_1_counter;
volatile unsigned long tm_interrupt_preemption_handler_counter;


/* Define the test thread prototypes. */

void tm_interrupt_preemption_thread_0_entry(void);
void tm_interrupt_preemption_thread_1_entry(void);
void tm_interrupt_preemption_handler_entry(void);


/* Define the reporting thread prototype. */

void tm_interrupt_preemption_thread_report(void);


/* Define the interrupt handler.  This must be called from the RTOS. */

void tm_interrupt_preemption_handler(void);


/* Define the initialization prototype. */

void tm_interrupt_preemption_processing_initialize(void);


/* Define main entry point. */

void tm_main(void)
{
    /* Initialize the test. */
    tm_initialize(tm_interrupt_preemption_processing_initialize);
}


/* Define the interrupt processing test initialization. */

void tm_interrupt_preemption_processing_initialize(void)
{
    /* Create interrupt thread at priority 3. */
    TM_CHECK(tm_thread_create(0, 3, tm_interrupt_preemption_thread_0_entry));

    /* Create thread that generates the interrupt at priority 10. */
    TM_CHECK(tm_thread_create(1, 10, tm_interrupt_preemption_thread_1_entry));

    /* Resume just thread 1. */
    TM_CHECK(tm_thread_resume(1));

    /* Create the reporting thread. It will preempt the other
     * threads and print out the test results.
     */
    TM_CHECK(tm_thread_create(5, 2, tm_interrupt_preemption_thread_report));
    TM_CHECK(tm_thread_resume(5));
}


/* Define the interrupt thread.  This thread is resumed from the
 * interrupt handler.  It runs and suspends.
 */
void tm_interrupt_preemption_thread_0_entry(void)
{
    while (1) {
        /* Increment this thread's counter. */
        tm_interrupt_preemption_thread_0_counter++;

        /* Suspend. This will allow the thread generating the
         * interrupt to run again.
         */
        tm_thread_suspend(0);
    }
}

/* Define the thread that generates the interrupt. */
void tm_interrupt_preemption_thread_1_entry(void)
{
    while (1) {
        /* Force an interrupt. The underlying RTOS must see that the
         * the interrupt handler is called from the appropriate software
         * interrupt or trap.
         */
        tm_cause_interrupt();

        /* We won't get back here until the interrupt processing is complete,
         * including the execution of the higher priority thread made ready
         * by the interrupt.
         */

        /* Increment this thread's counter. */
        tm_interrupt_preemption_thread_1_counter++;
    }
}


/* Define the interrupt handler.  This must be called from the RTOS trap
 * handler. To be fair, it must behave just like a processor interrupt, i.e. it
 * must save the full context of the interrupted thread during the preemption
 * processing.
 */
void tm_interrupt_preemption_handler(void)
{
    /* Increment the interrupt count. */
    tm_interrupt_preemption_handler_counter++;

    /* Resume the higher priority thread from the ISR. */
    tm_thread_resume(0);
}


/* Define the interrupt test reporting thread. */
void tm_interrupt_preemption_thread_report(void)
{
    unsigned long total;
    unsigned long relative_time;
    unsigned long last_total;
    unsigned long average;
    unsigned long c0, c1, ch;

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
            "**** Thread-Metric Interrupt Preemption Processing Test **** "
            "Relative Time: %lu\n",
            relative_time);

        /* Snapshot counters for consistent total and tolerance check. */
        c0 = tm_interrupt_preemption_thread_0_counter;
        c1 = tm_interrupt_preemption_thread_1_counter;
        ch = tm_interrupt_preemption_handler_counter;

        /* Calculate the total of all the counters. */
        total = c0 + c1 + ch;

        /* Calculate the average of all the counters. */
        average = total / 3;

        /* See if there are any errors. Skip when average is 0 to avoid unsigned
         * wraparound on (average - 1).
         */
        if (average > 0 && ((c0 < (average - 1)) || (c0 > (average + 1)) ||
                            (c1 < (average - 1)) || (c1 > (average + 1)) ||
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
