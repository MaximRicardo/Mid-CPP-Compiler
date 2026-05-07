#pragma once

#include "ints.h"

void better_qsort(void *v, isize_t nmemb, isize_t size,
                  int comp(const void *, const void *, const void *),
                  const void *info);
