#pragma once

#include "ints.h"

void Mid_qsort(void *v, mid_isize nmemb, mid_isize size,
               int comp(const void *, const void *, const void *),
               const void *info);
