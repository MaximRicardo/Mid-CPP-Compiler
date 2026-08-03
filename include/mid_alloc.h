#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void *mid_malloc(size_t n)
{
    void *ret = malloc(n);
    if (!ret) {
        perror("mid_malloc");
        abort();
    }
    return ret;
}

static inline void *mid_calloc(size_t nmemb, size_t size)
{
    void *ret = calloc(nmemb, size);
    if (!ret) {
        perror("mid_calloc");
        abort();
    }
    return ret;
}

static inline void *mid_realloc(void *p, size_t n)
{
    void *ret = realloc(p, n);
    if (!ret) {
        perror("mid_realloc");
        abort();
    }
    return ret;
}

#ifdef __cplusplus
}
#endif
