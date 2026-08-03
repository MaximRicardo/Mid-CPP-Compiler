#pragma once

#include "generics/bumpalloc.h"

midgen_bumpalloc_struct_named(midllvm_ScopeBump, struct midllvm_Scope);

struct midllvm_Allocators {
    struct midllvm_ScopeBump scope;
};

void midllvm_Allocators_deinit(struct midllvm_Allocators *allocs);
