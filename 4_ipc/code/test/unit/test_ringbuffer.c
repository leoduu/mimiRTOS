#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "ringbuffer.h"
#include "test_mocks.h"

/* for multi-producer thread test */
#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
  #ifdef _WIN32
    #include <windows.h>
    typedef HANDLE         mimi_thread;
    #define mimi_thread_create(t, fn, arg)  \
        (*(t) = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(fn), (arg), 0, NULL), *(t) != NULL)
    #define mimi_thread_join(t)             \
        (WaitForSingleObject((t), INFINITE), CloseHandle((t)))
  #else
    #include <pthread.h>
    typedef pthread_t      mimi_thread;
    #define mimi_thread_create(t, fn, arg)  (pthread_create((t), NULL, (fn), (arg)) == 0)
    #define mimi_thread_join(t)             pthread_join((t), NULL)
  #endif
  #define HAS_THREADS  1
#else
  #define HAS_THREADS  0
#endif

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static void fill_pattern(uint8_t *buf, uint32_t len, uint8_t seed)
{
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(seed + i);
    }
}

static int verify_pattern(const uint8_t *buf, uint32_t len, uint8_t seed)
{
    for (uint32_t i = 0; i < len; i++) {
        if (buf[i] != (uint8_t)(seed + i)) {
            return -1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Creation / teardown                                                */
/* ------------------------------------------------------------------ */

static void test_create_power_of_two(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);
    mimi_ringbuffer_delete(r);

    r = mimi_ringbuffer_create(2);
    TEST_ASSERT_NOT_NULL(r);
    mimi_ringbuffer_delete(r);

    r = mimi_ringbuffer_create(256);
    TEST_ASSERT_NOT_NULL(r);
    mimi_ringbuffer_delete(r);
}

static void test_create_non_power_of_two(void)
{
    /* capacity 不是 2 的幂 / 为 0, 现在由断言拦截, 不再返回 NULL */
    TEST_EXPECT_ASSERT(mimi_ringbuffer_create(3));
    TEST_EXPECT_ASSERT(mimi_ringbuffer_create(5));
    TEST_EXPECT_ASSERT(mimi_ringbuffer_create(100));
    TEST_EXPECT_ASSERT(mimi_ringbuffer_create(0));
}

static void test_create_delete_cycle(void)
{
    /* Repeated create/delete — no crash or leak (valgrind check) */
    for (int i = 0; i < 10; i++) {
        mimi_ringbuffer *r = mimi_ringbuffer_create(64);
        TEST_ASSERT_NOT_NULL(r);
        mimi_ringbuffer_delete(r);
    }
}

/* ------------------------------------------------------------------ */
/*  Basic write / read                                                 */
/* ------------------------------------------------------------------ */

static void test_write_read_one(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t byte = 0xAA;
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, &byte, 1));

    uint8_t out;
    TEST_ASSERT_EQUAL_UINT32(1, mimi_ringbuffer_read(r, &out, 1));
    TEST_ASSERT_EQUAL_UINT8(0xAA, out);

    mimi_ringbuffer_delete(r);
}

static void test_write_read_sequential(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t data[32];
    fill_pattern(data, 32, 0x10);

    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, data, 32));

    uint8_t out[32];
    TEST_ASSERT_EQUAL_UINT32(32, mimi_ringbuffer_read(r, out, 32));
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, verify_pattern(out, 32, 0x10));

    mimi_ringbuffer_delete(r);
}

static void test_read_empty_fails(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t out;
    TEST_ASSERT_EQUAL_UINT32(0, mimi_ringbuffer_read(r, &out, 1));

    mimi_ringbuffer_delete(r);
}

static void test_read_more_than_available(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t data = 0x42;
    mimi_ringbuffer_write(r, &data, 1);

    /* Only 1 byte available — read returns that byte, not a failure */
    uint8_t out[10];
    TEST_ASSERT_EQUAL_UINT32(1, mimi_ringbuffer_read(r, out, 10));
    TEST_ASSERT_EQUAL_UINT8(0x42, out[0]);

    /* Buffer is now empty */
    TEST_ASSERT_EQUAL_UINT32(0, mimi_ringbuffer_read(r, out, 1));

    mimi_ringbuffer_delete(r);
}

static void test_write_full_buffer(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t in[64];
    fill_pattern(in, 64, 0xA0);

    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, in, 64));

    uint8_t out[64];
    TEST_ASSERT_EQUAL_UINT32(64, mimi_ringbuffer_read(r, out, 64));
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, verify_pattern(out, 64, 0xA0));

    mimi_ringbuffer_delete(r);
}

static void test_write_full_blocks_writer(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t data[64];
    fill_pattern(data, 64, 0x00);
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, data, 64));

    /* Buffer is full — any extra write must fail */
    uint8_t one = 0xFF;
    TEST_ASSERT_EQUAL_INT(MIMI_ERROR, mimi_ringbuffer_write(r, &one, 1));

    mimi_ringbuffer_delete(r);
}

static void test_write_len_exceeds_capacity(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    /* Single write larger than total capacity — must fail */
    uint8_t big[65];
    TEST_ASSERT_EQUAL_INT(MIMI_ERROR, mimi_ringbuffer_write(r, big, 65));

    /* Buffer should remain empty after the failed write */
    uint8_t out;
    TEST_ASSERT_EQUAL_UINT32(0, mimi_ringbuffer_read(r, &out, 1));

    mimi_ringbuffer_delete(r);
}

/* ------------------------------------------------------------------ */
/*  Wrap-around                                                        */
/* ------------------------------------------------------------------ */

static void test_write_wraparound(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(16);
    TEST_ASSERT_NOT_NULL(r);

    /* Pre-fill to push the write cursor near the end */
    uint8_t pre[10];
    fill_pattern(pre, 10, 0x00);
    mimi_ringbuffer_write(r, pre, 10);

    /* Drain to move read cursor forward (not erasing data) */
    uint8_t tmp[8];
    TEST_ASSERT_EQUAL_UINT32(8, mimi_ringbuffer_read(r, tmp, 8));

    /* Now write 14 bytes — should wrap around */
    uint8_t in[14];
    fill_pattern(in, 14, 0x50);
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, in, 14));

    /* Read leftover pre (2 bytes) + the 14 new bytes */
    uint8_t out[16];
    TEST_ASSERT_EQUAL_UINT32(16, mimi_ringbuffer_read(r, out, 16));

    TEST_ASSERT_EQUAL_UINT8(0x08, out[0]);  /* pre[8] */
    TEST_ASSERT_EQUAL_UINT8(0x09, out[1]);  /* pre[9] */
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, verify_pattern(out + 2, 14, 0x50));

    mimi_ringbuffer_delete(r);
}

static void test_write_wraparound_exact_capacity(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(8);
    TEST_ASSERT_NOT_NULL(r);

    /* Fill: cons=0, prodHead=8, prodTail=8 (buffer full, 8 bytes) */
    uint8_t in[8];
    fill_pattern(in, 8, 0x10);
    mimi_ringbuffer_write(r, in, 8);

    /* Read half: cons=4, 4 bytes free */
    uint8_t out[4];
    TEST_ASSERT_EQUAL_UINT32(4, mimi_ringbuffer_read(r, out, 4));

    /* Write 4 bytes — the free space after draining half */
    fill_pattern(in, 4, 0x80);
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, in, 4));

    /* Read all: 4 remaining old + 4 new = 8 bytes */
    uint8_t all[8];
    TEST_ASSERT_EQUAL_UINT32(8, mimi_ringbuffer_read(r, all, 8));
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, verify_pattern(all, 4, 0x14));    /* old[4..7] */
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, verify_pattern(all + 4, 4, 0x80)); /* new */

    mimi_ringbuffer_delete(r);
}

/* ------------------------------------------------------------------ */
/*  Capacity = 1 edge case                                             */
/* ------------------------------------------------------------------ */

static void test_capacity_one(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(1);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t in = 0xAA;
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, &in, 1));

    uint8_t out;
    TEST_ASSERT_EQUAL_UINT32(1, mimi_ringbuffer_read(r, &out, 1));
    TEST_ASSERT_EQUAL_UINT8(0xAA, out);

    /* Write again after drain */
    in = 0xBB;
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, &in, 1));
    TEST_ASSERT_EQUAL_UINT32(1, mimi_ringbuffer_read(r, &out, 1));
    TEST_ASSERT_EQUAL_UINT8(0xBB, out);

    mimi_ringbuffer_delete(r);
}

static void test_capacity_one_full_blocks(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(1);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t in = 0xCC;
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, &in, 1));
    TEST_ASSERT_EQUAL_INT(MIMI_ERROR, mimi_ringbuffer_write(r, &in, 1));

    mimi_ringbuffer_delete(r);
}

/* ------------------------------------------------------------------ */
/*  Fill-drain cycles                                                  */
/* ------------------------------------------------------------------ */

static void test_fill_drain_cycles(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t seed = 0;
    for (int cycle = 0; cycle < 5; cycle++) {
        /* Write varying amounts */
        uint8_t in[64];
        fill_pattern(in, 64, seed);
        TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, in, 64));

        uint8_t out[64];
        TEST_ASSERT_EQUAL_UINT32(64, mimi_ringbuffer_read(r, out, 64));
        TEST_ASSERT_EQUAL_INT(MIMI_EOK, verify_pattern(out, 64, seed));

        seed += 64;
    }

    mimi_ringbuffer_delete(r);
}

static void test_interleaved_write_read(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(128);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t in[20];
    uint8_t out[20];

    fill_pattern(in, 10, 0x00);
    mimi_ringbuffer_write(r, in, 10);

    fill_pattern(in, 10, 0x10);
    mimi_ringbuffer_write(r, in, 10);

    /* Read partial */
    TEST_ASSERT_EQUAL_UINT32(7, mimi_ringbuffer_read(r, out, 7));
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, verify_pattern(out, 7, 0x00));

    /* Write more */
    fill_pattern(in, 15, 0x30);
    mimi_ringbuffer_write(r, in, 15);

    /* Read everything remaining (3 old-pattern + 10 second + 15 third = 28) */
    uint8_t big[30];
    TEST_ASSERT_EQUAL_UINT32(28, mimi_ringbuffer_read(r, big, 28));
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, verify_pattern(big,      3, 0x07));   /* rest of first */
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, verify_pattern(big + 3, 10, 0x10));   /* second       */
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, verify_pattern(big + 13, 15, 0x30));  /* third        */

    mimi_ringbuffer_delete(r);
}

/* ------------------------------------------------------------------ */
/*  Write repeats with small buffer                                    */
/* ------------------------------------------------------------------ */

static void test_many_small_writes(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    /* Write 256 bytes one at a time, draining 1-byte-at-a-time
     * when full. */
    uint8_t byte;
    uint8_t drain;
    uint32_t written = 0;
    uint32_t readback = 0;

    for (int i = 0; i < 256; i++) {
        byte = (uint8_t)i;
        while (mimi_ringbuffer_write(r, &byte, 1) != MIMI_EOK) {
            /* Drain 1 byte to make room — verify content */
            if (mimi_ringbuffer_read(r, &drain, 1) > 0) {
                TEST_ASSERT_EQUAL_UINT8((uint8_t)readback, drain);
                readback++;
            }
        }
        written++;
    }

    /* Drain remaining — verify content */
    while (mimi_ringbuffer_read(r, &drain, 1) > 0) {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)readback, drain);
        readback++;
    }

    TEST_ASSERT_EQUAL_UINT32(256, written);
    TEST_ASSERT_EQUAL_UINT32(256, readback);

    mimi_ringbuffer_delete(r);
}

/* ------------------------------------------------------------------ */
/*  Multi-producer (thread test, native only)                          */
/*                                                                    */
/*  Uses real OS threads (Win32 CreateThread / pthread) to exercise    */
/*  the lock-free CAS concurrency path.  Tests are guarded by          */
/*  HAS_THREADS and require a native CI environment with thread        */
/*  support.  Deterministic on single-core / low-contention setups;    */
/*  CI should ensure consistent scheduling (e.g., pinned CPUs).       */
/* ------------------------------------------------------------------ */

#if HAS_THREADS

#define MP_CAPACITY    256
#define MP_BYTES_PER_THREAD  64
#define MP_NUM_THREADS       4
#define MP_TOTAL_BYTES  (MP_BYTES_PER_THREAD * MP_NUM_THREADS)

static void *producer_thread(void *arg)
{
    mimi_ringbuffer *r = (mimi_ringbuffer *)arg;
    uint8_t data[MP_BYTES_PER_THREAD];

    for (int i = 0; i < MP_BYTES_PER_THREAD; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }

    uint32_t written = 0;
    while (written < MP_BYTES_PER_THREAD) {
        mimi_err rc = mimi_ringbuffer_write(r, data + written, 1);
        if (rc == MIMI_EOK) {
            written++;
        }
    }

    return NULL;
}

static void test_multi_producer(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(MP_CAPACITY);
    TEST_ASSERT_NOT_NULL(r);

    mimi_thread threads[MP_NUM_THREADS];
    for (int i = 0; i < MP_NUM_THREADS; i++) {
        TEST_ASSERT_TRUE(mimi_thread_create(&threads[i], producer_thread, r));
    }

    /* Consumer: read all bytes */
    uint8_t out[MP_TOTAL_BYTES];
    uint32_t total = 0;
    while (total < MP_TOTAL_BYTES) {
        uint32_t rc = mimi_ringbuffer_read(r, out + total, 1);
        if (rc > 0) {
            total++;
        }
    }

    for (int i = 0; i < MP_NUM_THREADS; i++) {
        mimi_thread_join(threads[i]);
    }

    TEST_ASSERT_EQUAL_UINT32(MP_TOTAL_BYTES, total);

    mimi_ringbuffer_delete(r);
}

#define MI_MP_CAPACITY   64
#define MI_MP_BYTES_PER_THREAD  64
#define MI_MP_THREADS     4
#define MI_MP_BYTES_TOTAL  (MI_MP_THREADS * MI_MP_BYTES_PER_THREAD)

typedef struct {
    mimi_ringbuffer *ring;
    uint8_t          thread_id;
} mi_thread_ctx;

static void *mi_producer_thread(void *arg)
{
    mi_thread_ctx *ctx = (mi_thread_ctx *)arg;
    mimi_ringbuffer *r = ctx->ring;
    uint8_t thread_id = ctx->thread_id;
    free(ctx);

    for (int i = 0; i < MI_MP_BYTES_PER_THREAD; i++) {
        /* Each byte encodes (thread_id << 4) | index */
        uint8_t byte = (uint8_t)((thread_id << 4) | (i & 0x0F));
        while (mimi_ringbuffer_write(r, &byte, 1) != MIMI_EOK) {
            /* spin */
        }
    }
    return NULL;
}

static void test_multi_producer_data_integrity(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(MI_MP_CAPACITY);
    TEST_ASSERT_NOT_NULL(r);

    mimi_thread threads[MI_MP_THREADS];
    for (int i = 0; i < MI_MP_THREADS; i++) {
        mi_thread_ctx *tctx = (mi_thread_ctx *)malloc(sizeof(mi_thread_ctx));
        TEST_ASSERT_NOT_NULL(tctx);
        tctx->ring      = r;
        tctx->thread_id = (uint8_t)i;
        TEST_ASSERT_TRUE(mimi_thread_create(&threads[i], mi_producer_thread,
                                            tctx));
    }

    /* Consumer: read all bytes, classify by thread */
    uint32_t count[MI_MP_THREADS] = {0};
    uint32_t total = 0;
    while (total < MI_MP_BYTES_TOTAL) {
        uint8_t byte;
        if (mimi_ringbuffer_read(r, &byte, 1) > 0) {
            uint8_t tid = byte >> 4;
            uint8_t idx = byte & 0x0F;
            TEST_ASSERT_TRUE(tid < MI_MP_THREADS);
            TEST_ASSERT_EQUAL_UINT8(idx, count[tid] & 0x0F);
            count[tid]++;
            total++;
        }
    }
    for (int i = 0; i < MI_MP_THREADS; i++) {
        mimi_thread_join(threads[i]);
    }

    /* Each thread must have produced exactly MI_MP_BYTES_PER_THREAD */
    for (int i = 0; i < MI_MP_THREADS; i++) {
        TEST_ASSERT_EQUAL_UINT32(MI_MP_BYTES_PER_THREAD, count[i]);
    }

    mimi_ringbuffer_delete(r);
}

#endif /* HAS_THREADS */

/* ------------------------------------------------------------------ */
/*  Zero-length write / read                                           */
/* ------------------------------------------------------------------ */

static void test_write_zero_len(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t dummy = 0;
    TEST_ASSERT_EQUAL_INT(MIMI_EOK, mimi_ringbuffer_write(r, &dummy, 0));

    /* Buffer should still be empty */
    uint8_t out;
    TEST_ASSERT_EQUAL_UINT32(0, mimi_ringbuffer_read(r, &out, 1));

    mimi_ringbuffer_delete(r);
}

static void test_read_zero_len(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t in = 0x55;
    mimi_ringbuffer_write(r, &in, 1);

    uint8_t dummy;
    TEST_ASSERT_EQUAL_UINT32(0, mimi_ringbuffer_read(r, &dummy, 0));

    /* byte should still be there */
    uint8_t out;
    TEST_ASSERT_EQUAL_UINT32(1, mimi_ringbuffer_read(r, &out, 1));
    TEST_ASSERT_EQUAL_UINT8(0x55, out);

    mimi_ringbuffer_delete(r);
}

/* ------------------------------------------------------------------ */
/*  Static init (mimi_ringbuffer_init)                                  */
/* ------------------------------------------------------------------ */

static void test_static_init(void)
{
    /* Allocate a contiguous block: struct + payload buffer.
     * The flexible array `ring->buffer` aliases the tail of this
     * allocation, which matches what mimi_ringbuffer_init expects. */
    struct {
        mimi_ringbuffer ring;
        uint8_t         buf[64];
    } combo;

    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_ringbuffer_init(&combo.ring, 64, combo.buf));

    uint8_t in = 0xAB;
    TEST_ASSERT_EQUAL_INT(MIMI_EOK,
        mimi_ringbuffer_write(&combo.ring, &in, 1));

    uint8_t out;
    TEST_ASSERT_EQUAL_UINT32(1, mimi_ringbuffer_read(&combo.ring, &out, 1));
    TEST_ASSERT_EQUAL_UINT8(0xAB, out);
}

static void test_init_rejects_bad_args(void)
{
    uint8_t buf[64];
    mimi_ringbuffer ring;

    /* 非法入参现在由断言拦截 */
    TEST_EXPECT_ASSERT(mimi_ringbuffer_init(NULL, 64, buf));
    TEST_EXPECT_ASSERT(mimi_ringbuffer_init(&ring, 64, NULL));
    TEST_EXPECT_ASSERT(mimi_ringbuffer_init(&ring, 3, buf));  /* non-power-of-2 */
    TEST_EXPECT_ASSERT(mimi_ringbuffer_init(&ring, 0, buf));
}

/* ------------------------------------------------------------------ */
/*  NULL pointer defense                                                */
/* ------------------------------------------------------------------ */

static void test_null_pointer_defense(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(64);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t byte = 0x42;

    /* write / read 的空指针现在由断言拦截 */
    TEST_EXPECT_ASSERT(mimi_ringbuffer_write(NULL, &byte, 1));
    TEST_EXPECT_ASSERT(mimi_ringbuffer_write(r, NULL, 1));

    TEST_EXPECT_ASSERT(mimi_ringbuffer_read(NULL, &byte, 1));
    TEST_EXPECT_ASSERT(mimi_ringbuffer_read(r, NULL, 1));

    TEST_EXPECT_ASSERT(mimi_ringbuffer_delete(NULL));

    mimi_ringbuffer_delete(r);
}

/* ------------------------------------------------------------------ */
/*  Stress — random write/read sizes                                   */
/* ------------------------------------------------------------------ */

#define STRESS_CAPACITY     256
#define STRESS_IN_SIZE      (STRESS_CAPACITY + 16)  /* +max wlen guard */
#define STRESS_WLEN_MAX     16
#define STRESS_DLEN_MAX     8
#define STRESS_CYCLES       500
#define STRESS_LCG_MULT     1103515245
#define STRESS_LCG_ADD      12345

static void test_stress_random_sizes(void)
{
    mimi_ringbuffer *r = mimi_ringbuffer_create(STRESS_CAPACITY);
    TEST_ASSERT_NOT_NULL(r);

    uint8_t in[STRESS_IN_SIZE];
    uint8_t out[STRESS_DLEN_MAX];
    uint32_t seed    = 0xAB;
    uint32_t written = 0;
    uint32_t drained = 0;

    fill_pattern(in, STRESS_IN_SIZE, 0x00);

    for (int cycle = 0; cycle < STRESS_CYCLES; cycle++) {
        /* Write a random-sized chunk */
        uint32_t wlen = ((seed * STRESS_LCG_MULT + STRESS_LCG_ADD) % STRESS_WLEN_MAX) + 1;
        seed = seed * STRESS_LCG_MULT + STRESS_LCG_ADD;

        int rc = mimi_ringbuffer_write(r, in + (written & 0xFF), wlen);
        if (rc == MIMI_EOK) {
            written += wlen;
        }
        /* If full, that's fine — drain will catch up below */

        /* Drain a few bytes to keep buffer from permanently filling */
        uint32_t dlen = ((seed * STRESS_LCG_MULT + STRESS_LCG_ADD) % STRESS_DLEN_MAX) + 1;
        seed = seed * STRESS_LCG_MULT + STRESS_LCG_ADD;

        for (uint32_t i = 0; i < dlen; i++) {
            if (mimi_ringbuffer_read(r, out, 1) > 0) {
                TEST_ASSERT_EQUAL_UINT8(drained & 0xFF, out[0]);
                drained++;
            } else {
                break;  /* buffer empty */
            }
        }
    }

    /* Drain remaining — verify content */
    uint8_t tail;
    while (mimi_ringbuffer_read(r, &tail, 1) > 0) {
        TEST_ASSERT_EQUAL_UINT8(drained & 0xFF, tail);
        drained++;
    }

    TEST_ASSERT_EQUAL_UINT32(written, drained);

    mimi_ringbuffer_delete(r);
}

/* ------------------------------------------------------------------ */
/*  Runner                                                             */
/* ------------------------------------------------------------------ */

int run_ringbuffer_tests(void)
{
    UNITY_BEGIN();

    /* Creation */
    RUN_TEST(test_create_power_of_two);
    RUN_TEST(test_create_non_power_of_two);
    RUN_TEST(test_create_delete_cycle);

    /* Static init */
    RUN_TEST(test_static_init);
    RUN_TEST(test_init_rejects_bad_args);

    /* Basic write/read */
    RUN_TEST(test_write_read_one);
    RUN_TEST(test_write_read_sequential);
    RUN_TEST(test_read_empty_fails);
    RUN_TEST(test_read_more_than_available);
    RUN_TEST(test_write_full_buffer);
    RUN_TEST(test_write_full_blocks_writer);
    RUN_TEST(test_write_len_exceeds_capacity);

    /* NULL parameter defense */
    RUN_TEST(test_null_pointer_defense);

    /* Wrap-around */
    RUN_TEST(test_write_wraparound);
    RUN_TEST(test_write_wraparound_exact_capacity);

    /* Edge: capacity = 1 */
    RUN_TEST(test_capacity_one);
    RUN_TEST(test_capacity_one_full_blocks);

    /* Fill-drain cycles */
    RUN_TEST(test_fill_drain_cycles);
    RUN_TEST(test_interleaved_write_read);
    RUN_TEST(test_many_small_writes);

    /* Multi-producer */
#if HAS_THREADS
    RUN_TEST(test_multi_producer);
    RUN_TEST(test_multi_producer_data_integrity);
#endif

    /* Zero-length boundary */
    RUN_TEST(test_write_zero_len);
    RUN_TEST(test_read_zero_len);

    /* Stress */
    RUN_TEST(test_stress_random_sizes);

    return UNITY_END();
}
