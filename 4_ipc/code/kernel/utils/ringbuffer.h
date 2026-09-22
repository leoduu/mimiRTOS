#ifndef __MIMI_RINGBUFFER__
#define __MIMI_RINGBUFFER__

#include <stdint.h>
#include "mimi.h"

/*
 * Multi-producer / single-consumer lock-free ring buffer
 *
 * Based on the DPDK ring buffer design:
 *   1. Producers atomically claim write slots via CAS on prod_head.
 *   2. Producers write payload, then commit via CAS on prod_tail.
 *   3. Consumer reads only up to prod_tail (committed data boundary).
 *   4. Consumer advances cons — single-thread, no atomics needed.
 *
 * Requirements:
 *   - ARM Cortex-M3 or later (LDREX/STREX/DMB ISA support)
 *   - capacity MUST be a power of 2
 *   - consumer MUST be single-threaded (non-atomic cons update)
 */

////////////////////////////////////////////////////////////////////////////////
// platform abstraction — barrier / pause primitives

#if defined(__ARM_ARCH) && (__ARM_ARCH >= 7)
  #include "cmsis_compiler.h"
  #ifndef mimi_dmb
    #define mimi_dmb()  __DMB()
  #endif
  #ifndef mimi_nop
    #define mimi_nop()  __NOP()
  #endif
#else
  #ifndef mimi_dmb
    #define mimi_dmb()  __asm__ volatile ("" ::: "memory")
  #endif
  #ifndef mimi_nop
    #define mimi_nop()  __asm__ volatile ("nop")
  #endif
#endif

typedef struct {
    volatile uint32_t prod_head;   /* claimed write-index by producers      */
    volatile uint32_t prod_tail;   /* committed write-index by producers    */
    volatile uint32_t cons;        /* read-index, consumer-only write       */
    uint32_t           capacity;   /* buffer size (power of 2)              */
    uint8_t            buffer[];   /* flexible array, caller-allocated       */
} mimi_ringbuffer;

/**
 * initialise a ring buffer.
 * @param capacity  MUST be a power of 2.
 * @return          valid pointer on success, NULL if alloc fails or
 *                  capacity is not a power of 2.
 */
mimi_err mimi_ringbuffer_init(mimi_ringbuffer *ring, uint32_t capacity,
                              void* buffer);

/**
 * Allocate and initialise a ring buffer on the heap.
 * @param capacity  MUST be a power of 2.
 * @return          valid pointer on success, NULL if alloc fails or
 *                  capacity is not a power of 2.
 *
 * NOTE: malloc() is only safe if called before the scheduler starts.
 *       For RTOS-task-time allocation use static rings or a memory pool.
 */
mimi_ringbuffer *mimi_ringbuffer_create(uint32_t capacity);

/**
 * Write `len` bytes from `buffer` into the ring.
 * @return 0 on success, -1 if insufficient free space.
 * Safe from multiple producer threads or ISRs.
 */
mimi_err mimi_ringbuffer_write(mimi_ringbuffer *ring, const void *buf,
                               uint32_t len);

/**
 * Read up to `len` bytes from the ring into `buf`.
 * @return  number of bytes actually read (0 if the ring is empty).
 *          Never returns more than `len`.
 * MUST only be called from a single consumer context.
 */
uint32_t mimi_ringbuffer_read(mimi_ringbuffer *ring, void *buf,
                              uint32_t len);

/**
 * Free a ring buffer previously created with CreateRingBuffer().
 */
void mimi_ringbuffer_delete(mimi_ringbuffer *ring);

#endif  // __MIMI_RINGBUFFER__
