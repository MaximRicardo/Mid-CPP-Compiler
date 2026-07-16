#pragma once

#include "mid_alloc.h"
#include <stdint.h>

uintmax_t ceil_pow2(uintmax_t x);

#define GEN_EXPAND(x) x
#define GEN_GET_MACRO_2(_1, _2, name, ...) name

#ifndef GEN_MALLOC
#define GEN_MALLOC(n) mid_malloc(n)
#endif

#ifndef GEN_CALLOC
#define GEN_CALLOC(nmemb, size) mid_calloc(nmemb, size)
#endif

#ifndef GEN_REALLOC
#define GEN_REALLOC(p, n) mid_realloc(p, n)
#endif
