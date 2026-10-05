
#include "ringbuffer.h"
#include <stdlib.h>
#include <string.h>

/*
 * CAS wrapper.
 * __sync_bool_compare_and_swap provides a full memory barrier (DMB)
 * on both sides on ARM GCC — this is the ordering primitive we rely on.
 */
#define CAS(P, O, V)  __sync_bool_compare_and_swap((size_t *)(P), (O), (V))

/**
 * Available free space (best-effort snapshot).
 * Uses prodHead (claimed slots) so the check is conservative:
 * uncommitted claims are counted as "used".
 */
static uint32_t get_space(mimi_ringbuffer *ring)
{
    return ring->capacity - (ring->prod_head - ring->cons);
}

/**
 * Bytes available for the consumer.
 * Uses prodTail (committed data boundary) — this is the fix.
 * The consumer MUST NOT read past prodTail.
 */
static uint32_t get_exist(mimi_ringbuffer *ring)
{
    return ring->prod_tail - ring->cons;
}

/**
 * Atomically claim `len` bytes in the producer index.
 * Returns 0 on success, -1 if insufficient space.
 * CAS retry loop with space re-check on every iteration.
 */
static mimi_err update_prod_head(mimi_ringbuffer *ring, uint32_t len,
                                 uint32_t *old_head, uint32_t *new_head)
{
    do {
        *old_head = ring->prod_head;

        if (get_space(ring) < len) {
            return MIMI_ERROR;
        }
        *new_head = *old_head + len;
    } while (!CAS(&ring->prod_head, *old_head, *new_head));

    return MIMI_EOK;
}

/**
 * Publish committed data by advancing the tail pointer.
 * Spins until our slot is the next contiguous block to commit
 * (handles out-of-order producer completion).
 */
static void update_prod_tail(mimi_ringbuffer *ring, uint32_t old_head,
                             uint32_t new_head)
{
    while (!CAS(&ring->prod_tail, old_head, new_head)) {
        mimi_nop();
    }
}

mimi_err mimi_ringbuffer_init(mimi_ringbuffer *ring, uint32_t capacity,
                              void *buffer)
{
    mimi_assert(ring != NULL);
    mimi_assert(buffer != NULL);
    mimi_assert(capacity != 0);
    mimi_assert((capacity & (capacity - 1)) == 0);

    memset(buffer, 0, capacity);
    ring->capacity  = capacity;
    ring->prod_head = 0;
    ring->prod_tail = 0;
    ring->cons      = 0;

    return MIMI_EOK;
}

mimi_ringbuffer *mimi_ringbuffer_create(uint32_t capacity)
{
    mimi_assert(capacity != 0);
    mimi_assert((capacity & (capacity - 1)) == 0);

    mimi_ringbuffer *ring =
        (mimi_ringbuffer *)malloc(sizeof(mimi_ringbuffer) + capacity);
    if (ring == NULL) {
        return NULL;
    }

    ring->capacity  = capacity;
    ring->prod_head = 0;
    ring->prod_tail = 0;
    ring->cons      = 0;

    return ring;
}

void mimi_ringbuffer_delete(mimi_ringbuffer *ring)
{
    mimi_assert(ring != 0);

    free(ring);
}

mimi_err mimi_ringbuffer_write(mimi_ringbuffer *ring, const void *buf,
                               uint32_t len)
{
    mimi_assert(ring != NULL);
    mimi_assert(buf != NULL);

    uint32_t old_head;
    uint32_t new_head;

    if (update_prod_head(ring, len, &old_head, &new_head) != MIMI_EOK) {
        return MIMI_ERROR;
    }

    // Slot claimed — write payload, handling wrap-around
    uint32_t head_mask = old_head & (ring->capacity - 1);
    uint32_t left      = ring->capacity - head_mask;

    if (left >= len) {
        memcpy(ring->buffer + head_mask, buf, len);
    } else {
        memcpy(ring->buffer + head_mask, buf, left);
        memcpy(ring->buffer,
               (const uint8_t *)buf + left, len - left);
    }

    /*
     * CAS in UpdateProdTail provides a leading DMB:
     *   memcpy stores  →  DMB(sy)  →  STREX[prodTail]
     *
     * This guarantees the payload is visible to any observer that
     * subsequently sees the updated prodTail.
     */
    update_prod_tail(ring, old_head, new_head);

    return MIMI_EOK;
}

uint32_t mimi_ringbuffer_read(mimi_ringbuffer *ring, void *buf, uint32_t len)
{
    mimi_assert(ring != NULL);
    mimi_assert(buf != NULL);

    uint32_t avail = get_exist(ring);
    if (avail == 0) {
        return 0;
    }
    if (avail > len) {
        avail = len;
    }

    /*
     * DMB: ensure the prod_tail load (in get_exist) is ordered before
     * the buffer payload loads below.
     */
    mimi_dmb();

    uint32_t head_mask = ring->cons & (ring->capacity - 1);
    uint32_t left      = ring->capacity - head_mask;

    if (left >= avail) {
        memcpy(buf, ring->buffer + head_mask, avail);
    } else {
        memcpy(buf, ring->buffer + head_mask, left);
        memcpy((uint8_t *)buf + left, ring->buffer, avail - left);
    }

    ring->cons += avail;

    return avail;
}
