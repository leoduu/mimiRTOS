/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Report helpers and tiny printf for the Thread-Metric test suite.
 *
 * Provides tm_printf() so test sources need no libc <stdio.h>.
 * Each porting layer supplies tm_putchar(int c) as the low-level
 * output bridge.
 */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifndef TM_SEMIHOSTING
#include <unistd.h>
#endif
#include "tm_api.h"

#ifdef TM_SEMIHOSTING
/* Defined in ports/common/cortex-m/tm_putchar.c.  Direct SYS_EXIT
 * semihosting call that bypasses newlib's _exit() and its deep
 * dependency chain (__sinit, _swiopen, etc.).
 */
void tm_semihosting_exit(int code);
#endif

/* Runtime test parameters -- default to compile-time values. */
int tm_test_duration = TM_TEST_DURATION;
int tm_test_cycles = TM_TEST_CYCLES;

/* Allow runtime override via environment variables on hosted
 * platforms.  On bare-metal (semihosting) targets getenv() is
 * unavailable, so the compile-time defaults are the only knob.
 */
void tm_report_init(void)
{
#ifndef TM_SEMIHOSTING
    const char *env;
    char *end;
    long val;

    env = getenv("TM_TEST_DURATION");
    if (env) {
        errno = 0;
        val = strtol(env, &end, 10);
        if (errno == 0 && end != env && *end == '\0' && val > 0 &&
            val <= INT_MAX)
            tm_test_duration = (int) val;
    }

    env = getenv("TM_TEST_CYCLES");
    if (env) {
        errno = 0;
        val = strtol(env, &end, 10);
        if (errno == 0 && end != env && *end == '\0' && val >= 0 &&
            val <= INT_MAX)
            tm_test_cycles = (int) val;
    }
#endif
}

/* On semihosting targets, allow runtime override of --duration=N --cycles=N
 * via argc/argv if the C startup populates them from SYS_GET_CMDLINE.
 * On POSIX targets this is a no-op; env vars are handled by tm_report_init().
 * Note: arm-none-eabi-gcc 15.x rdimon does not populate argc/argv from
 * SYS_GET_CMDLINE, so this path is currently a no-op on Cortex-M QEMU too.
 */
void tm_report_init_argv(int argc, char **argv)
{
#ifdef TM_SEMIHOSTING
    int i;
    long val;
    char *end;
    const char *arg;

    for (i = 1; i < argc; i++) {
        arg = argv[i];
        if (!strncmp(arg, "--duration=", 11)) {
            errno = 0;
            val = strtol(arg + 11, &end, 10);
            if (errno == 0 && end != arg + 11 && *end == '\0' && val > 0 &&
                val <= INT_MAX)
                tm_test_duration = (int) val;
        } else if (!strncmp(arg, "--cycles=", 9)) {
            errno = 0;
            val = strtol(arg + 9, &end, 10);
            if (errno == 0 && end != arg + 9 && *end == '\0' && val >= 0 &&
                val <= INT_MAX)
                tm_test_cycles = (int) val;
        }
    }
#else
    (void) argc;
    (void) argv;
#endif
}

/* Print an unsigned long in decimal via tm_putchar(). */
static void tm_print_unsigned_long(unsigned long val)
{
    char buf[21];
    int i = 0;

    if (val == 0) {
        tm_putchar('0');
        return;
    }

    while (val > 0) {
        buf[i++] = (char) ('0' + (val % 10));
        val /= 10;
    }

    while (i > 0)
        tm_putchar(buf[--i]);
}

/* Print a signed int in decimal via tm_putchar().
 * Negation is done in unsigned to avoid UB on INT_MIN.
 */
static void tm_print_int(int val)
{
    if (val < 0) {
        tm_putchar('-');
        tm_print_unsigned_long((unsigned long) (-(unsigned) val));
    } else {
        tm_print_unsigned_long((unsigned long) val);
    }
}

/* Tiny printf: handles %d, %lu, %s, %%.  C89 / freestanding-safe. */
void tm_printf(const char *fmt, ...)
{
    va_list ap;
    const char *s;

    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            tm_putchar(*fmt++);
            continue;
        }
        fmt++;
        switch (*fmt) {
        case 'l':
            fmt++;
            if (*fmt == 'u') {
                tm_print_unsigned_long(va_arg(ap, unsigned long));
            } else {
                /* Unknown %l_ combo -- print literal. */
                tm_putchar('%');
                tm_putchar('l');
                if (*fmt == '\0')
                    goto done;
                tm_putchar(*fmt);
            }
            break;
        case 'd':
            tm_print_int(va_arg(ap, int));
            break;
        case 's':
            s = va_arg(ap, char *);
            if (s == NULL)
                s = "(null)";
            while (*s)
                tm_putchar(*s++);
            break;
        case '%':
            tm_putchar('%');
            break;
        case '\0':
            goto done;
        default:
            tm_putchar('%');
            tm_putchar(*fmt);
            break;
        }
        fmt++;
    }
done:
    va_end(ap);
}

void tm_report_finish(void)
{
    /* POSIX: exit() flushes stdio and runs atexit handlers (sanitizers
     * register theirs via atexit).  Semihosting: direct SYS_EXIT
     * bypasses newlib's _exit() which pulls in __sinit and file I/O
     * that depend on initialise_monitor_handles (-nostartfiles skips it).
     */
#ifdef TM_SEMIHOSTING
    tm_semihosting_exit(0);
#else
    exit(0);
#endif
}

void tm_check_fail(const char *msg)
{
    while (*msg)
        tm_putchar(*msg++);
    /* See tm_report_finish() for the POSIX vs semihosting rationale. */
#ifdef TM_SEMIHOSTING
    tm_semihosting_exit(1);
#else
    exit(1);
#endif
}
