#include "common.h"
#include <stdint.h>

uintmax_t midgen_ceil_pow2(uintmax_t x)
{
    --x;
    for (size_t i = 1; i < (sizeof(x) * 8); i = i * 2) {
        x |= x >> i;
    }
    ++x;
    return x;
}
