#ifndef __MIMI_RINGBUFFER__
#define __MIMI_RINGBUFFER__

#include <stdint.h>
#include "mimi.h"

/* Packing layout for prod_head / prod_tail. */
#define MIMI_RB_POS_BITS      24
#define MIMI_RB_POS_MASK      ((uint32_t)((1u << MIMI_RB_POS_BITS) - 1))
#define MIMI_RB_CNT_MASK      ((1u << (32 - MIMI_RB_POS_BITS)) - 1)
#define MIMI_RB_CAPACITY_MAX  (1u << (MIMI_RB_POS_BITS - 1))

////////////////////////////////////////////////////////////////////////////////
// platform abstraction — barrier / pause primitives
#if defined(__ARM_ARCH) && (__ARM_ARCH >= 7)
  #include "cmsis_compiler.h"
  #ifndef mimi_dmb
    #define mimi_dmb()  __DMB()
  #endif
#else
  #ifndef mimi_dmb
    #define mimi_dmb()  __asm__ volatile ("" ::: "memory")
  #endif
#endif

typedef struct {
    /* packed {cnt:8, pos:24} */
    volatile uint32_t prod_head;
    volatile uint32_t prod_tail;
    volatile uint32_t cons;
    uint32_t           capacity;
    uint8_t            buffer[];
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
 */
mimi_ringbuffer *mimi_ringbuffer_create(uint32_t capacity);

/**
 * Write `len` bytes from `buffer` into the ring.
 * @return 0 on success, -1 if insufficient free space.
 * Safe from multiple producer threads or ISRs, including the case where a
 * producer is preempted between claiming its slot and committing it.
 */
mimi_err mimi_ringbuffer_write(mimi_ringbuffer *ring, const void *buf,
                               uint32_t len);

/**
 * Read up to `len` bytes from the ring into `buf`.
 * @return  number of bytes actually read (0 if the ring is empty).
 *          Never returns more than `len`.
 */
uint32_t mimi_ringbuffer_read(mimi_ringbuffer *ring, void *buf,
                              uint32_t len);

/**
 * Free a ring buffer previously created with CreateRingBuffer().
 */
void mimi_ringbuffer_delete(mimi_ringbuffer *ring);

#endif  // __MIMI_RINGBUFFER__
