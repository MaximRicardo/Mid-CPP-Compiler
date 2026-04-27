#pragma once

// son im crine

#include "common.h"
#include <assert.h>
#include <stdlib.h>

#ifndef GEN_DYNARRAY_DEFAULT_SIZE_TYPE
#define GEN_DYNARRAY_DEFAULT_SIZE_TYPE long long
#endif

// a prefix can be something like struct, union or enum
#define GEN_DYNARRAY_STRUCT_W_PREFIX(prefix, elem_type)                        \
    struct prefix##elem_type##GEN_DYNARRAY {                                   \
        prefix elem_type *arr;                                                 \
        GEN_DYNARRAY_DEFAULT_SIZE_TYPE len;                                    \
        GEN_DYNARRAY_DEFAULT_SIZE_TYPE cap;                                    \
    }

#define GEN_DYNARRAY_STRUCT_NO_PREFIX(elem_type)                               \
    struct elem_type##GEN_DYNARRAY {                                           \
        elem_type *arr;                                                        \
        GEN_DYNARRAY_DEFAULT_SIZE_TYPE len;                                    \
        GEN_DYNARRAY_DEFAULT_SIZE_TYPE cap;                                    \
    }

#define gen_dynarray_struct_named(name, elem_type)                             \
    struct name {                                                              \
        elem_type *arr;                                                        \
        GEN_DYNARRAY_DEFAULT_SIZE_TYPE len;                                    \
        GEN_DYNARRAY_DEFAULT_SIZE_TYPE cap;                                    \
    }

// picks GEN_DYNARRAY_NO_PREFIX if only a type is provided, else picks
// GEN_DYNARRAY_W_PREFIX
#define gen_dynarray_struct(...)                                               \
    GEN_EXPAND(GEN_GET_MACRO_2(__VA_ARGS__, GEN_DYNARRAY_STRUCT_W_PREFIX,      \
                               GEN_DYNARRAY_STRUCT_NO_PREFIX)(__VA_ARGS__))

#define GEN_DYNARRAY_W_PREFIX(prefix, elem_type)                               \
    struct prefix##elem_type##GEN_DYNARRAY

#define GEN_DYNARRAY_NO_PREFIX(elem_type) struct elem_type##GEN_DYNARRAY

#define gen_dynarray(...)                                                      \
    GEN_EXPAND(GEN_GET_MACRO_2(__VA_ARGS__, GEN_DYNARRAY_W_PREFIX,             \
                               GEN_DYNARRAY_NO_PREFIX)(__VA_ARGS__))

#define GEN_DYNARRAY_REALLOC(self)                                             \
    do {                                                                       \
        (self)->arr =                                                          \
            realloc((self)->arr, (self)->cap * sizeof(*(self)->arr));          \
    } while (0)

#define GEN_DYNARRAY_ALLOC_SPACE(self)                                         \
    do {                                                                       \
        while ((self)->len >= (self)->cap) {                                   \
            (self)->cap = (self)->cap > 0 ? (self)->cap * 2 : 1;               \
            GEN_DYNARRAY_REALLOC(self);                                        \
        }                                                                      \
    } while (0)

#define GEN_DYNARRAY_IDX_VALID(self, idx)                                      \
    do {                                                                       \
        assert((self) >= 0 && (idx) < (self)->len);                            \
    } while (0)

#define gen_dyninit() {0}

#define GEN_DYNDEINIT_NO_FREE(self_arg)                                        \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        free(self_super_specific_name______->arr);                             \
        self_super_specific_name______->arr = NULL;                            \
    } while (0)

#define GEN_DYNDEINIT_W_FREE(self_arg, free_func)                              \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        for (typeof(self_super_specific_name______->len) i = 0;                \
             i < self_super_specific_name______->len; ++i)                     \
            free_func(&self_super_specific_name______->arr[i]);                \
        free(self_super_specific_name______->arr);                             \
        self_super_specific_name______->arr = NULL;                            \
    } while (0)

// void gen_dyndeinit(gen_dynarray(elem_type) *self,
//                    /* optional */ void free_func(elem_type *))
#define gen_dyndeinit(...)                                                     \
    GEN_EXPAND(GEN_GET_MACRO_2(__VA_ARGS__, GEN_DYNDEINIT_W_FREE,              \
                               GEN_DYNDEINIT_NO_FREE)(__VA_ARGS__))

// void gen_dynpush(gen_dynarray(elem_type) *self, elem_type elem)
#define gen_dynpush(self_arg, elem_arg)                                        \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(elem_arg) elem_super_specific_name______ = elem_arg;            \
        ++self_super_specific_name______->len;                                 \
        GEN_DYNARRAY_ALLOC_SPACE(self_super_specific_name______);              \
        self_super_specific_name______                                         \
            ->arr[self_super_specific_name______->len - 1] =                   \
            elem_super_specific_name______;                                    \
    } while (0)

#define GEN_DYNPOP_NO_FREE(self_arg)                                           \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        --self_super_specific_name______->len;                                 \
    } while (0)

#define GEN_DYNPOP_W_FREE(self_arg, free_func)                                 \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        free_func(&self_super_specific_name______                              \
                       ->arr[self_super_specific_name______->len - 1]);        \
        --self_super_specific_name______->len;                                 \
    } while (0)

// void gen_dynpop(gen_dynarray(elem_type) *self,
//                 /* optional */ void free_func(elem_type *))
#define gen_dynpop(...)                                                        \
    GEN_EXPAND(GEN_GET_MACRO_2(__VA_ARGS__, GEN_DYNPOP_W_FREE,                 \
                               GEN_DYNPOP_NO_FREE)(__VA_ARGS__))

#define GEN_DYNREMOVE_NO_FREE(self_arg, idx_arg)                               \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(idx_arg) idx_super_specific_name______ = idx_arg;               \
        GEN_DYNARRAY_IDX_VALID(self_super_specific_name______,                 \
                               idx_super_specific_name______);                 \
        for (typeof(self_super_specific_name______->len) i =                   \
                 idx_super_specific_name______;                                \
             i < self_super_specific_name______->len - 1; ++i) {               \
            self_super_specific_name______->arr[i] =                           \
                self_super_specific_name______->arr[i + 1];                    \
        }                                                                      \
        --self_super_specific_name______->len;                                 \
    } while (0)

#define GEN_DYNREMOVE_W_FREE(self_arg, idx_arg, free_func)                     \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(idx_arg) idx_super_specific_name______ = idx_arg;               \
        GEN_DYNARRAY_IDX_VALID(self_super_specific_name______,                 \
                               idx_super_specific_name______);                 \
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

// void gen_dynremove(gen_dynarray(elem_type) *self, size_type idx,
//                    /* optional */ void free_func(elem_type *))
#define gen_dynremove(self, ...)                                               \
    GEN_EXPAND(GEN_GET_MACRO_2(__VA_ARGS__, GEN_DYNREMOVE_W_FREE,              \
                               GEN_DYNREMOVE_NO_FREE)(self, __VA_ARGS__))

// void gen_dyninsert(gen_dynarray(elem_type) *self, size_type idx,
//                    elem_type elem)
#define gen_dyninsert(self_arg, idx_arg, elem_arg)                             \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(idx_arg) idx_super_specific_name______ = idx_arg;               \
        typeof(elem_arg) elem_super_specific_name______ = elem_arg;            \
        GEN_DYNARRAY_IDX_VALID(self_super_specific_name______,                 \
                               idx_super_specific_name______);                 \
        ++self_super_specific_name______->len;                                 \
        for (typeof(self_super_specific_name______->len) i =                   \
                 self_super_specific_name______->len - 1;                      \
             i > idx_super_specific_name______; --i) {                         \
            self_super_specific_name______->arr[i] =                           \
                self_super_specific_name______->arr[i - 1];                    \
        }                                                                      \
        self_super_specific_name______->arr[idx_super_specific_name______] =   \
            elem_super_specific_name______;                                    \
    } while (0)
