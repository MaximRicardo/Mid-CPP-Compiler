#pragma once

// son im crine

#include "common.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "ints.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MIDGEN_DYNARRAY_DEFAULT_SIZE_TYPE
#define MIDGEN_DYNARRAY_DEFAULT_SIZE_TYPE long long
#endif

// a prefix can be something like struct, union or enum
#define MIDGEN_DYNARRAY_STRUCT_W_PREFIX(prefix, elem_type)                     \
    struct prefix##elem_type##MIDGEN_DYNARRAY {                                \
        prefix elem_type *arr;                                                 \
        MIDGEN_DYNARRAY_DEFAULT_SIZE_TYPE len;                                 \
        MIDGEN_DYNARRAY_DEFAULT_SIZE_TYPE cap;                                 \
    }

#define MIDGEN_DYNARRAY_STRUCT_NO_PREFIX(elem_type)                            \
    struct elem_type##MIDGEN_DYNARRAY {                                        \
        elem_type *arr;                                                        \
        MIDGEN_DYNARRAY_DEFAULT_SIZE_TYPE len;                                 \
        MIDGEN_DYNARRAY_DEFAULT_SIZE_TYPE cap;                                 \
    }

#define midgen_dynarray_struct_named(name, elem_type)                          \
    struct name {                                                              \
        elem_type *arr;                                                        \
        MIDGEN_DYNARRAY_DEFAULT_SIZE_TYPE len;                                 \
        MIDGEN_DYNARRAY_DEFAULT_SIZE_TYPE cap;                                 \
    }

// picks MIDGEN_DYNARRAY_NO_PREFIX if only a type is provided, else picks
// MIDGEN_DYNARRAY_W_PREFIX
#define midgen_dynarray_struct(...)                                            \
    MIDGEN_EXPAND(                                                             \
        MIDGEN_GET_MACRO_2(__VA_ARGS__, MIDGEN_DYNARRAY_STRUCT_W_PREFIX,       \
                           MIDGEN_DYNARRAY_STRUCT_NO_PREFIX)(__VA_ARGS__))

#define MIDGEN_DYNARRAY_W_PREFIX(prefix, elem_type)                            \
    struct prefix##elem_type##MIDGEN_DYNARRAY

#define MIDGEN_DYNARRAY_NO_PREFIX(elem_type) struct elem_type##MIDGEN_DYNARRAY

#define midgen_dynarray(...)                                                   \
    MIDGEN_EXPAND(MIDGEN_GET_MACRO_2(__VA_ARGS__, MIDGEN_DYNARRAY_W_PREFIX,    \
                                     MIDGEN_DYNARRAY_NO_PREFIX)(__VA_ARGS__))

#define MIDGEN_DYNARRAY_REALLOC(self)                                          \
    do {                                                                       \
        (self)->arr =                                                          \
            MIDGEN_REALLOC((self)->arr, (self)->cap * sizeof(*(self)->arr));   \
    } while (0)

#define MIDGEN_DYNARRAY_ALLOC_SPACE(self)                                      \
    do {                                                                       \
        self->cap = midgen_ceil_pow2(self->len);                               \
        MIDGEN_DYNARRAY_REALLOC(self);                                         \
    } while (0)

#define MIDGEN_DYNARRAY_IDX_VALID(self, idx)                                   \
    do {                                                                       \
        assert((idx) >= 0 && (idx) < (self)->len);                             \
    } while (0)

#define midgen_dyninit() {0}

#define MIDGEN_DYNDEINIT_NO_FREE(self_arg)                                     \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        free(self_super_specific_name______->arr);                             \
        self_super_specific_name______->arr = NULL;                            \
        self_super_specific_name______->len = 0;                               \
    } while (0)

#define MIDGEN_DYNDEINIT_W_FREE(self_arg, free_func)                           \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        for (typeof(self_super_specific_name______->len) i = 0;                \
             i < self_super_specific_name______->len; ++i)                     \
            free_func(&self_super_specific_name______->arr[i]);                \
        free(self_super_specific_name______->arr);                             \
        self_super_specific_name______->arr = NULL;                            \
        self_super_specific_name______->len = 0;                               \
    } while (0)

// void midgen_dyndeinit(midgen_dynarray<elem_type> *self,
//                    /* optional */ void free_func(elem_type *))
#define midgen_dyndeinit(...)                                                  \
    MIDGEN_EXPAND(MIDGEN_GET_MACRO_2(__VA_ARGS__, MIDGEN_DYNDEINIT_W_FREE,     \
                                     MIDGEN_DYNDEINIT_NO_FREE)(__VA_ARGS__))

// rounds new_cap up to a power of 2
// void midgen_dynreserve(midgen_dynarray<elem_type> *self, size_type new_cap)
#define midgen_dynreserve(self_arg, new_cap_arg)                               \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(new_cap_arg) new_cap_super_specific_name______ = new_cap_arg;   \
        self_super_specific_name______->cap =                                  \
            midgen_ceil_pow2(new_cap_super_specific_name______);               \
        MIDGEN_DYNARRAY_REALLOC(self_arg);                                     \
    } while (0)

// DOES NOT round new_cap
// void midgen_dynreserve(midgen_dynarray<elem_type> *self, size_type new_cap)
#define midgen_dynreserve_no_round(self_arg, new_cap_arg)                      \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(new_cap_arg) new_cap_super_specific_name______ = new_cap_arg;   \
        self_super_specific_name______->cap =                                  \
            new_cap_super_specific_name______;                                 \
        MIDGEN_DYNARRAY_REALLOC(self_arg);                                     \
    } while (0)

// void midgen_dynpush(midgen_dynarray<elem_type> *self, elem_type elem)
#define midgen_dynpush(self_arg, elem_arg)                                     \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(elem_arg) elem_super_specific_name______ = elem_arg;            \
        ++self_super_specific_name______->len;                                 \
        MIDGEN_DYNARRAY_ALLOC_SPACE(self_super_specific_name______);           \
        self_super_specific_name______                                         \
            ->arr[self_super_specific_name______->len - 1] =                   \
            elem_super_specific_name______;                                    \
    } while (0)

#define MIDGEN_DYNPOP_NO_FREE(self_arg)                                        \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        --self_super_specific_name______->len;                                 \
    } while (0)

#define MIDGEN_DYNPOP_W_FREE(self_arg, free_func)                              \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        free_func(&self_super_specific_name______                              \
                       ->arr[self_super_specific_name______->len - 1]);        \
        --self_super_specific_name______->len;                                 \
    } while (0)

// void midgen_dynpop(midgen_dynarray<elem_type> *self,
//                 /* optional */ void free_func(elem_type *))
#define midgen_dynpop(...)                                                     \
    MIDGEN_EXPAND(MIDGEN_GET_MACRO_2(__VA_ARGS__, MIDGEN_DYNPOP_W_FREE,        \
                                     MIDGEN_DYNPOP_NO_FREE)(__VA_ARGS__))

#define MIDGEN_DYNREMOVE_NO_FREE(self_arg, idx_arg)                            \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(idx_arg) idx_super_specific_name______ = idx_arg;               \
        MIDGEN_DYNARRAY_IDX_VALID(self_super_specific_name______,              \
                                  idx_super_specific_name______);              \
        for (typeof(self_super_specific_name______->len) i =                   \
                 idx_super_specific_name______;                                \
             i < self_super_specific_name______->len - 1; ++i) {               \
            self_super_specific_name______->arr[i] =                           \
                self_super_specific_name______->arr[i + 1];                    \
        }                                                                      \
        --self_super_specific_name______->len;                                 \
    } while (0)

#define MIDGEN_DYNREMOVE_W_FREE(self_arg, idx_arg, free_func)                  \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(idx_arg) idx_super_specific_name______ = idx_arg;               \
        MIDGEN_DYNARRAY_IDX_VALID(self_super_specific_name______,              \
                                  idx_super_specific_name______);              \
        free_func(&self_super_specific_name______                              \
                       ->arr[idx_super_specific_name______]);                  \
        for (typeof(self_super_specific_name______->len) i =                   \
                 idx_super_specific_name______;                                \
             i < self_super_specific_name______->len - 1; ++i) {               \
            self_super_specific_name______->arr[i] =                           \
                self_super_specific_name______->arr[i + 1];                    \
        }                                                                      \
        --self_super_specific_name______->len;                                 \
    } while (0)

// void midgen_dynremove(midgen_dynarray<elem_type> *self, size_type idx,
//                    /* optional */ void free_func(elem_type *))
#define midgen_dynremove(self, ...)                                            \
    MIDGEN_EXPAND(                                                             \
        MIDGEN_GET_MACRO_2(__VA_ARGS__, MIDGEN_DYNREMOVE_W_FREE,               \
                           MIDGEN_DYNREMOVE_NO_FREE)(self, __VA_ARGS__))

// void midgen_dyninsert(midgen_dynarray<elem_type> *self, size_type idx,
//                    elem_type elem)
#define midgen_dyninsert(self_arg, idx_arg, elem_arg)                          \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(idx_arg) idx_super_specific_name______ = idx_arg;               \
        typeof(elem_arg) elem_super_specific_name______ = elem_arg;            \
        MIDGEN_DYNARRAY_IDX_VALID(self_super_specific_name______,              \
                                  idx_super_specific_name______);              \
        ++self_super_specific_name______->len;                                 \
        MIDGEN_DYNARRAY_ALLOC_SPACE(self_super_specific_name______);           \
        for (typeof(self_super_specific_name______->len) i =                   \
                 self_super_specific_name______->len - 1;                      \
             i > idx_super_specific_name______; --i) {                         \
            self_super_specific_name______->arr[i] =                           \
                self_super_specific_name______->arr[i - 1];                    \
        }                                                                      \
        self_super_specific_name______->arr[idx_super_specific_name______] =   \
            elem_super_specific_name______;                                    \
    } while (0)

#ifdef __cplusplus
}
#endif
