#pragma once

#include "ints.h"
#include <stddef.h>
#include <stdlib.h>

// all identifiers are put in a symbol table to reduce memory load
// TODO: make this a hashmap for faster lookup during lexing
struct MidSymbol_Table {
    char **arr;
    mid_isize len, cap;
};

static inline void MidSymbol_deinit_symbol(char **arr)
{
    free(*arr);
    *arr = NULL;
}
