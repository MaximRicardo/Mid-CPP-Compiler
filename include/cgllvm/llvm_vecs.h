#pragma once

#include "generics/dynarray.h"
#include <llvm-c-20/llvm-c/Types.h>

#ifdef __cplusplus
extern "C" {
#endif

midgen_dynarray_struct_named(midllvm_TypeRefVec, LLVMTypeRef);

#ifdef __cplusplus
}
#endif
