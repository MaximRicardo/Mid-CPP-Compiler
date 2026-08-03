#pragma once

#include "mid_alloc.h"
#include <stdint.h>

static inline uintmax_t midgen_ceil_pow2(uintmax_t x)
{
    --x;
    for (size_t i = 1; i < (sizeof(x) * 8); i = i * 2) {
        x |= x >> i;
    }
    ++x;
    return x;
}

#define MIDGEN_EXPAND(x) x
#define MIDGEN_GET_MACRO_2(_1, _2, name, ...) name

#ifndef MIDGEN_MALLOC
#define MIDGEN_MALLOC(n) mid_malloc(n)
#endif

#ifndef MIDGEN_CALLOC
#define MIDGEN_CALLOC(nmemb, size) mid_calloc(nmemb, size)
#endif

#ifndef MIDGEN_REALLOC
#define MIDGEN_REALLOC(p, n) mid_realloc(p, n)
#endif
