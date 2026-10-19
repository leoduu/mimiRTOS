#ifndef __MIMI_STRING__
#define __MIMI_STRING__

#include <stddef.h>
#include <stdarg.h>

/* -------------------------------------------------------------------------- */
/*  minimal reentrant formatter                                               */
/*                                                                            */
/*  No libc formatting calls and no static state: safe to use from an ISR      */
/*  while another thread is formatting.  Supported conversions:                */
/*      %s %c %d %i %u %x %X %p %%                                            */
/*  plus the 'l' length modifier (%ld / %lu / %lx).                           */
/*                                                                            */
/*  Field width, precision and flags are deliberately NOT supported -- they    */
/*  account for most of the code in mpaland/printf and nanoprintf.  An        */
/*  unsupported spec such as "%08X" is copied through literally, so the        */
/*  mistake is visible instead of silently dropping an argument.              */
/*                                                                            */
/*  Returns the number of characters that would have been written and always  */
/*  NUL-terminates when size > 0.  The return value is never negative.        */
/* -------------------------------------------------------------------------- */
int mimi_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int mimi_snprintf(char *buf, size_t size, const char *fmt, ...);

#endif  // __MIMI_STRING__
