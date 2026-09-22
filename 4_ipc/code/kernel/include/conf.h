#ifndef __MIMI_CONFIG__
#define __MIMI_CONFIG__

/* -------------------------------------------------------------------------- */
/*  os config                                                                 */
/* -------------------------------------------------------------------------- */
#define OS_TICK_PER_SECOND          1000
#define THREAD_PRIORITY_MAX         32
#define THREAD_TIME_SLICE_DEFAULT   10
#define THREAD_NAME_LEN             16

#define IDLE_THREAD_PRIORITY        (THREAD_PRIORITY_MAX - 1)
#define IDLE_THREAD_TICK_SLICE      5
#define IDLE_STACK_SIZE             2048

/* -------------------------------------------------------------------------- */
/*  log                                                                       */
/* -------------------------------------------------------------------------- */
#define MIMI_LOG_LEVEL_ERROR        0
#define MIMI_LOG_LEVEL_WARNING      1
#define MIMI_LOG_LEVEL_INFO         2
#define MIMI_LOG_LEVEL_DEBUG        3
#define MIMI_LOG_LEVEL_VERBOSE      4
#define MIMI_LOG_LEVEL              MIMI_LOG_LEVEL_DEBUG
#define MIMI_LOG_BUFFER_SIZE        (1024 * 8)

/* -------------------------------------------------------------------------- */
/*  service                                                                   */
/* -------------------------------------------------------------------------- */
#define MIMI_DEBUG_ASSERT           1

/* -------------------------------------------------------------------------- */
/*  component                                                                 */
/* -------------------------------------------------------------------------- */
#define MIMI_CONSOLE                1

#endif  // __MIMI_CONFIG__
