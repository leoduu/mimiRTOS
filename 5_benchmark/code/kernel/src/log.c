
#include "log.h"
#include <stdarg.h>
#include "sched.h"
#include "thread.h"
#include "ringbuffer.h"
#include "ipc.h"
#include "export.h"
#include "mimi_port.h"
#include "mimi_string.h"

/* -------------------------------------------------------------------------- */
/*  log                                                                       */
/* -------------------------------------------------------------------------- */
#define LOG_THREAD_PRIORITY     (THREAD_PRIORITY_MAX - 3)
#define LOG_THREAD_TIME_SLICE   THREAD_TIME_SLICE_DEFAULT
#define LOG_THREAD_STACK_SIZE   2048
#define LOG_TEMP_BUFFER_SIZE    256

struct {
    mimi_ringbuffer ring;
    uint8_t buffer[MIMI_LOG_BUFFER_SIZE];
} log_ringbuffer;

mimi_sem log_sem;
mimi_tcb log_thread;
uint8_t log_thread_stack[LOG_THREAD_STACK_SIZE] = {0};
uint8_t read_buffer[LOG_TEMP_BUFFER_SIZE];
log_output_func log_output_func_ptr = NULL;

void mimi_log_print_raw(const char *fmt, ...)
{
    uint8_t print_buffer[LOG_TEMP_BUFFER_SIZE];
    va_list args;
    int len;

    va_start(args, fmt);
    len = mimi_vsnprintf((char *)print_buffer, LOG_TEMP_BUFFER_SIZE, fmt, args);
    va_end(args);

    if (len == 0) {
        return;
    }
    if (len >= LOG_TEMP_BUFFER_SIZE) {
        len = LOG_TEMP_BUFFER_SIZE - 1;
    }

    if (mimi_ringbuffer_write(&log_ringbuffer.ring, print_buffer,
                              (uint32_t)len) == MIMI_EOK) {
        mimi_sem_release(&log_sem);
    }
}

void mimi_log_printf(char level, const char *fmt, ...)
{
    uint8_t print_buffer[LOG_TEMP_BUFFER_SIZE];
    va_list args;
    const char *name;
    int len;

    mimi_tcb *tcb = mimi_thread_current();
    if (mimi_is_in_isr()) {
        name = "isr";
    } else if (tcb == NULL) {
        name = "main";
    } else {
        name = tcb->name;
    }

    len = mimi_snprintf((char *)print_buffer, LOG_TEMP_BUFFER_SIZE,
                        "<%ld>[%c][%s]\t", (long)mimi_sys_tick(),
                        (int)level, name);
    if (len >= LOG_TEMP_BUFFER_SIZE) {
        len = LOG_TEMP_BUFFER_SIZE - 1;
    }

    va_start(args, fmt);
    len += mimi_vsnprintf((char *)print_buffer + len,
                          LOG_TEMP_BUFFER_SIZE - (size_t)len, fmt, args);
    va_end(args);

    if (len >= LOG_TEMP_BUFFER_SIZE) {
        len = LOG_TEMP_BUFFER_SIZE - 1;
    }

    if (mimi_ringbuffer_write(&log_ringbuffer.ring, print_buffer,
                              (uint32_t)len) == MIMI_EOK) {
        mimi_sem_release(&log_sem);
    }
}

void mimi_log_kprintf(const char *fmt, ...)
{
    uint8_t print_buffer[LOG_TEMP_BUFFER_SIZE];
    va_list args;
    int len;

    va_start(args, fmt);
    len = mimi_vsnprintf((char *)print_buffer, LOG_TEMP_BUFFER_SIZE, fmt, args);
    va_end(args);

    if (len == 0 || log_output_func_ptr == NULL) {
        return;
    }
    if (len >= LOG_TEMP_BUFFER_SIZE) {
        len = LOG_TEMP_BUFFER_SIZE - 1;
    }

    log_output_func_ptr(print_buffer, (size_t)len);
}

void mimi_log_thread_entry(void *param)
{
    uint32_t len = 0;

    while (1) {
        mimi_sem_take(&log_sem, MIMI_TIMEOUT_FOREVER);
        if (log_output_func_ptr == NULL) {
            continue;
        }

        while ((len = mimi_ringbuffer_read(&log_ringbuffer.ring, read_buffer,
                LOG_TEMP_BUFFER_SIZE)) > 0) {
            log_output_func_ptr(read_buffer, len);
        };
    }
}

void mimi_log_init(void)
{
    mimi_ringbuffer_init(&log_ringbuffer.ring, MIMI_LOG_BUFFER_SIZE,
                         &log_ringbuffer.buffer);

    mimi_sem_init(&log_sem, 1, 0);
    log_output_func_ptr = mimi_log_output_func();
}

int mimi_log_thread_init(void)
{
    mimi_thread_init(&log_thread, "log", LOG_THREAD_PRIORITY,
                    LOG_THREAD_TIME_SLICE, log_thread_stack,
                    LOG_THREAD_STACK_SIZE, mimi_log_thread_entry, NULL, NULL);

    return MIMI_EOK;
}
MIMI_INIT_APP_EXPORT(mimi_log_thread_init);
