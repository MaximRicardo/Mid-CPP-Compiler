#pragma once

#include "mid_alloc.h"
#include <stdint.h>

uintmax_t midgen_ceil_pow2(uintmax_t x);

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
