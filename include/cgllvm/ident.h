#pragma once

#include "generics/dynarray.h"
#include <llvm-c-20/llvm-c/Types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct midllvm_Ident {
    char *name;
    LLVMTypeRef type;
    LLVMValueRef val; // for variables this will be a ptr to the value
};
midgen_dynarray_struct_named(midllvm_IdentVec, struct midllvm_Ident);

void midllvm_Ident_deinit(struct midllvm_Ident *self);

#ifdef __cplusplus
}
#endif
