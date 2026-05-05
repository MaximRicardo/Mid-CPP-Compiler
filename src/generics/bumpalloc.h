#pragma once

#include "common.h"
#include <stdlib.h>
#include <string.h>

#include "ints.h"

#ifndef GEN_BUMPALLOC_DEFAULT_SIZE_TYPE
#define GEN_BUMPALLOC_DEFAULT_SIZE_TYPE long long
#endif

#ifndef GEN_BUMPALLOC_DEFAULT_CHUNK_SIZE
// measured in elements, NOT bytes
#define GEN_BUMPALLOC_DEFAULT_CHUNK_SIZE 128
#endif

#define gen_bumpalloc_struct_named(name, elem_type)                            \
    struct name {                                                              \
        elem_type **chunks;                                                    \
        GEN_BUMPALLOC_DEFAULT_SIZE_TYPE n_chunks;                              \
        GEN_BUMPALLOC_DEFAULT_SIZE_TYPE n_elems;                               \
    }

#define GEN_BUMPALLOC_CREATE_NEW_CHUNK(self)                                   \
    do {                                                                       \
        ++(self)->n_chunks;                                                    \
        (self)->chunks = realloc((self)->chunks,                               \
                                 (self)->n_chunks * sizeof(*(self)->chunks));  \
        (self)->chunks[(self)->n_chunks - 1] =                                 \
            malloc(GEN_BUMPALLOC_DEFAULT_CHUNK_SIZE *                          \
                   sizeof(*(self)->chunks[(self)->n_chunks - 1]));             \
    } while (0)

#define gen_bumpinit() {};

#define GEN_BUMPDEINIT_NO_FREE(self_arg)                                       \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        for (typeof(self_super_specific_name______->n_chunks) i = 0;           \
             i < self_super_specific_name______->n_chunks; ++i) {              \
            free(self_super_specific_name______->chunks[i]);                   \
        }                                                                      \
        free(self_super_specific_name______->chunks);                          \
        self_super_specific_name______->chunks = NULL;                         \
    } while (0)

#define GEN_BUMPDEINIT_W_FREE(self_arg, free_func)                             \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        for (typeof(self_super_specific_name______->n_chunks) i = 0;           \
             i < self_super_specific_name______->n_chunks; ++i) {              \
            for (long long j = 0;                                              \
                 j < self_super_specific_name______->n_elems -                 \
                         (self_super_specific_name______->n_chunks - 1) *      \
                             GEN_BUMPALLOC_DEFAULT_CHUNK_SIZE;                 \
                 ++j) {                                                        \
                free_func(&self_super_specific_name______->chunks[i][j]);      \
            }                                                                  \
            free(self_super_specific_name______->chunks[i]);                   \
        }                                                                      \
        free(self_super_specific_name______->chunks);                          \
        self_super_specific_name______->chunks = NULL;                         \
    } while (0)

// void gen_bumpdeinit(gen_bumpalloc<elem_type> *self,
//                    /* optional */ void free_func(elem_type *))
#define gen_bumpdeinit(...)                                                    \
    GEN_EXPAND(GEN_GET_MACRO_2(__VA_ARGS__, GEN_BUMPDEINIT_W_FREE,             \
                               GEN_BUMPDEINIT_NO_FREE)(__VA_ARGS__))

// void gen_bumpmalloc(gen_bumpalloc<elem_type> *self, elem_type **out_ptr)
#define gen_bumpmalloc(self_arg, out_ptr)                                      \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(out_ptr) out_ptr_super_specific_name______ = out_ptr;           \
        GEN_BUMPALLOC_DEFAULT_SIZE_TYPE chunk_off_super_specific_name______ =  \
            self_super_specific_name______->n_elems %                          \
            GEN_BUMPALLOC_DEFAULT_CHUNK_SIZE;                                  \
        if (chunk_off_super_specific_name______ != 0) {                        \
            *out_ptr_super_specific_name______ =                               \
                &self_super_specific_name______                                \
                     ->chunks[self_super_specific_name______->n_chunks - 1]    \
                             [chunk_off_super_specific_name______];            \
        } else {                                                               \
            GEN_BUMPALLOC_CREATE_NEW_CHUNK(self_super_specific_name______);    \
            *out_ptr_super_specific_name______ =                               \
                &self_super_specific_name______                                \
                     ->chunks[self_super_specific_name______->n_chunks - 1]    \
                             [0];                                              \
        }                                                                      \
        ++self_super_specific_name______->n_elems;                             \
    } while (0)

// initializes the allocated element to 0
// void gen_bumpcalloc(gen_bumpalloc<elem_type> *self, elem_type **out_ptr)
#define gen_bumpcalloc(self_arg, out_ptr)                                      \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(out_ptr) out_ptr_super_specific_name______ = out_ptr;           \
        GEN_BUMPALLOC_DEFAULT_SIZE_TYPE chunk_off_super_specific_name______ =  \
            self_super_specific_name______->n_elems %                          \
            GEN_BUMPALLOC_DEFAULT_CHUNK_SIZE;                                  \
        if (chunk_off_super_specific_name______ != 0) {                        \
            *out_ptr_super_specific_name______ =                               \
                &self_super_specific_name______                                \
                     ->chunks[self_super_specific_name______->n_chunks - 1]    \
                             [chunk_off_super_specific_name______];            \
        } else {                                                               \
            GEN_BUMPALLOC_CREATE_NEW_CHUNK(self_super_specific_name______);    \
            *out_ptr_super_specific_name______ =                               \
                &self_super_specific_name______                                \
                     ->chunks[self_super_specific_name______->n_chunks - 1]    \
                             [0];                                              \
        }                                                                      \
        ++self_super_specific_name______->n_elems;                             \
        memset(*out_ptr_super_specific_name______, 0,                          \
               sizeof(**out_ptr_super_specific_name______));                   \
    } while (0)
