#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

#include <stdio.h>

/* Unity native test 输出到 stdout */
#define UNITY_OUTPUT_CHAR(c)  putchar(c)
#define UNITY_OUTPUT_FLUSH()  fflush(stdout)

#endif
