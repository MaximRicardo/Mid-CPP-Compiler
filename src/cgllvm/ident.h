#pragma once

#include "generics/dynarray.h"
#include <llvm-c-20/llvm-c/Types.h>

struct CGLLVM_Ident {
    char *name;
    LLVMTypeRef type;
    LLVMValueRef val; // for variables this will be a ptr to the value
};
gen_dynarray_struct_named(CGLLVM_IdentVec, struct CGLLVM_Ident);

void CGLLVM_Ident_deinit(struct CGLLVM_Ident *self);
