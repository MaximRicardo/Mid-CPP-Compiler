#pragma once

#include "generics/dynarray.h"
#include <llvm-c-20/llvm-c/Types.h>

struct MidLLVM_Ident {
    char *name;
    LLVMTypeRef type;
    LLVMValueRef val; // for variables this will be a ptr to the value
};
MidGen_dynarray_struct_named(MidLLVM_IdentVec, struct MidLLVM_Ident);

void MidLLVM_Ident_deinit(struct MidLLVM_Ident *self);
