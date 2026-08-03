#pragma once

#include "common.h"
#include <stdlib.h>
#include <string.h>

#include "ints.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MIDGEN_BUMPALLOC_DEFAULT_SIZE_TYPE
#define MIDGEN_BUMPALLOC_DEFAULT_SIZE_TYPE long long
#endif

#ifndef MIDGEN_BUMPALLOC_DEFAULT_CHUNK_SIZE
// measured in elements, NOT bytes
#define MIDGEN_BUMPALLOC_DEFAULT_CHUNK_SIZE 128
#endif

#define midgen_bumpalloc_struct_named(name, elem_type)                         \
    struct name {                                                              \
        elem_type **chunks;                                                    \
        MIDGEN_BUMPALLOC_DEFAULT_SIZE_TYPE n_chunks;                           \
        MIDGEN_BUMPALLOC_DEFAULT_SIZE_TYPE n_elems;                            \
    }

#define MIDGEN_BUMPALLOC_CREATE_NEW_CHUNK(self)                                \
    do {                                                                       \
        ++(self)->n_chunks;                                                    \
        (self)->chunks = MIDGEN_REALLOC(                                       \
            (self)->chunks, (self)->n_chunks * sizeof(*(self)->chunks));       \
        (self)->chunks[(self)->n_chunks - 1] =                                 \
            MIDGEN_MALLOC(MIDGEN_BUMPALLOC_DEFAULT_CHUNK_SIZE *                \
                          sizeof(*(self)->chunks[(self)->n_chunks - 1]));      \
    } while (0)

#define midgen_bumpinit() {};

#define MIDGEN_BUMPDEINIT_NO_FREE(self_arg)                                    \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        for (typeof(self_super_specific_name______->n_chunks) i = 0;           \
             i < self_super_specific_name______->n_chunks; ++i) {              \
            free(self_super_specific_name______->chunks[i]);                   \
        }                                                                      \
        free(self_super_specific_name______->chunks);                          \
        self_super_specific_name______->chunks = NULL;                         \
    } while (0)

#define MIDGEN_BUMPDEINIT_W_FREE(self_arg, free_func)                          \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        for (typeof(self_super_specific_name______->n_chunks) i = 0;           \
             i < self_super_specific_name______->n_chunks; ++i) {              \
            if (i == self_super_specific_name______->n_chunks - 1) {           \
                /* on the last block we dont wanna free any extra elems so we  \
                 * iterate until n_elems - (n_block - 1) * block_size, giving  \
                 * the number of elements past the beginning of the last block \
                 */                                                            \
                for (long long j = 0;                                          \
                     j < self_super_specific_name______->n_elems -             \
                             (self_super_specific_name______->n_chunks - 1) *  \
                                 MIDGEN_BUMPALLOC_DEFAULT_CHUNK_SIZE;          \
                     ++j) {                                                    \
                    free_func(&self_super_specific_name______->chunks[i][j]);  \
                }                                                              \
            } else {                                                           \
                /* any blocks that are fully used can be fully iterated        \
                 * through                                                     \
                 */                                                            \
                for (long long j = 0; j < MIDGEN_BUMPALLOC_DEFAULT_CHUNK_SIZE; \
                     ++j) {                                                    \
                    free_func(&self_super_specific_name______->chunks[i][j]);  \
                }                                                              \
            }                                                                  \
            free(self_super_specific_name______->chunks[i]);                   \
        }                                                                      \
        free(self_super_specific_name______->chunks);                          \
        self_super_specific_name______->chunks = NULL;                         \
    } while (0)

// void midgen_bumpdeinit(midgen_bumpalloc<elem_type> *self,
//                    /* optional */ void free_func(elem_type *))
#define midgen_bumpdeinit(...)                                                 \
    MIDGEN_EXPAND(MIDGEN_GET_MACRO_2(__VA_ARGS__, MIDGEN_BUMPDEINIT_W_FREE,    \
                                     MIDGEN_BUMPDEINIT_NO_FREE)(__VA_ARGS__))

// void midgen_bumpmalloc(midgen_bumpalloc<elem_type> *self, elem_type
// **out_ptr)
#define midgen_bumpmalloc(self_arg, out_ptr)                                   \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(out_ptr) out_ptr_super_specific_name______ = out_ptr;           \
        MIDGEN_BUMPALLOC_DEFAULT_SIZE_TYPE                                     \
        chunk_off_super_specific_name______ =                                  \
            self_super_specific_name______->n_elems %                          \
            MIDGEN_BUMPALLOC_DEFAULT_CHUNK_SIZE;                               \
        if (chunk_off_super_specific_name______ != 0) {                        \
            *out_ptr_super_specific_name______ =                               \
                &self_super_specific_name______                                \
                     ->chunks[self_super_specific_name______->n_chunks - 1]    \
                             [chunk_off_super_specific_name______];            \
        } else {                                                               \
            MIDGEN_BUMPALLOC_CREATE_NEW_CHUNK(self_super_specific_name______); \
            *out_ptr_super_specific_name______ =                               \
                &self_super_specific_name______                                \
                     ->chunks[self_super_specific_name______->n_chunks - 1]    \
                             [0];                                              \
        }                                                                      \
        ++self_super_specific_name______->n_elems;                             \
    } while (0)

// initializes the allocated element to 0
// void midgen_bumpcalloc(midgen_bumpalloc<elem_type> *self, elem_type
// **out_ptr)
#define midgen_bumpcalloc(self_arg, out_ptr)                                   \
    do {                                                                       \
        typeof(self_arg) self_super_specific_name______ = self_arg;            \
        typeof(out_ptr) out_ptr_super_specific_name______ = out_ptr;           \
        MIDGEN_BUMPALLOC_DEFAULT_SIZE_TYPE                                     \
        chunk_off_super_specific_name______ =                                  \
            self_super_specific_name______->n_elems %                          \
            MIDGEN_BUMPALLOC_DEFAULT_CHUNK_SIZE;                               \
        if (chunk_off_super_specific_name______ != 0) {                        \
            *out_ptr_super_specific_name______ =                               \
                &self_super_specific_name______                                \
                     ->chunks[self_super_specific_name______->n_chunks - 1]    \
                             [chunk_off_super_specific_name______];            \
        } else {                                                               \
            MIDGEN_BUMPALLOC_CREATE_NEW_CHUNK(self_super_specific_name______); \
            *out_ptr_super_specific_name______ =                               \
                &self_super_specific_name______                                \
                     ->chunks[self_super_specific_name______->n_chunks - 1]    \
                             [0];                                              \
        }                                                                      \
        ++self_super_specific_name______->n_elems;                             \
        memset(*out_ptr_super_specific_name______, 0,                          \
               sizeof(**out_ptr_super_specific_name______));                   \
    } while (0)

#ifdef __cplusplus
}
#endif
