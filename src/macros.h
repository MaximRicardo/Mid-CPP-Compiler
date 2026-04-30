#pragma once

#include <stdio.h>

#define CRASH(msg)                                                             \
    do {                                                                       \
        printf("crashed at %s, line %d: '%s'\n", __FILE__, __LINE__, msg);     \
        abort();                                                               \
    } while (0)

#define SWAP(x, y)                                                             \
    do {                                                                       \
        typeof(x) tmp_super_specific_name______ = x;                           \
        x = y;                                                                 \
        y = tmp_super_specific_name______;                                     \
    } while (0)

#define ARRLEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MIN(x, y) ((x) > (y) ? (y) : (x))
