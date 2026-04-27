#pragma once

#include "ints.h"

// all identifiers are put in a symbol table to reduce memory load
struct SymbolTable {
    char **arr;
    isize_t len, cap;
};
