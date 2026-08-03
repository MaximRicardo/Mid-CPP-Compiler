#pragma once

#include "ints.h"

#ifdef __cplusplus
extern "C" {
#endif

void mid_qsort(void *v, mid_isize nmemb, mid_isize size,
               int comp(const void *, const void *, const void *),
               const void *info);

#ifdef __cplusplus
}
#endif
