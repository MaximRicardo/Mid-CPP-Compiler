#pragma once

#include "ints.h"
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// all identifiers are put in a symbol table to reduce memory load
// TODO: make this a hashmap for faster lookup during lexing
struct midsymb_Table {
    char **arr;
    mid_isize len, cap;
};

static inline void midsymb_deinit_symbol(char **arr)
{
    free(*arr);
    *arr = NULL;
}

#ifdef __cplusplus
}
#endif
