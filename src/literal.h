#pragma once

#include "generics/dynarray.h"
#include "ints.h"

union Literal_Value {
    i64 sint;
    u64 uint;
    long double flt;
    void *ptr;
};

gen_dynarray_struct_named(Literal_ValueVec, union Literal_Value);
