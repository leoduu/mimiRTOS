
#include "ringbuffer.h"
#include <stdlib.h>
#include <string.h>


#define CAS(P, O, V) \
    __sync_bool_compare_and_swap((uint32_t *)(P), (uint32_t)(O), (uint32_t)(V))

static inline uint32_t rb_pos(uint32_t raw)
{
    return raw & MIMI_RB_POS_MASK;
}

static inline uint32_t rb_cnt(uint32_t raw)
{
    return (raw >> MIMI_RB_POS_BITS) & MIMI_RB_CNT_MASK;
}

static inline uint32_t rb_pack(uint32_t pos, uint32_t cnt)
{
    return (pos & MIMI_RB_POS_MASK) |
           ((cnt & MIMI_RB_CNT_MASK) << MIMI_RB_POS_BITS);
}

static inline uint32_t get_space(mimi_ringbuffer *ring)
{
    uint32_t used = (rb_pos(ring->prod_head) - ring->cons) & MIMI_RB_POS_MASK;
    return ring->capacity - used;
}

static inline uint32_t get_exist(mimi_ringbuffer *ring)
{
    return (rb_pos(ring->prod_tail) - ring->cons) & MIMI_RB_POS_MASK;
}

static mimi_err update_prod_head(mimi_ringbuffer *ring, uint32_t len,
                                 uint32_t *old_pos)
{
    uint32_t old_head;   /* packed snapshot of prod_head */
    uint32_t new_head;   /* packed value to publish      */

    do {
        old_head = ring->prod_head;
        if (get_space(ring) < len) {
            return MIMI_ERROR;
        }
        new_head = rb_pack(rb_pos(old_head) + len, rb_cnt(old_head) + 1);

    } while (!CAS(&ring->prod_head, old_head, new_head));

    *old_pos = rb_pos(old_head);
    return MIMI_EOK;
}

static void update_prod_tail(mimi_ringbuffer *ring)
{
    while (1) {
        uint32_t old_tail = ring->prod_tail;
        uint32_t current_head = ring->prod_head;
        uint32_t new_count = (rb_cnt(old_tail) + 1) & MIMI_RB_CNT_MASK;
        uint32_t new_tail  = rb_pack(rb_pos(old_tail), new_count);

        if (new_count == rb_cnt(current_head)) {
            new_tail = rb_pack(rb_pos(current_head), new_count);
        }

        if (CAS(&ring->prod_tail, old_tail, new_tail)) {
            return;
        }
    }
}

mimi_err mimi_ringbuffer_init(mimi_ringbuffer *ring, uint32_t capacity,
                              void *buffer)
{
    mimi_assert(ring != NULL);
    mimi_assert(buffer != NULL);
    mimi_assert(capacity != 0);
    mimi_assert((capacity & (capacity - 1)) == 0);
    mimi_assert(capacity <= MIMI_RB_CAPACITY_MAX);

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
    mimi_assert(capacity <= MIMI_RB_CAPACITY_MAX);

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

    uint32_t old_pos;

    if (update_prod_head(ring, len, &old_pos) != MIMI_EOK) {
        return MIMI_ERROR;
    }

    uint32_t head_mask = old_pos & (ring->capacity - 1);
    uint32_t left      = ring->capacity - head_mask;

    if (left >= len) {
        memcpy(ring->buffer + head_mask, buf, len);
    } else {
        memcpy(ring->buffer + head_mask, buf, left);
        memcpy(ring->buffer, (const uint8_t *)buf + left, len - left);
    }

    /*
     * CAS in update_prod_tail provides a leading DMB:
     *   memcpy stores  →  DMB(sy)  →  STREX[prodTail]
     *
     * This guarantees the payload is visible to any observer that
     * subsequently sees the updated prodTail.
     */
    update_prod_tail(ring);

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
    ring->cons = (ring->cons + avail) & MIMI_RB_POS_MASK;

    return avail;
}
