#include "mimi_string.h"
#include <stdint.h>
#include <string.h>
#include "mimi.h"

#define STR_BULK_COPY   16U     /* above this, libc does it faster */

typedef struct {
    char   *buf;
    size_t  cap;    /* usable bytes, excluding the terminating NUL */
    size_t  pos;    /* bytes that would have been written              */
} str_out;

mimi_inline void str_char(str_out *o, char c)
{
    if (o->pos < o->cap) {
        o->buf[o->pos] = c;
    }
    o->pos++;
}

mimi_inline void str_mem(str_out *o, const char *s, size_t n)
{
    size_t room;
    size_t w;

    if (n == 0) {
        return;
    }
    room = o->pos < o->cap ? o->cap - o->pos : 0;
    w = n < room ? n : room;            /* w <= n, keeps the copy bounded */
    {
        char *d = o->buf + o->pos;

        if (w >= STR_BULK_COPY) {
            memcpy(d, s, w);
        } else {
            for (size_t i = 0; i < w; i++) {
                d[i] = s[i];
            }
        }
    }
    o->pos += n;
}

mimi_inline size_t str_len(const char *s)
{
    const char *p = s;

    while (*p != '\0') {
        p++;
    }
    return (size_t)(p - s);
}

/* v is emitted reversed: the caller appends it back to front. */
static void str_number(str_out *o, unsigned long value, int negative,
                       unsigned int base, int upper)
{
    static const char lower_digits[] = "0123456789abcdef";
    static const char upper_digits[] = "0123456789ABCDEF";
    const char *digits = upper ? upper_digits : lower_digits;
    char tmp[32];
    int n = 0;

    if (value == 0UL) {
        tmp[n++] = '0';
    }
    while (value != 0UL) {
        tmp[n++] = digits[value % base];
        value /= base;
    }

    if (negative) {
        str_char(o, '-');
    }
    while (n > 0) {
        str_char(o, tmp[--n]);
    }
}

int mimi_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    const char *p = fmt;
    str_out out = {
        buf,
        size > 0 ? size - 1 : 0,
        0
    };

    while (*p != '\0') {
        int is_long;
        char conv;

        if (*p != '%') {
            const char *run = p;

            do {
                p++;
            } while (*p != '\0' && *p != '%');
            str_mem(&out, run, (size_t)(p - run));
            continue;
        }
        p++;

        is_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
            if (*p == 'l') {        /* 64-bit values are not used by the log */
                p++;
            }
        }
        conv = *p;
        if (conv == '\0') {
            str_char(&out, '%');
            break;
        }
        p++;

        switch (conv) {
        case '%':
            str_char(&out, '%');
            break;
        case 'c':
            str_char(&out, (char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);

            if (s == NULL) {
                s = "(null)";
            }
            str_mem(&out, s, str_len(s));
            break;
        }
        case 'd':
        case 'i': {
            long value = is_long ? va_arg(ap, long) : (long)va_arg(ap, int);
            unsigned long mag = (unsigned long)value;
            int negative = 0;

            if (value < 0) {
                negative = 1;
                mag = ~mag + 1UL;
            }
            str_number(&out, mag, negative, 10, 0);
            break;
        }
        case 'u': {
            unsigned long value = is_long ? va_arg(ap, unsigned long)
                                          : (unsigned long)va_arg(ap, unsigned int);

            str_number(&out, value, 0, 10, 0);
            break;
        }
        case 'x':
        case 'X': {
            unsigned long value = is_long ? va_arg(ap, unsigned long)
                                          : (unsigned long)va_arg(ap, unsigned int);

            str_number(&out, value, 0, 16, conv == 'X');
            break;
        }
        case 'p': {
            unsigned long value = (unsigned long)(uintptr_t)va_arg(ap, void *);

            str_mem(&out, "0x", 2);
            str_number(&out, value, 0, 16, 0);
            break;
        }
        default:
            str_char(&out, '%');
            str_char(&out, conv);
            break;
        }
    }

    if (size > 0) {
        size_t end = out.pos < out.cap ? out.pos : out.cap;

        buf[end] = '\0';
    }

    return (int)out.pos;
}

int mimi_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int len = mimi_vsnprintf(buf, size, fmt, ap);
    va_end(ap);

    return len;
}
