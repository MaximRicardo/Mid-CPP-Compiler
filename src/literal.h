#pragma once

#include "generics/dynarray.h"
#include "ints.h"

union Literal_Value {
    i64 sint;
    u64 uint;
    float flt;
    double dbl;
    long double l_dbl;
};

gen_dynarray_struct_named(Literal_ValueVec, union Literal_Value);
