#pragma once

#include "generics/dynarray.h"

#ifdef __cplusplus
extern "C" {
#endif

midgen_dynarray_struct_named(mid_BoolVec, bool);
midgen_dynarray_struct_named(mid_ConstStringVec, const char *);

#ifdef __cplusplus
}
#endif
