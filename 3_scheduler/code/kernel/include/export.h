#ifndef __MIMI_EXPORT__
#define __MIMI_EXPORT__

#include "mimi.h"

/* -------------------------------------------------------------------------- */
/*  auto init                                                                 */
/* -------------------------------------------------------------------------- */
typedef int (*init_fn_t)(void);

#define INIT_EXPORT(fn, level)                                \
    mimi_used const init_fn_t __mimi_init_##fn mimi_section(".mimi_init_fn." level) = fn

#define MIMI_INIT_DEVICE_EXPORT(fn)          INIT_EXPORT(fn, "0")
#define MIMI_INIT_APP_EXPORT(fn)             INIT_EXPORT(fn, "1")

#endif // __MIMI_EXPORT__
