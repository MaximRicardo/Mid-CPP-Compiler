#pragma once

#include "ints.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MID_CRASH(msg)                                                         \
    do {                                                                       \
        printf("crashed at %s, line %d: '%s'\n", __FILE__, __LINE__, msg);     \
        abort();                                                               \
    } while (0)

#define MID_SWAP(x, y)                                                         \
    do {                                                                       \
        typeof(x) tmp_super_specific_name______ = x;                           \
        x = y;                                                                 \
        y = tmp_super_specific_name______;                                     \
    } while (0)

#define MID_ARRLEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MID_SARRLEN(arr) ((mid_isize)(sizeof(arr) / sizeof((arr)[0])))

#define MID_MAX(x, y) ((x) < (y) ? (y) : (x))
#define MID_MIN(x, y) ((x) > (y) ? (y) : (x))

#ifdef __cplusplus
}
#endif
