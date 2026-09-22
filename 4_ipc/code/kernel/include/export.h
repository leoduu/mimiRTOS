#ifndef __MIMI_EXPORT__
#define __MIMI_EXPORT__

#include "mimi.h"

/* -------------------------------------------------------------------------- */
/*  auto init                                                                 */
/* -------------------------------------------------------------------------- */
typedef int (*init_fn_t)(void);

#define INIT_EXPORT(fn, level)                                \
    mimi_used const init_fn_t __mimi_init_##fn mimi_section(".mimi_init_fn." level) = fn

#define MIMI_INIT_BOARD_EXPORT(fn)           INIT_EXPORT(fn, "0")
#define MIMI_INIT_DEVICE_EXPORT(fn)          INIT_EXPORT(fn, "1")
#define MIMI_INIT_APP_EXPORT(fn)             INIT_EXPORT(fn, "2")


/* -------------------------------------------------------------------------- */
/*  command                                                                   */
/* -------------------------------------------------------------------------- */
typedef int (*cmd_fn_t)(void);

struct mimi_cmd_desc {
    const char* cmd;
    const cmd_fn_t fn;
};

#define MIMI_CMD_EXPORT(cmd, fn)                                \
    const char __mimi_cmd_##cmd[] = #cmd;       \
    mimi_used const struct mimi_cmd_desc __mimi_cmd_desc_##fn mimi_section(".mimi_cmd_fn") = \
    { __mimi_cmd_##cmd, fn};

#endif // __MIMI_EXPORT__
