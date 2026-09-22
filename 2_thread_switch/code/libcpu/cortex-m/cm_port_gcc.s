
  .syntax unified
    .cpu cortex-m4
    .fpu softvfp
    .thumb
    .text


    .global     PendSV_Handler
    .type       PendSV_Handler, %function
PendSV_Handler:
    MRS     R2, PRIMASK
    CPSID   I                       // disable interrupt

    LDR     R1, =from_thread_sp
    LDR     R1, [R1]                // R1 = the address of tcb->sp
    CBZ     R1, switch_to_next      // if null, skip saving

    MRS     R0, PSP
    STMDB   R0!, {R4 - R11}         // save R4-R11
    STR     R0, [R1]                // save updated sp back to tcb->sp

switch_to_next:
    LDR     R1, =to_thread_sp
    LDR     R1, [R1]
    LDR     R1, [R1]
    LDMIA   R1!, {R4 - R11}         // restore R4-R11
    MSR     PSP, R1                 // update PSP

_exit:
    MSR     PRIMASK, R2             // restore interrupt
    ORR     LR, LR, #0x04           // use PSP to return
    BX      LR                      // return to next thread
