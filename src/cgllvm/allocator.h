#pragma once

#include "generics/bumpalloc.h"

gen_bumpalloc_struct_named(CGLLVM_ScopeBump, struct CGLLVM_Scope);

struct CGLLVM_Allocators {
    struct CGLLVM_ScopeBump scope;
};

void CGLLVM_Allocators_deinit(struct CGLLVM_Allocators *allocs);
