#pragma once

#include "generics/dynarray.h"
#include "ints.h"
#include <llvm-c-20/llvm-c/Types.h>

struct CGLLVM_Ident {
    char *name;
    LLVMTypeRef type;
    union {
        LLVMValueRef val; // for variables this will be a ptr to the value
        isize_t param_idx;
    };
    bool is_param;
};
gen_dynarray_struct_named(CGLLVM_IdentVec, struct CGLLVM_Ident);

void CGLLVM_Ident_deinit(struct CGLLVM_Ident *self);
