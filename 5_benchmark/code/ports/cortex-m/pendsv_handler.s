
    .syntax unified
    .thumb
    .text
    .align 4

    .global     mimi_curr_thread
    .global     mimi_next_thread
    .global     PendSV_Handler
    .type       PendSV_Handler, %function
PendSV_Handler:
    CPSID   I                           // disable interrupt

    LDR     R3, =mimi_curr_thread
    LDR     R1, [R3]                    // current thread tcb
    CBZ     R1, switch_to_next          // if null, skip saving

    MRS     R0, PSP                     // thread stack pointer
    STMDB   R0!, {R4 - R11}             // save registers
    STR     R0, [R1, #8]                // save the thread stack pointer

switch_to_next:
    LDR     R12, =mimi_next_thread
    LDR     R12, [R12]                  // next thread tcb
    LDR     R1, [R12, #8]               // load the thread stack pointer
    LDMIA   R1!, {R4 - R11}             // restore registers
    MSR     PSP, R1                     // update the thread stack pointer

    STR     R12, [R3]                   // mimi_curr_thread = mimi_next_thread

_exit:
    CPSIE   I                           // enable interrupt
    ORR     LR, LR, #0x04               // select PSP as thread stack point
    BX      LR                          // return to next thread
