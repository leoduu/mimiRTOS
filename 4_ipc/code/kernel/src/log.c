
#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "sched.h"
#include "thread.h"
#include "ringbuffer.h"
#include "ipc.h"
#include "export.h"
#include "board.h"
#include "cpuport.h"

/* -------------------------------------------------------------------------- */
/*  log                                                                       */
/* -------------------------------------------------------------------------- */
#define LOG_THREAD_PRIORITY     5
#define LOG_THREAD_TIME_SLICE   5
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
log_output_func mimi_log_output_func = NULL;

void mimi_log_print_raw(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    uint8_t print_buffer[LOG_TEMP_BUFFER_SIZE];

    vsnprintf((char *)(&print_buffer), LOG_TEMP_BUFFER_SIZE, fmt, args);
    va_end(args);

    int len = strlen((const char *)print_buffer);
    if (len >= LOG_TEMP_BUFFER_SIZE) {
        return;
    }

    if (mimi_ringbuffer_write(&log_ringbuffer.ring, print_buffer, len) == MIMI_EOK) {
        mimi_sem_release(&log_sem);
    }
}

void mimi_log_printf(char level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    uint8_t print_buffer[LOG_TEMP_BUFFER_SIZE];

    mimi_tcb *tcb = mimi_thread_current();
    char *name;
    if (mimi_is_in_isr()) {
        name = "isr";
    } else {
        if (tcb == NULL) {
            name = "main";
        } else {
            name = tcb->name;
        }
    }

    snprintf((char *)print_buffer, LOG_TEMP_BUFFER_SIZE, "<%ld>[%c][%s]\t",
             mimi_sys_tick(), level, name);
    int len = strlen((const char *)print_buffer);

    vsnprintf((char *)(&print_buffer[len]), LOG_TEMP_BUFFER_SIZE - len, fmt, args);
    va_end(args);

    len = strlen((const char *)print_buffer);
    if (len >= LOG_TEMP_BUFFER_SIZE) {
        return;
    }

    if (mimi_ringbuffer_write(&log_ringbuffer.ring, print_buffer, len) == MIMI_EOK) {
        mimi_sem_release(&log_sem);
    }
}

void mimi_log_kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    uint8_t print_buffer[LOG_TEMP_BUFFER_SIZE];

    vsnprintf((char *)(print_buffer), LOG_TEMP_BUFFER_SIZE, fmt, args);
    va_end(args);

    if (mimi_log_output_func != NULL) {
        mimi_log_output_func(print_buffer, strlen((const char *)print_buffer));
    }
}

void mimi_log_thread_entry(void *param)
{
    uint32_t len = 0;

    while (1) {
        mimi_sem_take(&log_sem, MIMI_TIMEOUT_FOREVER);
        if (mimi_log_output_func == NULL) {
            continue;
        }

        while ((len = mimi_ringbuffer_read(&log_ringbuffer.ring, read_buffer,
                LOG_TEMP_BUFFER_SIZE)) > 0) {
            mimi_log_output_func(read_buffer, len);
        };
    }
}

void mimi_log_init(void)
{
    mimi_ringbuffer_init(&log_ringbuffer.ring, MIMI_LOG_BUFFER_SIZE,
                         &log_ringbuffer.buffer);

    mimi_sem_init(&log_sem, 1, 0);
    mimi_log_output_func = mimi_board_get_log_output_func();
}

void mimi_log_start(void)
{
    mimi_thread_init(&log_thread, "log", LOG_THREAD_PRIORITY,
                    LOG_THREAD_TIME_SLICE, log_thread_stack,
                    LOG_THREAD_STACK_SIZE, mimi_log_thread_entry, NULL, NULL);
}

void mimi_log_set_output(log_output_func func)
{
    mimi_log_output_func = func;
}


/* -------------------------------------------------------------------------- */
/*  console                                                                   */
/* -------------------------------------------------------------------------- */
#if MIMI_CONSOLE

#define CONSOLE_THREAD_PRIORITY     5
#define CONSOLE_THREAD_TIME_SLICE   5
#define CONSOLE_THREAD_STACK_SIZE   2048
#define CONSOLE_TEMP_BUFFER_SIZE    256

mimi_tcb console_thread;
uint8_t console_thread_stack[CONSOLE_THREAD_STACK_SIZE] = {0};
uint8_t console_buffer[LOG_TEMP_BUFFER_SIZE];
console_input_func mimi_console_input_func = NULL;

extern const size_t __mimi_cmd_start;
extern const size_t __mimi_cmd_end;

int mimi_cmd_list(void)
{
    volatile const struct mimi_cmd_desc *desc;

    MIMI_LOG("<console> ");
    for (desc = (struct mimi_cmd_desc *)&__mimi_cmd_start;
         desc < (struct mimi_cmd_desc *)&__mimi_cmd_end; desc++) {
        MIMI_LOG("%s ", desc->cmd);
    }
    MIMI_LOG("\r\n");
    return MIMI_EOK;
}
MIMI_CMD_EXPORT(cmd, mimi_cmd_list);

int mimi_console_input(const char* cmd, int len)
{
    volatile const struct mimi_cmd_desc *desc;

    while (len > 0) {
        if (cmd[len-1] == '\r' || cmd[len-1] == '\n' || cmd[len-1] == ' ') {
            len--;
        } else {
            break;
        }
    }

    for (desc = (struct mimi_cmd_desc *)&__mimi_cmd_start;
         desc < (struct mimi_cmd_desc *)&__mimi_cmd_end; desc++) {
        if (strncmp((const char*)cmd, desc->cmd, len) == 0) {
            return desc->fn();
        }
    }
    MIMI_LOG_I("undefine command\r\n");
    return MIMI_EOK;
}

void mimi_console_thread_entry(void *param)
{
    while (1) {
        if (mimi_console_input_func != NULL) {
            uint16_t len = mimi_console_input_func(console_buffer, CONSOLE_TEMP_BUFFER_SIZE);
            mimi_console_input((const char*)console_buffer, len);
        } else {
            mimi_thread_delay(100);
        }
    }
}

void mimi_console_init(void)
{
    mimi_thread_init(&console_thread, "console", CONSOLE_THREAD_PRIORITY,
                    CONSOLE_THREAD_TIME_SLICE, console_thread_stack,
                    CONSOLE_THREAD_STACK_SIZE, mimi_console_thread_entry, NULL, NULL);

    mimi_console_input_func = mimi_board_get_console_input_func();
}

void mimi_console_set_input(console_input_func func)
{
    mimi_console_input_func = func;
}
#endif // MIMI_CONSOLE
