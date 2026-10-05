
#include "console.h"
#include <string.h>
#include "conf.h"
#include "export.h"
#include "thread.h"
#include "log.h"

#if MIMI_CONSOLE

#define CONSOLE_THREAD_PRIORITY     (THREAD_PRIORITY_MAX - 2)
#define CONSOLE_THREAD_TIME_SLICE   THREAD_TIME_SLICE_DEFAULT
#define CONSOLE_THREAD_STACK_SIZE   2048
#define CONSOLE_TEMP_BUFFER_SIZE    256

mimi_tcb console_thread;
uint8_t console_thread_stack[CONSOLE_THREAD_STACK_SIZE] = {0};
uint8_t console_buffer[CONSOLE_TEMP_BUFFER_SIZE];
console_input_func console_input_func_ptr = NULL;

extern const size_t __mimi_cmd_start;
extern const size_t __mimi_cmd_end;

#define mimi_cmd_for_each                                                   \
    volatile const struct mimi_cmd_desc *desc;                              \
    for (desc = (struct mimi_cmd_desc *)&__mimi_cmd_start;                  \
         desc < (struct mimi_cmd_desc *)&__mimi_cmd_end; desc++)            \

int mimi_cmd_list(void)
{
    MIMI_LOG("<console> cmd list: ");
    int i = 0;
    mimi_cmd_for_each {
        if (++i == 5) {
            i = 0;
            MIMI_LOG("\r\n                    ");
        }
        MIMI_LOG("%s  ", desc->cmd);
    }
    MIMI_LOG("\r\n");
    return MIMI_EOK;
}
MIMI_CMD_EXPORT(cmd, mimi_cmd_list);

int mimi_console_input(const char* cmd, uint16_t len)
{
    while (len > 0) {
        if (cmd[len-1] == '\r' || cmd[len-1] == '\n' || cmd[len-1] == ' ') {
            len--;
        } else {
            break;
        }
    }

    mimi_cmd_for_each {
        if (len == strlen(desc->cmd) &&
            strncmp((const char*)cmd, desc->cmd, len) == 0) {
            return desc->fn();
        }
    }
    MIMI_LOG_I("undefined command\r\n");
    return MIMI_EOK;
}

void mimi_console_thread_entry(void *param)
{
    mimi_cmd_list();

    while (1) {
        if (console_input_func_ptr != NULL) {
            uint16_t len = console_input_func_ptr(console_buffer, CONSOLE_TEMP_BUFFER_SIZE);
            mimi_console_input((const char*)console_buffer, len);
        } else {
            mimi_thread_delay(100);
        }
    }
}

int mimi_console_init(void)
{
    mimi_thread_init(&console_thread, "console", CONSOLE_THREAD_PRIORITY,
                    CONSOLE_THREAD_TIME_SLICE, console_thread_stack,
                    CONSOLE_THREAD_STACK_SIZE, mimi_console_thread_entry, NULL, NULL);

    console_input_func_ptr = mimi_console_input_func();
    return MIMI_EOK;
}
MIMI_INIT_SERVICE_EXPORT(mimi_console_init);

#endif // MIMI_CONSOLE
