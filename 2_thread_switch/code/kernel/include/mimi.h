#ifndef __MIMI_COMMON__
#define __MIMI_COMMON__

#if defined (__GNUC__)            /* GNU GCC Compiler */

#define mimi_section(x)         __attribute__((section(x)))
#define mimi_aligned(x)         __attribute__((aligned(x)))
#define mimi_used               __attribute__((used))
#define mimi_unreachable        __builtin_unreachable()
#define mimi_weak               __attribute__((weak))

#define static_assert(expr, ...) __static_assert(expr, ##__VA_ARGS__, #expr)
#define __static_assert(expr, msg, ...) _Static_assert(expr, msg)
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

#define container_of(ptr, type, member) ({                                  \
    void *__mptr = (void *)(ptr);                                           \
    static_assert(__same_type(*(ptr), ((type *)0)->member) ||               \
            __same_type(*(ptr), void),                                      \
            "pointer type mismatch in container_of()");                     \
    ((type *)(__mptr - offsetof(type, member))); })

#endif  /* GNU GCC Compiler */

#define mimi_unused(x)                  ((void)(x))
#define mimi_min(x, y)                  ((x) < (y) ? (x) : (y))
#define mimi_max(x, y)                  ((x) > (y) ? (x) : (y))
#define mimi_align_down(size, align)    ((size) & ~((align) - 1))

void mimi_assert_failed(const char *x, const char *file, unsigned int line);
#define mimi_assert(x)                                                      \
    if (!(x)) { mimi_assert_failed(#x, __FILE__, __LINE__); }


typedef enum {
    MIMI_EOK        = 0,
    MIMI_ERROR      = -1,
    MIMI_ETIMEOUT   = -2,
    MIMI_ERESOURCE  = -3,
    MIMI_EPARAMETER = -4,
    MIMI_ENOMEMORY  = -5,
    MIMI_EISR       = -6,
    MIMI_ERESERVED  = 0x7FFFFFFF,
} mimi_err;

typedef enum {
    MIMI_FALSE = 0,
    MIMI_TRUE = 1,
} mimi_bool;

#endif  // __MIMI_COMMON__
