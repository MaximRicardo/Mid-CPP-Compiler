#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static inline void *Mid_malloc(size_t n)
{
    void *ret = malloc(n);
    if (!ret) {
        perror("Mid_malloc");
        abort();
    }
    return ret;
}

static inline void *Mid_calloc(size_t nmemb, size_t size)
{
    void *ret = calloc(nmemb, size);
    if (!ret) {
        perror("Mid_calloc");
        abort();
    }
    return ret;
}

static inline void *Mid_realloc(void *p, size_t n)
{
    void *ret = realloc(p, n);
    if (!ret) {
        perror("Mid_realloc");
        abort();
    }
    return ret;
}
