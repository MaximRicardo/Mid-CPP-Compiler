#pragma once

#include "ints.h"
#include <stddef.h>
#include <stdlib.h>

// all identifiers are put in a symbol table to reduce memory load
struct SymbolTable {
    char **arr;
    isize_t len, cap;
};

static inline void Symbol_deinit_symbol(char **arr)
{
    free(*arr);
    *arr = NULL;
}
